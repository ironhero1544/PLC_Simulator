# 공개문제 3번 직접 제작 가이드

**부품 배치 → 전원 배선 → 제어 배선 → HMI·래더 작성** 순서로 진행합니다.
각 페이지 맨 아래의 **다음 →** 링크로 이어가세요.

## 실습 시작

| 순서 | 할 일 | 완료 기준 |
| --- | --- | --- |
| 1 | [부품 배치](01-placement.md) | 부품 39개, 연결선과 HMI 화면은 비어 있음 |
| 2 | [전원 배선](02-power.md) | 전원·동력 연결 51개 |
| 3 | [제어 배선](03-control.md) | 제어·공압·광 연결 65개 |
| 4 | [HMI 화면 만들기](04-hmi.md) | 테스트 동작 / 장비 시운전 두 화면 |
| 5 | [래더 입력 방법](05-ladder-basics.md) | 접점·명령을 넣는 방법 익히기 |

[부품 배치 JSON 받기](exam03-placement-only.json)

배선·HMI·프로그램은 가이드를 보고 직접 작성합니다.

## 동작별 래더 작성

한 페이지에서 **동작 흐름 → 논리도 → 실제 래더 → 작성 방법 → 완료 확인**을 봅니다.

1. [초기화](logic-01-init.md)
2. [수동 조작](logic-02-manual.md)
3. [공급·가공](logic-03-process.md)
4. [재질 분기](logic-04-material.md)
5. [흡착·적재](logic-05-storage.md)
6. [배출·종료](logic-06-finish.md)
7. [일시정지·복귀](logic-07-pause.md)

[래더를 읽는 방법](reference-ladder-reading.md) · [컴파일과 시운전 확인](06-checks.md)

## 필요한 때만 보는 참고 자료

- [부품 좌표·Z 설정](reference-placement.md)
- [주소별 역할](reference-devices.md)
- [단계·서보·복귀 조건](reference-sequence.md)
- [전체 회로 입력표·명령어](reference-program.md)

