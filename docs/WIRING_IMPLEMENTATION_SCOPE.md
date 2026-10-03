# 실배선 확장 변경 범위

작성일: 2026-10-04  
상태: 저장소 조사에 기반한 구현 범위 지정. 아직 기능 코드 수정 없음.

기준 문서: [전체 계획](WIRING_Z_LAYER_COMPONENT_PLAN.md),
[부품 물리 동작·핀아웃](WIRING_NEW_COMPONENT_SPEC.md).
아래 경로는 프로젝트 루트 기준이다. 신규 파일 이름은 제안이며 기존 파일은
실제 존재와 연결 지점을 확인했다. 파일 전체를 재작성한다는 뜻은 아니다.

## 1. 구현 대상으로 고정하는 기능

1. 전기/공압/광을 시작 핀으로 결정하는 단일 배선 도구.
2. 포트 역할 검증과 Wire 종류 enum, 이전 배선 저장 형식 호환.
3. 그리기 순서와 독립된 물리 Z, 이동 범위/현재 높이/접촉면 모델.
4. 기존 이송·센서·가공·실린더·박스 및 신규 부품의 공통 Z 판정.
5. 신규 목록 항목 13개: 센서 내장 복동, 센서 내장 흡착, 스토퍼,
   DC 모터, AC 서보 모터, 서보앰프, 서보 리프트, 리프트·흡착 일체형,
   6칸 창고, 터치패널, 광화이버 증폭부, 송신 헤드, 수신 헤드.
   송신/수신은 별도 목록 항목이며 구현은 공통 헤드와 역할 설정을 공유할 수 있다.
6. 기본 평면 배치, 정면 확인, 창고 오버레이와 Z 편집 창.
7. 저장/로드/Undo/복사와 실제 배선→물리→PLC 경로 검증.

기존 가공 실린더는 확장 대상이며 신규 목록 항목이 아니다.
입면/평면, 스토퍼 상승/하강은 별도 부품으로 추가하지 않는다.

## 2. 기존 파일의 필수 수정

### 데이터와 등록

| 파일 | 수정 범위 |
| --- | --- |
| `include/plc_emulator/core/data_types.h` | ComponentType 신규 항목, 단일 배선 ToolType, 광 PortType, Wire 종류, Z 설정/상태와 소유/슬롯 데이터 연결 |
| `include/plc_emulator/core/application.h` | 배선 진행 상태, Z 편집/미리보기 상태, Undo에 프로젝트 높이 설정 포함 |
| `include/plc_emulator/components/component_definition.h` | 포트 역할/호환 프로필과 투영·작용면 정의의 접근점 |
| `src/components/component_registry.cpp` | 신규 정의 등록과 목록 정보. 포트 수/렌더/초기화 등록 |
| `src/components/component_behavior.cpp` | 신규 동작 등록, 고정 21개 배열 의존 제거, 설정 메뉴 연결 |
| `src/physics/component_physics_adapter.cpp` | 신규 어댑터 등록, 고정 배열 의존 제거, 공통 축 상태 접근 |

관련 헤더 `component_registry.h`, `component_behavior.h`,
`component_physics_adapter.h`는 새 인터페이스를 노출해야 하는 부분만 수정한다.
기존 모든 타입의 이름과 enum 표기법을 한꺼번에 변경하지 않는다.

### 배선 도구와 네트워크

