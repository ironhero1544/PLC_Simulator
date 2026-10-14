# Ladder programming core rewrite

> 이 문서는 2026-09-28의 구현 기록입니다. 현재 범위와 남은 명령은
> [FX_RUNTIME_STATUS.md](FX_RUNTIME_STATUS.md) 및
> [FX_INSTRUCTION_COVERAGE.md](FX_INSTRUCTION_COVERAGE.md)를 확인하세요.

2026-09-28 · `LADDER_REWRITE_PLAN.md`의 2.0 코어 1차 개편

## 실행 경로

기존 `CompiledPLCExecutor`와 스캔/I/O 인터페이스를 유지했다. 별도 실행기,
VM, JIT 또는 런타임은 추가하지 않았다. 기존 타이머/카운터의 수치 실행
처리도 이 클래스에서 계속 사용한다.

```
Ladder grid / imported canonical topology
  -> CompileLadder / CompileStatements
  -> ExecutionProgram (typed operands and condition DAG)
  -> CompiledPLCExecutor::LoadProgram
  -> existing scan and I/O / Wiring / Physics
```

실행 경로에서 generated C++와 regex 역파싱을 제거했다. LD/CSV 문자열은
가져오기·내보내기 경계에서만 처리한다. 주소는 컴파일 시 정규화하며 스캔의
명령과 접점은 숫자 인덱스로 메모리에 접근한다. 외부 문자열 I/O API는
기존 호출부를 위해 유지하며 해석 결과를 캐시한다.

## 구현 범위

- DeviceAddress/Operand/InstructionDef: X/Y 8진 주소, M/S/T/C/D,
  일반 M과 특수 M 분리, K/H 상수, 오퍼랜드 수·유형·범위 검증.
- 열별 연결 버스와 조건 DAG: 직렬/병렬/중첩 분기, 정상/반전/엣지 접점,
  비교 접점, 단절 출력·순환 참조·잘못된 디바이스 검증.
- F4: 검증 후 구조화된 프로그램을 기존 실행기에 설치. 실패 시 실행
  프로그램을 유지하고 ERROR 표시. 편집 시 UNCONVERTED, 성공 시 CONVERTED.
- 인라인 명령 입력, 레지스트리 기반 명령 추천, 입력 오류 표시,
  Enter 후 커서 이동, 균일한 칼럼 폭, D 값 모니터 표시.
- F5/F6 접점, Shift+F5/F6 상승/하강 접점, F7/F8 입력 진입과 기존
  결선 단축키, F4 변환.
- OUT/SET/RST, 타이머/카운터, MOV, ADD/SUB/MUL/DIV, INC/DEC,
  WAND/WOR/WXOR, CMP/ZCP, ZRST, ALT, BMOV/FMOV 및 지원되는 D/P/DP 변형.
- GX CSV: 따옴표 처리, 탭/쉼표 입력, UTF-8 BOM/UTF-16LE 입력,
  다중 행 오퍼랜드, 타이머/카운터 프리셋, MPS/MRD/MPP/ANB/ORB.
  가져온 분기 명령은 typed canonical topology로 보존하며, 그리드 변경 시
  fingerprint를 비교하여 캐시를 무효화한다.
- I/O 라벨과 물리 연동 주소를 X0..X7/X10..X17, Y0..Y7/Y10..Y17로 통일.

## GX Works2 wrapping 입력 수정

