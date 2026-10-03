# FX3U/FX3UC 래더 실행 현황

2026-09-30 기준. 사용자 제공 `LADDER_REWRITE_PLAN.md`는 원래 계획이다.
현재 코드의 실행 범위와 남은 작업을 기록한다. 공식 응용 명령의 등록 현황은
[`FX_INSTRUCTION_COVERAGE.md`](FX_INSTRUCTION_COVERAGE.md)에 있다.
명령 등록은 모든 하드웨어 조합의 검증 완료를 뜻하지 않는다.

## 실행 구조

그리드 또는 LD/GX CSV → `ExecutionProgram`(typed operand 및 조건 DAG) →
`CompiledPLCExecutor::LoadProgram` → 기존 PLC 스캔/I/O 경로를 사용한다.
generated C++ 역파싱, 별도 VM, JIT는 사용하지 않는다. 프로그램 설치 때
명령별 상태와 관측 버퍼를 할당하고 스캔 중 재할당을 피한다.

F4는 그리드와 오퍼랜드를 검증한 후 프로그램을 교체한다. 오류가 있으면 기존
실행 프로그램을 유지하고 원인을 보고한다. 출력·접점·회로의 통전 상태를
스캔별로 관측하여 편집기 모니터와 연결한다.

## 그리드와 GX Works2 방식

기본 12열을 유지하고 임의의 `+ Column` 또는 명령 길이에 따른 열 폭 자동
확장을 사용하지 않는다. F9의 `K번호`는 GX Works2의 wrapping 결선 표식이다.
F7 코일 생성 후 선택은 코일로 이동한다. 화살표는 셀 그리드 안에서 이동하며
END 선택은 전체 레일 폭과 높이를 사용한다. END에서 코일을 만들면 새 행의
코일을 선택한다.

GX CSV는 탭 필드, 따옴표, UTF-8 BOM 및 UTF-16LE 입력, 연속 오퍼랜드 행,
포인터와 분기, 문자/부동소수점 값을 다룬다. DTBL의 17스텝 등 알려진
스텝 수를 내보낼 때 사용한다. 실제 GX Works2의 다양한 프로젝트를 통한
실기 가져오기·내보내기 검증은 아직 필요하다. 2026-10-01 기본 회로의
실제 GX Works2 가져오기에서 UTF-16LE BOM/CRLF 필요성을 확인하고 기본
출력을 수정했다. 범위와 미검증 항목은
[`GX_WORKS2_IMPORT_TEST.md`](GX_WORKS2_IMPORT_TEST.md)에 기록했다.

## CPU 메모리와 명령 실행

X/Y 256점, M/S, 특수 M/D, T 512점, C 256점, D/R/V/Z, 확장 파일 ER,
U\\G 버퍼 메모리, `D.b`, `KnX/Y/M/S` 및 V/Z 인덱스를 지원한다.
T/C 단위와 16/32비트 카운터 구분, 고속 C235..255의 물리 입력 펄스를
모델링한다. 인덱스에 의한 접근은 스캔에서 검사한다.

기본·데이터·비교·시프트·문자열·부동소수점·표 연산, P/I 흐름·서브루틴·
인터럽트·루프, 타이머/카운터, MC/MCR, PLS/PLF를 실행한다. 가상 아날로그·
BFM, 물리 입력 이미지와 출력 래치, 고속 비교, 조작 패널, 드럼 시퀀서,
R/ER, 위치 제어와 PID의 별도 테스트가 있다.

가상 축 4개는 위치 D8340+10n, PLSY/PLSR 누적 펄스 D8140+2n,
BUSY/완료/비정상 종료, 속도·가감속·리미트·DOG/제로·DVIT 인터럽트,
PWM 출력, TBL의 R/D 메모리 배치, ABS의 2비트 서보 핸드셰이크와 6비트
체크섬을 사용한다. `ConfigurePositioningTable`, `SetPositioningEntry`,
`SetAbsoluteEncoder`, `GetAxisState`로 외부 가상 장치를 설정·관측한다.
PID는 공식 증분식, 샘플링 주기, 필터, 경보·상하한, 스텝 응답 및 리미트
사이클 튜닝의 모델을 사용한다. PR은 ASCII 병렬 출력/스트로브를 실행한다.

## 통신·SFC·가상 외부 장치

공식 219개 응용 명령 모두 실행 경로가 등록되어 있다. 기본 STL/RET도
실행한다. 명령을 빈 동작으로 성공 처리하지 않는다.

IST는 수동·원점 복귀·스텝·단일 사이클·연속 운전의 선택 입력과
M8040..M8047, 초기 S0/S1/S2를 처리한다. STL/RET는 상태 전이,
이전 상태의 한 스캔 출력 겹침과 종료 처리, 서로 다른 상태의 Y 출력 OR,
SET 출력 유지, 최대 8개 상태의 병렬 합류와 활성 상태 모니터를 처리한다.

RS/RS2는 가상 직렬 포트 3개의 송수신 큐, 통신 형식과 속도, 헤더·종료
문자·체크섬, 수신 완료·타임아웃·특수 디바이스를 사용한다.
`InjectSerialReceived`와 `ReadSerialTransmitted`로 가상 장치를 연결한다.

IVCK/IVDR/IVRD/IVWR/IVBWR/IVMC는 채널별 가상 인버터의 모니터,
명령·주파수·파라미터·일괄 전송, 응답 지연·시간 초과·오류를 처리한다.
`ConfigureInverter`, `SetInverterParameter`, `SetInverterMonitor`로 설정한다.