| 파일 | 수정 범위 |
| --- | --- |
| `src/wiring/application_wiring.cpp` | Start/CompleteWireConnection, 진행/취소/호버 검증, 선 미리보기·렌더·캐시의 bool 의존 제거, 더블클릭/컨텍스트 항목 |
| `src/wiring/application_ports.cpp` | 신규 포트/커넥터 앵커와 역할 툴팁, 기존 포트 위치 보존 |
| `src/application/app_input.cpp` | Q 도구 순환과 배선 단축키/취소 상태 |
| `src/application/app_render.cpp` | 툴바·상태 표시의 전기/공압 도구 통합 |
| `src/application_physics/physics_electrical.cpp` | 전기선 분류, 센서 출력, 서보/통신 역할이 일반 DC 신호와 섞이지 않게 분리 |
| `src/application_physics/physics_update.cpp` | 네트워크 분류와 신규 시뮬레이션 단계 연결 |
| `src/physics/pneumatic_simulation.cpp` | 공압선 종류 판정, VAC 역할과 진공 상태 처리 |
| `src/physics/physics_engine.cpp` | Wire 종류 이행과 신규 상태/효과 연결 |
| `include/plc_emulator/physics/physics_engine.h` | 배선/물리 캐시에서 bool 종류 의존 정리 |
| `src/io/io_mapper.cpp`, `include/plc_emulator/io/io_mapping.h` | 기존 isElectric 의존을 새 종류로 이행. PLC 주소 매핑 동작 보존 |

`isElectric`은 이 파일들에 실제 사용 중이다. Wire 필드만 바꾸고 호출부를
남기는 단계는 완료 단위로 취급하지 않는다. 엔코더/통신/서보 전력은
전기 타입 내 역할로 처리하고, 기계 장착을 새 Wire 타입으로 만들지 않는다.

### 물리 Z와 부품 동작

| 파일 | 수정 범위 |
| --- | --- |
| `src/physics/box2d_simulation.cpp`, `include/plc_emulator/physics/box2d_simulation.h` | 공간 질의 후보의 Z 필터, 작용면/소유 상태, Z만 변해도 캐시 갱신 |
| `src/application_physics/physics_workpiece.cpp` | 공작물 소유·방출·슬롯 이전, XY 폴백에도 동일 Z 적용 |
| `src/application_physics/physics_actuators.cpp` | 공통 실린더 끝단, 가공 Z 매핑, 스토퍼/리프트 축 갱신과 내장 센서 |
| `src/application_physics/physics_sync.cpp` | 고급 엔진과 앱의 Z/축/소유 상태 동기화 |
| `include/plc_emulator/physics/physics_states.h` | 기존 엔진 상태에 신규 축/높이/소유 정보를 연결 |
| `src/application_physics/physics_update.cpp` | 축 갱신→작용면→소유 공작물→접촉 효과→피드백 순서 보장 |
| `src/components/processing_cylinder_def.cpp` | 기존 가공 부품의 Z 표시/센서 역할 정리. 기존 숫자 포트 ID 보존 |

Z를 그리기 순서나 Box2D category bit로 대체하지 않는다. 실제 작용면에
판정을 적용하며, 부품 전체 AABB를 모든 높이에서 차단 영역으로 쓰지 않는다.

### 표시·설정·저장

| 파일 | 수정 범위 |
| --- | --- |
| `src/wiring/application_components.cpp` | 신규 목록 항목, 평면 기본 렌더, 정면/창고 오버레이, 리스트 옆 Z 창 버튼 |
| `src/wiring/application_wiring.cpp` | Z 설정/정면 확인 메뉴, 레이어 표시 필터에 따른 선택 판정 |
| `src/application/app_project.cpp` | 타입 문자열 매핑, Wire 종류와 Z/뷰/초기 슬롯 저장, 버전 2→3 마이그레이션 |
| `resources/lang/ko.lang`, `en.lang`, `ja.lang` | 배선 도구·신규 부품·Z 창·연결 오류·미리보기 문자열 |
| `CMakeLists.txt` | 신규 소스 등록 |
| `tests/CMakeLists.txt` | 새 공유 물리/앱 경로를 테스트 대상에 등록 |
| `tests/physical_runtime_test.cpp` | 실제 압력/축/Z/흡착/창고/서보/광 동작 검증, Wire 종류 이행 |
| `tests/application_runtime_test.cpp` | 도구·저장·Undo·포트 호환·PLC/HMI 연결 회귀 |

Undo/복사 경로는 기존 구현을 먼저 확인하고 같은 데이터 모델을 사용한다.
프로젝트 컨테이너 파일 전체 형식은 배치 JSON 변경만으로 충족되면 변경하지 않는다.