`+ Column`과 명령 길이에 따른 칼럼 폭 확장을 제거했다.
공식 [Simple Project 매뉴얼 6.2.7](https://dl.mitsubishielectric.com/dl/fa/document/manual/plc/sh080780eng/sh080780engag.pdf)
기준으로 F9 수평선 입력의 `K번호`를 wrapping 표식으로 처리한다.
두 번째 이후 칼럼의 출발 표식은 다음 행을 추가하며, 다음 행 첫 칼럼에서
F9로 동일한 번호를 입력하면 도착 표식이 된다. 숫자는 수평선 길이 입력이다.

표식은 디바이스나 PLC 명령이 아니라 편집 토폴로지다. 컴파일 시 두 줄을
하나의 조건 DAG로 연결한다. 번호 불일치·중복·잘못된 위치·순환을 거부하며
CSV에서는 연결된 PLC 명령으로 직렬화한다. XML과 클립보드 타입 변환에도
표식을 등록했다. F4에서 전체 회로가 현재 폭에 들어가면 표식을 정리한다.
행 메모가 있는 경우 배치를 유지한다.

기본 12열과 기존 파일의 열 데이터는 보존한다. 모든 GX Works2 표시 폭
선택이나 자동 wrapping 배치 정책을 복제한 것은 아니다. 실제 GX Works2
GUI와 비교한 조작 검증은 추가로 필요하다.

## 추가 구현: Phase 6 모니터

- 기존 실행기에 스캔별 접점/배선 조건과 출력 실행 여부 관측값을 추가했다.
  스캔용 저장 공간은 프로그램 설치 때 할당하며, 스캔 루프에서는 재할당하지 않는다.
- SET/RST의 유지된 비트 값과 명령 실행 조건을 구분한다. P 명령은 실제
  상승 엣지 스캔에서만 활성 표시한다. 가져온 canonical 프로그램과 F4로
  축약한 wrapping 배치에도 현재 화면 좌표를 연결한다.
- 프로그램 설치 직후, 메모리 초기화 직후, 실행 실패 후에는 이전 관측값을
  표시하지 않는다. 실행 실패의 원인과 완료된 명령 수를 마지막 결과에 남긴다.
- 모니터의 응용/비교 명령에는 모든 D/T/C 피연산자 값을 표시하고, 긴 값은
  셀 내부에서 잘라 표시한 뒤 툴팁으로 전체를 보여준다. D 레지스터 목록도
  추가했다. 외부 PLC RUN과 편집기 자체 모니터는 각각 실행 중인 엔진을 읽는다.
- 열 추가나 열 폭 자동 증가 기능은 다시 도입하지 않았다. 가변 명령 박스와
  표시 폭을 넘는 명령의 자동 wrapping 배치는 아직 후속 항목이다.

## 추가 구현: Tier 3 데이터 명령

기존 OpenPLC 실행기에 다음 12개 기본 명령을 추가했다.

| 명령 | 추가한 동작 | 변형 |
| --- | --- | --- |
| ROR / ROL | 16/32비트 우/좌 회전, 마지막 비트 M8022 | P, D, DP |
| RCR / RCL | M8022를 포함한 우/좌 회전 | P, D, DP |
| SFTR / SFTL | 비트 시프트 레지스터와 입력 비트 삽입 | P |
| WSFR / WSFL | 워드 시프트 레지스터와 입력 워드 삽입 | P |
| DECO | 하위 n비트 값으로 one-hot 비트/워드 생성 | P |
| ENCO | 가장 높은 ON 비트의 위치 인코딩 | P |
| SUM | ON 비트 수 집계, M8020 zero flag | P, D, DP |
| BON | 지정된 16/32비트 위치를 비트 목적지에 복사 | P, D, DP |

공식 `JY997D16601R`의 11.1~11.8, 12.2~12.5절을 기준으로 구현했다.
[공식 매뉴얼](https://dl.mitsubishielectric.com/dl/fa/document/manual/plc_fx/jy997d16601/jy997d16601r.pdf)

레지스트리 등록으로 inline 입력, 형식 검사, F4, 그리드 명령 복원,
GX CSV import/export가 같은 opcode를 사용한다. shift는 겹치는 source를
이동 전에 저장한다. n1과 DECO/ENCO의 n은 K/H 상수이고, rotate/BON의
n과 shift의 n2에는 D도 사용할 수 있다. 상수 범위는 F4에서, D 값 범위는
실행 전에 검사한다. bit range는 X/Y/M/S, word range는 D 범위를 지원한다.
packed-bit, D.b, 인덱스, T/C/R 워드 목적지는 이번 추가 범위에 포함하지 않았다.

CJ/CALL/FOR/NEXT 및 Tier 4 명령군은 아직 미구현이다.

## 추가 구현: FX CPU 전송/변환/데이터/RTC 19종

| 계열 | 기본 명령 | 변형 |
| --- | --- | --- |
| 전송 | SMOV, CML, XCH | SMOV는 P, CML/XCH는 P/D/DP |
| 수 변환 | BCD, BIN, GRY, GBIN | P/D/DP |
| 계산 | NEG, MEAN, SQR | P/D/DP |
| 바이트 | SWAP, WTOB, BTOW | SWAP은 P/D/DP, WTOB/BTOW는 P |
| 범위 | LIMIT, BAND | P/D/DP |
| FIFO | SFWR, SFRD | P |
| RTC | TRD, TWR | P |

`SMOV`는 피연산자 5개를 처리하며 이진 정수를 4자리 십진수로 나눠
선택된 자리만 옮긴다. BCD/BIN은 잘못된 수 범위를 실행 전에 거부한다.
MEAN은 signed 64-bit 합산 후 나눗셈의 나머지를 버린다. SQR은 정수
제곱근과 M8020/M8021, FIFO는 M8020/M8022와 포인터를 처리한다.
WTOB/BTOW는 원본을 먼저 확보해 겹치는 범위와 홀수 바이트 수를 처리한다.
범위를 벗어나는 상수 크기는 F4에서, D 크기는 명령 실행 전에 검사한다.

TRD/TWR는 내부 PLC RTC의 7개 워드(년/월/일/시/분/초/요일)를 읽고 쓴다.
RTC는 시스템의 로컬 날짜로 초기화하고 이후 steady clock으로 진행한다.
TWR는 컴퓨터의 시스템 시간을 변경하지 않는다. 2자리 년도 모드를
사용하며 요일을 날짜에서 계산한다. 존재하지 않는 날짜를 쓰면 이전
RTC를 유지한다. 메모리 리셋이나 프로그램 교체로 RTC를 초기화하지 않는다.
특수 D8013~D8019 직접 접근, 4자리 년도 모드 및 M8015 제어는 후속 범위다.

이 추가도 기존 실행기의 typed instruction 경로를 사용한다. 새 VM은 없다.
공식 `JY997D16601R`의 9.4~9.10, 10.10, 11.9~11.10, 12.6/12.9,
19.2/19.3/19.6, 21.7~21.8, 22.1~22.2, 29.1~29.2절을 확인했다.
범용 디바이스 범위는 기존 D/X/Y/M/S 지원 범위 안에서 동작한다.

## 검증

MSVC Debug 새 디렉터리 `cmake-build-ladder-verified`에서 전체 앱과 테스트를
빌드했다. 기존 빌드 캐시의 한국어 `/showIncludes` prefix 인코딩 문제는
구성·빌드 쉘의 `chcp 65001`로 해결했다. 캐시된 FetchContent 소스를 사용해
네트워크 다운로드 없이 구성했다.

CTest의 기존 11개 테스트와 새 `ladder_rewrite` 항목을 실행한다.
전체 앱 빌드와 CTest 12/12 항목이 통과했다.
MinGW GCC 15.2 Release의 `PLCSimulator` 빌드도 `-Werror`를 유지한 채
통과했다. 이전 경로의 미사용 helper를 제거하고 aggregate 기본 초기화,
signed/unsigned 비교 및 optional 접근에 대한 GCC 경고를 수정했다.
새 항목은 다음 시나리오를 한 실행에서 검증한다.

- 주소/상수/경계 검증, 직렬·반전 접점, 병렬 입력 조합.
- 실패한 프로그램 설치 후 기존 실행 프로그램 유지.
- 타이머 시간 누적, 카운터 상승 엣지와 리셋.
- 중첩 CSV 분기의 32개 입력 조합과 다중 오퍼랜드 왕복.
- MPS 후 RST가 접점 메모리를 바꿔도 MPP에서 저장된 조건 유지.
- 같은 조건을 공유하는 그리드 출력의 CSV 왕복과 캐시 무효화.
- 서로 다른 출력 조건의 공통 접점이 앞선 출력에서 변경될 때,
  CSV의 MPS/MRD/MPP로 공통 조건을 보존하는 왕복.
- MOVP/ADDP, 비교 접점, 잘못된 wide 목적지와 스택 underflow.
- 차단된 직렬 접점/배선의 관측, SET 유지 상태와 실행 조건 구분,
  가져온 명령 좌표와 P 명령 관측, 리셋/실행 실패 시 관측 무효화,
  0 나눗셈 원인 보존.
- FX CPU 19종의 16/32비트 계산·변환 및 CSV 왕복, SMOV 5개
  피연산자 입력, FIFO full/empty/P 동작, 겹치는 홀수 바이트 변환,
  RTC 윤년·요일·무효 날짜·메모리 리셋, 잘못된 명령 범위 거부.
- Tier 3 회전/carry, 비트·워드 shift, DECO/ENCO, SUM/BON,
  P 명령의 유지 입력 및 재상승 엣지, overlapping source 보존,
  상수·D count 범위 오류와 신규 명령 CSV 왕복.
- LD 왕복, DIV 몫/나머지, DMUL 4레지스터 결과, CMP/ZCP,
  겹치는 BMOV와 FMOV, D 레지스터 RST/ZRST.

```powershell
ctest --test-dir cmake-build-ladder-verified --output-on-failure --timeout 15
```

전체 빌드에 필요한 기존 테스트의 오래된 enum/필드/API 이름을 현재 모델에
맞췄다. MSVC 경고 및 ImGui 호출의 기존 빌드 오류도 수정했다. Wiring/Physics
계산 알고리즘은 변경하지 않았다.

## 검증의 한계와 후속 범위

GX Works2 실제 데스크톱에서 파일을 가져오는 시험과 앱 GUI를 통한
편집→F4→RUN→물리 장치 시각 검증은 아직 수행하지 않았다. CSV step 수와
인코딩의 외부 도구 호환성을 실제 GX Works2에서 별도로 확인해야 한다.

이 구현은 FX3U 하드웨어 전체의 동작을 보장하지 않는다. 기존 엔진의
16개 X/Y 및 256개 T/C 범위를 유지하며 타이머는 기존 100ms 프리셋 기준이다.
타이머 클래스별 시간 단위, 고속/32비트 카운터, 특수 M 전체의 하드웨어
기능, packed-bit/인덱스 주소, Tier 3의 프로그램 제어 및 Tier 4 명령군은 후속 범위다.
M8000과 M8002는 RUN/최초 스캔 상태로 처리한다.

가져온 canonical 분기와 공통 접점에서 갈라지는 편집 그리드의 저장 조건을
보존한다. 여러 공유 부분이 교차하는 비정형 DAG까지 포함한 외부 도구
호환성은 추가 회귀 시험이 필요하다.