ADPRW는 가상 MODBUS 슬레이브의 코일·접점·입력/홀딩 레지스터,
단일·다중 읽기/쓰기, 마스크 쓰기, 동시 읽기/쓰기, 진단·이벤트·ID,
브로드캐스트, 재시도·시간 초과와 예외 응답을 처리한다.
`ConfigureModbusSlave`와 `SetModbusValue`로 상대 장치를 설정한다.

FLCRT/FLDEL/FLWR/FLRD/FLCMD/FLSTRD는 가상 CF 카드의 파일 생성·삭제,
비트·정수·16/32비트 16진수·부동소수점·문자열·혼합 행 전송,
타임스탬프·헤더, 링/FIFO, 파일 ID 연결, 버퍼·강제 저장·마운트·오류를
처리한다. `ConfigureCfCard`에서 카드 용량·레코드 풀·가상 버퍼 용량을
설정하며 `ReadCfCsv`로 저장된 파일을 읽는다. 버퍼 용량 기본값 32 KiB는
가상 모델의 설정값이며 실제 CF-ADP 버퍼 용량을 확인한 값이 아니다.
전원 차단은 미저장 행을 버리고, 언마운트·덮어쓰기·최종 행·링 회전은
해당 버퍼를 저장한다. 파일 시스템은 메모리 내 모델이며 FAT16 실제
이미지를 생성하지 않는다.

동일 포트의 통신 명령 충돌을 검사하고 인버터·MODBUS·CF 오류 위치는
GX CSV 내보내기와 같은 명령 스텝 계산을 사용한다.

## 추가 대조가 필요한 범위

등록된 명령에서도 실제 하드웨어의 주파수·서보 특성, 일부 특수 M/D,
드럼 테이블의 모든 디바이스 조합, 고속 입력 특수 모드, PID 자동 튜닝의
모든 산업 공정, 인버터 모델별 파라미터 제한, 모든 SFC/인덱스 조합,
물리 통신 프레임·실제 장치 응답 및 GX Works2 CSV 실기 호환성은 추가
검증이 필요하다. 물리 장비 제어에 사용하기 전에 대상 FX CPU/모듈과
대조해야 한다.

## 검증

추가 GX 실제 시험: 산술·부동소수점·비교 접점·문자열·위치 제어 회로를
208스텝으로 가져와 오류 0건·경고 0건을 확인했다. GX 재출력 후 21개
액션의 명령과 피연산자가 보존됐다. 네이티브 32비트 비교 표기와 $MOV/
FLCRT의 ASCII 문자열 길이별 스텝 계산을 수정했으며 28개 명령 시작
스텝 번호가 GX 재출력과 모두 일치했다. 최종 CSV의 실제 재가져오기와
CTest 12/12, MinGW 전체 앱 빌드도 통과했다.

2026-10-01 실제 GX Works2 시험: 외부 장치 명령 16종은 148스텝,
STL/RET 회로는 12스텝으로 가져오기·표시됐으며 F4 오류 대화상자가
없었다. GX 재출력 CSV를 앱에서 재가져와 명령과 피연산자 보존을 확인했다.
SET S 및 문자열 FLCRT의 스텝 계산을 수정한 뒤 명령 시작 스텝 번호도
GX 재출력과 일치했다. 상세 범위는 [실제 시험 기록](GX_WORKS2_IMPORT_TEST.md)에
정리했다. 전체 명령·피연산자 조합의 실기 검증 완료를 뜻하지 않는다.

`tests/ladder_rewrite_test.cpp`가 명령 파싱, CSV 왕복, 스캔 실행,
메모리·가상 입출력·축·PID·ABS/서보·에러를 검사한다. MSVC에서
`ladder_rewrite_tests`와 CTest를 실행하고 MinGW `-Werror` 설정으로
전체 `PLCSimulator`를 빌드한다. 공식 명령 목록과 레지스트리 대조는
`scripts/update_fx_instruction_coverage.py`로 갱신한다.

2026-09-30 검증: CTest 12/12 통과, MinGW `-Werror` 전체
`PLCSimulator` 빌드 통과, 공식 목록 대조 219/219. RS/RS2·IST·인버터
6종·ADPRW·CF 6종·STL/RET를 포함한 CSV 왕복에서 명령 및 오퍼랜드
순서를 보존했다. CF 숫자 비트 보존·혼합 행·버퍼·전원 차단·최종 행
저장·문자열 덮어쓰기와 SFC 병렬 합류의 전이를 실행 테스트했다.

기준 자료: [Mitsubishi Electric 응용 명령 목록](https://www.mitsubishielectric.com/fa/products/cnt/plc_fx/pmerit/contents/plc/instruction.html),
[FX3 위치 제어](https://dl.mitsubishielectric.com/dl/fa/document/manual/plc_fx/jy997d16801/jy997d16801k.pdf),
[FX3 아날로그/PID](https://dl.mitsubishielectric.com/dl/fa/document/manual/plc_fx/jy997d16701/jy997d16701r.pdf),
[FX3 프로그래밍](https://dl.mitsubishielectric.com/dl/fa/document/manual/plc_fx/jy997d16601/jy997d16601r.pdf),
[FX3 통신](https://dl.mitsubishielectric.com/dl/fa/document/manual/plc_fx/jy997d16901/jy997d16901r.pdf),
[MODBUS](https://dl.mitsubishielectric.com/dl/fa/document/manual/plc_fx/jy997d26201/jy997d26201g.pdf),
[FX3U-CF-ADP](https://dl.mitsubishielectric.com/dl/fa/document/manual/plc_fx/jy997d35401/jy997d35401e.pdf).