## 3. 추가할 파일과 책임

파일별로 대형 switch를 복제하지 않는다. 아래는 책임 경계이며,
한 책임을 구현하는 데 파일 하나로 충분하면 불필요하게 더 나누지 않는다.

| 신규 파일 제안 | 책임 |
| --- | --- |
| `include/plc_emulator/physics/z_layer.h`, `src/physics/z_layer.cpp` | 높이 설정 검증, 축→높이 매핑, 작용면 접촉 판정 |
| `include/plc_emulator/components/wiring_port_rules.h`, `src/components/wiring_port_rules.cpp` | 타입·역할·프로필·연결 수 공통 검증. UI와 로드가 공유 |
| `include/plc_emulator/physics/servo_simulation.h`, `src/physics/servo_simulation.cpp` | 앰프·모터·볼스크류 축, 누적 펄스 소비와 실제 위치 피드백 |
| `include/plc_emulator/physics/fiber_simulation.h`, `src/physics/fiber_simulation.cpp` | 광 링크, 헤드 정렬·Z·차광, 증폭부 출력 |
| `src/wiring/application_z_layers.cpp` | Z 편집 창/정면 미리보기 UI와 편집 명령 |
| `src/components/sensor_cylinder_def.cpp`와 대응 헤더 | 센서 내장 복동·흡착의 정의/렌더/초기 설정. 기존 실린더 계산 공유 |
| `src/components/stopper_cylinder_def.cpp`와 대응 헤더 | 수납/하강/평면 표시와 스토퍼 정의 |
| `src/components/motor_def.cpp`와 대응 헤더 | DC/AC 서보 모터의 별도 정의, 정면 표시 공통 요소 |
| `src/components/servo_amplifier_def.cpp`와 대응 헤더 | 앰프 형상과 프로필/포트 정의 |
| `src/components/servo_lift_def.cpp`와 대응 헤더 | 리프트·일체형 정의, 평면/정면 렌더, 장착 앵커 |
| `src/components/warehouse_def.cpp`와 대응 헤더 | 6슬롯 설정/표시와 실제 점유 오버레이 |
| `src/components/fiber_sensor_def.cpp`와 대응 헤더 | 증폭부/송신/수신 헤드 정의 및 투영 |
| `src/components/touch_panel_def.cpp`와 대응 헤더 | HMI 정의, 디바이스 매핑 설정과 전원/링크 표시 |

대응 헤더는 `include/plc_emulator/components/`에 둔다. 흡착/슬롯 소유
처리는 기존 공작물 경로에 연결하고, 복잡해질 경우에만 별도 구현 파일로
추출한다. UI 코드에 축 시뮬레이션을 넣지 않는다.

## 4. 조사 결과에 따라 수정하는 조건부 경로

| 파일/영역 | 수정 조건과 한계 |
| --- | --- |
| `src/components/cylinder_def.cpp`, `sensor_def.cpp`, `inductive_sensor_def.cpp`와 헤더 | 실제 공유 가능한 렌더/끝단·출력 설정을 추출할 때만 수정. 전면 재작성 금지 |
| `src/application_physics/physics_ports.cpp`, `physics_pneumatic.cpp` | 신규 역할·진공이 기존 포트/압력 동기화에 들어가는 연결 지점만 수정 |
| `src/physics/physics_solvers.cpp`, `physics_network.h`, `pneumatic_simulation.h` | AC/서보/통신/진공을 기존 일반 해석에서 분리하거나 입력 모델 확장이 필요할 때 |
| `src/application_physics/physics_plc.cpp` | 서보 누적 펄스 전달과 HMI 디바이스 접근 어댑터에 필요한 부분 |
| `include/plc_emulator/programming/compiled_plc_executor.h`와 대응 구현 | 현재 고속 펄스 상태를 외부로 읽을 인터페이스가 없을 때 읽기 API만 추가. 명령 의미/실행 엔진 재설계 없음 |
| `src/wiring/application_snap.cpp`, `core/component_transform.h` | 신규 장착 앵커/뷰 표시가 기존 이동·회전·포트 변환으로 처리되지 않을 때 |
| `src/core/data_types.cpp`, `component_manager.cpp`와 헤더 | 생성·초기화·복사에서 새 필드를 누락하는 경우만 보완 |
| `src/application_physics/physics_box2d.cpp`, `physics_helpers.h` | 공통 Z 질의 인터페이스 전달에 필요할 때 |
| 기존 배선 통합/단위 테스트 | 새 enum/생성 방식에 의한 컴파일 변경 또는 해당 회귀 검증이 필요할 때 |

