# Full Adder: RTL 부품 배치·배선·사용 실습

[사용법 목차](README.md) · [RTL 제작 기본 조작](12-rtl.md)

**직접 만든 Full Adder를 실배선 캔버스에 놓고, 버튼으로 입력을 바꿔 두 램프에서 결과를 읽습니다.**
A·B·CIN은 1비트 입력입니다. SUM은 합의 아래 비트, COUT은 자리올림입니다.
예를 들어 `1 + 1 + 0 = 2`이면 SUM=0, COUT=1입니다.

## 1. Full Adder 만들기

1. [Tool Doctor](rtl-tool-doctor.md)에서 빌드 환경을 준비합니다.
2. **RTL → RTL 라이브러리 → 새 RTL 모듈**을 누릅니다.
3. **표시 이름**을 `FULL ADDER`, **탑 모듈**을 `full_adder`로 바꿉니다.
4. 기본으로 생성된 [TOP] 파일을 선택하고 오른쪽 편집기에 아래 코드를 입력합니다.

```verilog
module full_adder(
  input wire VCC,
  input wire GND,
  input wire A,
  input wire B,
  input wire CIN,
  output wire SUM,
  output wire COUT
);
  assign SUM = A ^ B ^ CIN;
  assign COUT = (A & B) | (A & CIN) | (B & CIN);
endmodule
```

![Full Adder 소스와 탑 모듈 설정](../../images/program-guide/full-adder-01-source.jpg)

*실제 앱에 입력한 Full Adder입니다. 파일 이름과 탑 모듈 이름을 구분하세요.*

VCC·GND는 실배선 전원을 연결할 핀입니다. 이 실습은 **산업용 24V 로직**을 사용합니다.
각 포트는 위 코드처럼 한 줄에 하나씩 선언합니다.
파일 이름은 자동 생성된 이름이어도 됩니다. **탑 모듈**은 코드의 `module full_adder`와 맞춰야 합니다.

5. **분석**을 누릅니다. 입력 VCC·GND·A·B·CIN, 출력 SUM·COUT이 있는지 읽습니다.
6. **빌드**를 누르고 ‘빌드 상태: 성공’을 기다립니다.
7. 아래 결과 영역의 **부품으로 활성화**를 켭니다. RTL 창을 닫습니다.

![Full Adder 빌드 성공과 활성화 체크](../../images/program-guide/full-adder-02-enabled.jpg)

*빌드 성공 뒤 ‘부품으로 활성화’에 체크한 실제 화면입니다.*

## 2. 캔버스에 배치하기

| 부품 | 수량 | 용도 |
| --- | --- | --- |
| DC 전원 공급기 | 1 | 24V와 0V 공급 |
| FULL ADDER | 1 | 방금 만든 RTL 부품 |
| 램프/버튼 유닛 | 3 | A·B·CIN 입력. 각 유닛의 세 번째 녹색 버튼 사용 |
| 램프/버튼 유닛 | 1 | 첫 번째 램프는 SUM, 두 번째 램프는 COUT 표시 |

1. **실배선 모드**로 이동합니다. 부품 목록 필터를 **전체 / 전체 기능**으로 맞춥니다.
2. 목록을 아래로 내려 `FULL ADDER` 카드를 찾습니다. **전체 / 제어**로 좁혀 찾을 수도 있습니다.
3. 카드를 **캔버스 가운데로 끌어 놓습니다.** 라이브러리 창에서 이름을 선택하는 것만으로는 배치되지 않습니다.
4. 전원은 왼쪽 위, 입력 유닛 세 개는 왼쪽에 세로로, 출력 유닛은 오른쪽에 놓습니다.
5. 입력 유닛 이름을 A·B·CIN, 출력 유닛 이름을 SUM / COUT으로 편집하고 **이름 표시하기**를 켭니다.
6. FULL ADDER 몸체를 더블클릭하고 **RTL 런타임 → 로직 패밀리**에서 **로직 패밀리: 산업용 24V**를 확인합니다.
   전원 핀은 VCC, 접지 핀은 GND입니다. 이 조합 회로에는 클록·리셋을 지정하지 않습니다.

![제어 필터에 나타난 FULL ADDER 카드](../../images/program-guide/full-adder-03-list.jpg)

*위쪽은 자격증 필터 ‘전체’, 아래쪽은 기능 필터 ‘제어’입니다. 카드의 이름은 FULL ADDER입니다.*

![전원·입력·Full Adder·출력 유닛의 배치 예](../../images/program-guide/full-adder-04-placement.jpg)

*왼쪽 A·B·CIN, 가운데 FULL ADDER, 오른쪽 SUM / COUT입니다.*

목록에 없다면 라이브러리에서 **분석 성공 → 빌드 성공 → 부품으로 활성화**를 확인합니다.
선택한 부품 목록 필터가 RTL 부품을 숨기고 있지 않은지도 확인하세요.

## 3. 전원 배선

**배선** 도구를 선택합니다. 전기 핀을 연결하면 전기 배선으로 처리됩니다.
시작 핀 → 필요한 경유점 → 도착 핀 순서로 클릭합니다.

