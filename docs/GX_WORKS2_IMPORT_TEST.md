# GX Works2 실제 CSV 가져오기 시험

2026-10-01, 설치된 GX Works2의 FXCPU / FX3U/FX3UC / Simple Project /
Ladder / 라벨 없음 프로젝트에서 Computer Use로 수행했다.

## 확인 결과

1. `ExportGXCSV`가 출력한 UTF-8 CSV를 Edit → Read from CSV File로
   가져오면 `No readable information found in the file` 오류가 발생했다.
2. GX Works2의 Write to CSV File로 END만 있는 새 프로젝트를 저장했다.
   저장된 파일은 BOM `FF FE`, UTF-16LE, CRLF, 탭 구분 형식이었다.
3. 같은 기본 회로를 UTF-16LE BOM/CRLF로 저장하자 28스텝으로 정상
   가져와졌다. X000/X001 병렬 접점, X002 반전 접점, Y000, MOV K123 D0,
   DMOV K123456 D10, T0 K10, C0 K5 및 END가 래더 화면에 표시됐다.

이에 `ExportGXCSV`의 기본 출력과 앱의 CSV 저장을 UTF-16LE BOM/CRLF로
수정했다. `GXCSVEncoding::kUtf8`은 텍스트 검사 용도로 명시 선택할 수 있다.
가져오기는 기존 UTF-8/UTF-8 BOM/UTF-16LE를 계속 지원하고 UTF-16 서로게이트
쌍을 검증한다. Unicode 문자열 및 잘못된 인코딩에 대한 자동 테스트를 추가했다.

수정된 코드가 출력한 `basic-native.csv`는 실제 GX Works2가 받아들인
`basic-utf16.csv`와 바이트 단위로 일치했다. 기본·외부 장치·SFC 파일의
내부 CSV 왕복도 모두 통과했다(16진 상수의 선행 0 정규화 포함).
CTest 12/12 및 MinGW `-Werror` 전체 앱 빌드를 통과했다.

## 추가 실제 시험

Windows 잠금 해제 후 같은 프로젝트에서 다음 시험을 완료했다.

| 입력 | GX Works2 표시 | F4 | GX 재내보내기 → 앱 재가져오기 |
| --- | --- | --- | --- |
| `external-native.csv` | 148스텝 | 오류 대화상자 없음 | 16개 액션, 명령·피연산자 보존 |
| `sfc-native.csv` | 12스텝 | 오류 대화상자 없음 | 7개 액션, 명령·피연산자 보존 |

외부 명령은 IST, RS, RS2, IVCK, IVDR, IVRD, IVWR, IVBWR, IVMC,
ADPRW, FLCRT, FLDEL, FLWR, FLRD, FLCMD, FLSTRD를 포함한다.
GX가 저장한 `H0ED`와 `H3`는 앱에서 각각 `HED`, `H3`로 정규화되며
상수 값은 유지됐다. FLCRT의 문자열 `"DATA"`도 유지됐다.
SFC 입력은 SET S0, STL S0, OUT Y0, SET S20, STL S20, OUT Y1,
RET 및 END의 순서와 조건 접점을 유지했다.

재내보내기 대조에서 스텝 계산 오류도 발견했다. `SET S`를 1스텝으로,
문자열 `"DATA"`를 사용하는 FLCRT를 9스텝으로 계산했지만 GX는 각각
2스텝과 11스텝을 사용했다. 이를 수정하고 회귀 테스트를 추가했다.
수정 후 `external-corrected.csv`, `sfc-corrected.csv`의 모든 명령 시작
스텝 번호가 GX 재출력과 일치했다. CTest 12/12와 MinGW 전체 앱 빌드도
다시 통과했다. 다른 길이·인코딩의 파일명은 추가 실제 시험이 필요하다.

이는 해당 입력의 가져오기·표시·F4·CSV 왕복 검증이다. 모든 FX 명령과
모든 피연산자 조합, GX 프로그램 검사, 실제 CPU 실행은 아직 전수 검증하지
않았다. 기본 회로의 F4 및 GX 재내보내기도 별도 확인이 필요하다.

## 산술·문자열·위치 제어 추가 시험

`advanced.ld`는 ADD/DADD/MUL/DIV, DEMOV/DEADD/DECMP, $MOV,
FLCRT의 1·2·3·5바이트 파일명, 16/32비트 비교 접점,
PLSY/DDRVI/DDRVA를 포함한다. 최초 가져오기에서 일부 회로는 표시됐지만
Output 창에 `Invalid instruction. Please correct it.`, `Row 54`가 보고됐다.
54행은 `DLD>=`였다. GX 및 공식 FX 표기는 `LDD>=`이다.
CSV 내보내기의 DLD/DAND/DOR를 LDD/ANDD/ORD로 변환하고 가져오기에는
역변환을 적용했다. 기존 내부 표기는 유지된다. 네이티브 표기와 왕복 실행
회귀 테스트를 통과했다(CTest 12/12, MinGW 전체 빌드 성공).

잠금 해제 후 `advanced-corrected.csv`를 실제 GX에서 재가져왔다.
208스텝, 가져오기 Output 오류 0건·경고 0건이며 F4 후에도 오류 창이
없었다. GX가 직접 저장한 `gx-advanced-roundtrip.csv`를 앱에서 다시
가져오자 21개 액션이 통과했고 전체 명령·피연산자가 원본과 일치했다.
PLSY, DDRVI, DDRVA의 상수·출력 주소 및 부호도 유지됐다.

재출력 대조에서 $MOV 및 FLCRT 문자열 길이에 따른 스텝 차이를 발견했다.
1·2바이트 ASCII 문자열은 기본 스텝, 3바이트는 +2, 5바이트는 +4였다.
계산을 수정하고 해당 길이의 회귀 테스트를 추가했다. 최종 코드로 생성한
`advanced-final.csv`의 28개 명령 시작 스텝 번호는 GX 재출력과 전부
일치했다. 최종 파일의 실제 GX 가져오기도 208스텝, 오류 0건·경고 0건이다.
CTest 12/12 및 MinGW 전체 앱 빌드를 통과했다.

이번 시험은 CSV 호환 검증이며 실제 축 구동과 CPU 실행을 검증한 것은
아니다. 비ASCII·이스케이프 문자열, 다른 문자열 명령 및 전체 FX 명령
조합은 추가 검증이 필요하다.

## 재현 파일

`tmp/gx-import-test/`에 기본·외부 장치·SFC 명령의 `.ld` 입력과 CSV를
준비했다. `gx-native-empty.csv`는 GX Works2가 직접 저장한 형식 기준 파일,
`basic-utf16.csv`는 실제 가져오기에 성공한 파일이다.
`external-native.csv`, `sfc-native.csv`는 수정된 `ExportGXCSV`로 생성했다.
`gx-external-roundtrip.csv`, `gx-sfc-roundtrip.csv`는 GX Works2가 직접
재내보낸 파일이며, 같은 이름의 `.ld`는 앱 코드로 재가져온 명령 목록이다.
준비용 `tmp/gx_csv_probe.cpp`는 기존 프로젝트 오브젝트에 연결하여 실행하며
프로그램 컴파일·CSV 내보내기에 실제 앱과 같은 코드를 사용한다.