조건부는 생략 확정이 아니다. 구현 단계에서 호출 경로를 확인해 필요하면
수정하고, 최종 변경 목록에 이유를 기록한다. 현재 확인한 누적 펄스 관련
필드가 있다고 해서 이미 서보 전달 API가 완성됐다고 가정하지 않는다.

## 5. 이번 범위에서 제외

- 프로그래밍 모드 화면, 래더 편집·컴파일 구조, 명령어 집합 재설계.
- 기존 가공 실린더의 중복 신규 제작, 독립 흡착패드·공급 매거진 추가.
- 캔버스 z_order 방식 교체 또는 범용 3D 강체/중력/낙하 시뮬레이션.
- PWM·모터 권선 전류·광학 파동·실제 통신 프레임의 정밀 재현.
- 모든 MR-JE 모델의 핀아웃 지원. 첫 제어 프로필과 모델별 매핑을 분리한다.
- 실시간 전체 시뮬레이션 스냅샷, 네트워크 협업, 프로젝트 파일 전면 재설계.
- 레거시 전체 네이밍 변경, 무관한 meter valve/RTL 내부 구조 리팩토링.

기존 전기·공압 동작의 이행 수정과 PLC 읽기/지령 어댑터는 포함한다.
실제 장비 프로필 미확정은 임의 핀 번호를 만드는 근거가 되지 않는다.

## 6. 변경 묶음과 의존 순서

| 묶음 | 변경 | 다음 단계로 넘어가는 기준 |
| --- | --- | --- |
| A | Wire/포트 역할·도구 통합·저장 호환 | 기존 전기/공압 및 광 역할 연결 검증, 취소/분기/로드 통과 |
| B | Z 공통 모델·공간 질의·기존 물리 효과 | 모든 실행 경로에서 같은 XY/다른 Z 분리 |
| C | Z UI·저장/Undo·뷰 기반과 신규 렌더 등록 | 설정/투영이 배선 앵커와 물리 상태를 깨뜨리지 않음 |
| D | 센서 내장·흡착·스토퍼·기존 가공 | 실제 압력/센서/높이/소유 경로 통과 |
| E | 서보/DC 축·리프트·일체형 | 누적 펄스와 피드백, 두 축/흡착 추종 통과 |
| F | 6슬롯 창고·광화이버·HMI | 적재/재취출·광 검출·PLC 디바이스 제어 통과 |
| G | 통합 공정·성능/회귀 | 비금속 3번→2번, 금속 배출과 저장/복원 통과 |

서로 관련 없는 파일을 같은 묶음에서 청소하지 않는다. 기존 작업 중인
변경을 기준 상태로 존중하며 되돌리거나 덮어쓰지 않는다. 신규 enum으로
프로젝트가 컴파일되지 않는 상태를 중간 완료로 남기지 않는다.

## 7. 현재 확정 정도

변경 **책임과 기존 파일 연결 지점**은 위 표로 특정한다. 신규 파일 개수,
정확한 코드 줄 수, 모델별 커넥터 핀 수는 구현과 프로필 확정 후 결정한다.
지금 단계에서 그 수치를 확정한 것처럼 표시하지 않는다.
다음 구현은 A부터 시작하며 기존 계획의 완료 조건과 부품 명세를 함께 적용한다.