| 시작 핀 | 도착 핀 |
| --- | --- |
| DC 24V | FULL ADDER VCC |
| DC 0V | FULL ADDER GND |
| DC 24V | A·B·CIN 입력 유닛 각각의 **세 번째 행 NO** |
| DC 0V | A·B·CIN 입력 유닛 각각의 **세 번째 행 NC** |
| DC 0V | 출력 유닛의 **첫 번째 행 LAMP_0V** |
| DC 0V | 출력 유닛의 **두 번째 행 LAMP_0V** |

NO·NC는 버튼 접점, LAMP_24V·LAMP_0V는 램프 핀입니다.
입력 유닛의 램프 핀과 출력 유닛의 버튼 접점에는 이번 실습에서 배선하지 않습니다.

![Full Adder 전원 배선 결과](../../images/program-guide/full-adder-05-power.jpg)

*전원 배선을 먼저 마친 실제 앱 화면입니다. 신호선은 다음 단계에서 연결합니다.*

## 4. 신호 배선

| 시작 핀 | 도착 핀 |
| --- | --- |
| A 입력 유닛 세 번째 행 **COM** | FULL ADDER **A** |
| B 입력 유닛 세 번째 행 **COM** | FULL ADDER **B** |
| CIN 입력 유닛 세 번째 행 **COM** | FULL ADDER **CIN** |
| FULL ADDER **SUM** | 출력 유닛 첫 번째 행 **LAMP_24V** |
| FULL ADDER **COUT** | 출력 유닛 두 번째 행 **LAMP_24V** |

버튼을 켜면 COM이 NO의 24V에 연결되어 입력 1,
끄면 COM이 NC의 0V에 연결되어 입력 0이 됩니다.
**COM을 입력에 연결하고 NO·NC로 두 전압을 선택**하는 배선입니다.
24V와 0V를 같은 핀에 직접 연결하지 않습니다.

![전원과 신호선을 모두 연결한 Full Adder](../../images/program-guide/full-adder-06-wired.jpg)

*입력 세 개의 COM은 왼쪽 입력 핀으로, 오른쪽 SUM·COUT은 출력 유닛의 램프로 연결됩니다.*

## 5. 운전하고 결과 읽기

1. **프로그래밍 모드**에서 END만 있는 실습 프로그램을 **컴파일**합니다.
   이 실습에는 X/Y를 제어하는 래더 회로가 필요하지 않습니다.
2. 오른쪽 위 **시작**을 누릅니다. **실배선 → 선택도구**로 돌아옵니다.
3. 각 입력 유닛의 **세 번째 녹색 버튼을 더블클릭**해 켜거나 끕니다.
   세 번째 버튼은 상태가 유지됩니다. 위의 빨강·노랑 버튼은 누르는 동안만 동작하므로 사용하지 않습니다.
4. 출력 유닛의 **첫 번째 빨강 램프=SUM**, **두 번째 노랑 램프=COUT**을 읽습니다.

| A | B | CIN | SUM: 첫 번째 램프 | COUT: 두 번째 램프 |
| --- | --- | --- | --- | --- |
| 0 | 0 | 0 | OFF | OFF |
| 0 | 0 | 1 | ON | OFF |
| 0 | 1 | 0 | ON | OFF |
| 0 | 1 | 1 | OFF | ON |
| 1 | 0 | 0 | ON | OFF |
| 1 | 0 | 1 | OFF | ON |
| 1 | 1 | 0 | OFF | ON |
| 1 | 1 | 1 | ON | ON |

각 버튼의 눌린 모양을 기준으로 입력 상태를 읽습니다.
입력 유닛의 녹색 **램프**는 배선하지 않았으므로 그 램프의 밝기로 입력 상태를 판단하지 않습니다.

![Full Adder 입력 8가지와 SUM·COUT 램프 동작](../../images/program-guide/full-adder-operation.gif)

*위쪽 입력값과 오른쪽 SUM·COUT 램프를 함께 읽으세요.*

## 안 될 때

| 증상 | 확인할 부분 |
| --- | --- |
| 시작이 안 됨 | 래더 컴파일 성공 여부 |
| 두 램프가 항상 OFF | RUN 상태, VCC·GND 배선, 산업용 24V 설정, 빌드 성공 여부 |
| 특정 입력이 안 바뀜 | 세 번째 행 COM·NO·NC 배선과 녹색 버튼 더블클릭 |
| 결과가 반대로 나옴 | NO=24V, NC=0V인지, SUM/COUT 램프 행을 바꾸지 않았는지 |
| 소스를 바꿨는데 옛 동작 | 정지 후 재분석·재빌드하고 다시 시작 |

마지막으로 **프로젝트 → 프로젝트 저장…**에서 `.plcproj`로 보관합니다.
이 형식으로 배선·프로그램·RTL 부품을 함께 저장합니다.

[사용법 목차](README.md) · [RTL 제작 기본 조작](12-rtl.md)
