// include/PhysicsStates.h
// [추론] 기존 std::map<std::string, float> internalStates를 타입 안전한 구조체로 대체
// 각 컴포넌트별로 물리적으로 정확한 상태 변수들을 정의하여 타입 안전성과 성능 향상

#pragma once

namespace plc {

// ========== 전기 컴포넌트 물리 상태 ==========

// [추론] PLC 물리 상태: 실제 PLC의 전기적 특성을 모델링
// - 입출력 전압/전류 정확히 추적
// - 디지털 신호 처리 상태 관리  
// - 내부 연산 부하 및 전력소모 계산
typedef struct PLCPhysicsState {
    // 전기적 특성
    float inputVoltages[16];        // X0-X15 입력 포트 전압 [V] 
    float inputCurrents[16];        // X0-X15 입력 전류 [mA]
    float outputVoltages[16];       // Y0-Y15 출력 포트 전압 [V]
    float outputCurrents[16];       // Y0-Y15 출력 전류 [mA]
    
    // 디지털 상태
    bool inputStates[16];           // X0-X15 디지털 입력 상태
    bool outputStates[16];          // Y0-Y15 디지털 출력 상태
    
    // 내부 특성
    float cpuLoad;                  // CPU 사용률 [%]
    float powerConsumption;         // 소비전력 [W]
    float internalTemperature;      // 내부 온도 [°C]
    
    // 신호 처리 특성  
    float inputResponseTime[16];    // 입력 응답시간 [ms]
    float outputResponseTime[16];   // 출력 응답시간 [ms]
    float inputImpedance[16];       // 입력 임피던스 [Ω]
    float outputImpedance[16];      // 출력 임피던스 [Ω]
} PLCPhysicsState;

// [추론] 전원공급장치 물리 상태: 실제 SMPS의 전기적 특성
// - 정전압 제어, 전류 제한, 리플 특성 모델링
// - 부하 변동에 따른 출력 특성 변화 추적
typedef struct PowerSupplyPhysicsState {
    // 출력 특성
    float outputVoltage24V;         // 24V 출력 전압 [V]
    float outputVoltage0V;          // 0V(GND) 출력 전압 [V] 
    float outputCurrent24V;         // 24V 출력 전류 [A]
    float outputCurrent0V;          // 0V 출력 전류 [A]
    float totalPowerOutput;         // 총 출력 전력 [W]
    
    // 내부 특성
    float efficiency;               // 변환 효율 [%]
    float rippleVoltage;            // 리플 전압 [mV pp]
    float internalTemperature;      // 내부 온도 [°C]
    float loadRegulation;           // 부하 조정률 [%]
    
    // 제어 상태
    bool overvoltageProtection;     // 과전압 보호 동작 여부
    bool overcurrentProtection;     // 과전류 보호 동작 여부
    bool thermalProtection;         // 과온도 보호 동작 여부
    float currentLimit;             // 전류 제한값 [A]
} PowerSupplyPhysicsState;

// [추론] 버튼 유닛 물리 상태: 3개 버튼의 독립적 전기/기계 특성
// - 각 버튼별 접점 저항, LED 특성 개별 관리
// - 기계적 수명, 접점 마모도 추적
typedef struct ButtonUnitPhysicsState {
    // 버튼별 전기적 특성 (3개 버튼)
    float buttonVoltages[3];        // 각 버튼 전압 [V]
    float buttonCurrents[3];        // 각 버튼 전류 [mA]  
    float contactResistances[3];    // 접점 저항 [Ω]
    bool buttonStates[3];           // 버튼 눌림 상태
    
    // LED 특성 (3개 LED)
    float ledVoltages[3];           // LED 전압 [V]
    float ledCurrents[3];           // LED 전류 [mA] 
    float ledBrightness[3];         // LED 밝기 [cd]
    bool ledStates[3];              // LED 점등 상태
    
    // 기계적 특성
    float buttonForces[3];          // 버튼 누르는 힘 [N]
    float contactWear[3];           // 접점 마모도 [%]
    int operationCounts[3];         // 각 버튼 동작 횟수
    float springConstants[3];       // 스프링 상수 [N/m]
} ButtonUnitPhysicsState;

// [추론] 리미트 스위치 물리 상태: 기계-전기 변환 특성
// - 물리적 접촉력과 전기적 접점저항의 관계 모델링  
// - 기계적 히스테리시스, 접점 바운싱 현상 고려
typedef struct LimitSwitchPhysicsState {
    // 기계적 특성
    float contactForce;             // 접촉력 [N]
    float displacement;             // 액추에이터 변위 [mm]
    float springDeflection;         // 스프링 압축량 [mm]
    bool isPhysicallyPressed;       // 물리적 접촉 여부
    
    // 전기적 특성  
    float contactResistanceNO;      // NO 접점 저항 [Ω]
    float contactResistanceNC;      // NC 접점 저항 [Ω]
    float contactVoltage;           // 접점 전압 [V]
    float contactCurrent;           // 접점 전류 [mA]
    
    // 동적 특성
    float responseTime;             // 응답시간 [ms] 
    float hysteresis;               // 히스테리시스 [mm]
    float contactBounceTime;        // 접점 바운싱 시간 [ms]
    int operationCount;             // 동작 횟수
    
    // 환경 특성
    float ambientTemperature;       // 주변 온도 [°C]
    float contactWear;              // 접점 마모도 [%]
} LimitSwitchPhysicsState;

// [추론] 센서 물리 상태: 근접감지 센서의 전자기적 특성
// - 감지거리와 출력신호의 비선형 관계 모델링
// - 환경 노이즈, 온도 드리프트 영향 고려
typedef struct SensorPhysicsState {
    // 전원 특성
    float supplyVoltage;            // 공급 전압 [V]
    float supplyCurrent;            // 공급 전류 [mA]
    float powerConsumption;         // 소비전력 [W]
    
    // 감지 특성
    float targetDistance;           // 타겟까지의 거리 [mm]
    float detectionThreshold;       // 감지 임계값 [mm]
    float outputVoltage;            // 출력 전압 [V]
    float outputCurrent;            // 출력 전류 [mA]
    bool detectionState;            // 감지 상태
    
    // 성능 특성
    float sensitivity;              // 감도 [V/mm]
    float responseTime;             // 응답시간 [ms]
    float hysteresis;               // 히스테리시스 [mm]
    float temperatureDrift;         // 온도 드리프트 [%/°C]
    
    // 환경 특성
    float ambientTemperature;       // 주변 온도 [°C]
    float electromagneticNoise;     // 전자기 노이즈 레벨 [dBm]
} SensorPhysicsState;

// ========== 공압 컴포넌트 물리 상태 ==========

// [추론] FRL 물리 상태: 공압 공급원의 압력 조정 및 필터링 특성  
// - 베르누이 방정식, 유량 방정식 기반 모델링
// - 압력 조정기의 PID 제어 특성 반영
typedef struct FRLPhysicsState {
    // 압력 특성
    float inputPressure;            // 입력 압력 [bar]
    float outputPressure;           // 출력 압력 [bar] 
    float setPressure;              // 설정 압력 [bar]
    float pressureRipple;           // 압력 리플 [bar pp]
    
    // 유량 특성  
    float massFlowRate;             // 질량유량 [kg/s]
    float volumeFlowRate;           // 체적유량 [L/min]
    float flowCoefficient;          // 유량계수 Cv
    float pressureDrop;             // 압력강하 [bar]
    
    // 필터 특성
    float filterEfficiency;         // 필터 효율 [%]
    float filterPressureDrop;       // 필터 압력강하 [bar]
    float contaminationLevel;       // 오염도 [ppm]
    
    // 조정기 특성
    float regulatorGain;            // 조정기 게인
    float regulatorTimeConstant;    // 시정수 [s]
    float regulatorHysteresis;      // 히스테리시스 [bar]
    
    // 동적 특성
    float responseTime;             // 응답시간 [ms]
    float settlingTime;             // 안정시간 [s]
} FRLPhysicsState;

// [추론] 매니폴드 물리 상태: 압력 분배기의 유동 특성
// - 각 포트별 독립적 압력/유량 관리  
// - 연속성 방정식(질량보존) 기반 모델링
typedef struct ManifoldPhysicsState {
    // 입력 특성 
    float inputPressure;            // 입력 압력 [bar]
    float inputFlowRate;            // 입력 유량 [L/min]
    float inputTemperature;         // 입력 온도 [K]
    
    // 출력 특성 (최대 4개 출력 포트)
    float outputPressures[4];       // 각 출력 포트 압력 [bar]
    float outputFlowRates[4];       // 각 출력 포트 유량 [L/min] 
    float outputTemperatures[4];    // 각 출력 포트 온도 [K]
    
    // 내부 특성
    float internalVolume;           // 내부 체적 [L]
    float internalPressure;         // 내부 압력 [bar]
    float pressureLoss;             // 압력손실 [bar]
    float heatTransferCoeff;        // 열전달 계수 [W/m²K]
    
    // 동적 특성
    float pressureResponseTime[4];  // 각 포트별 압력 응답시간 [ms]
    float flowResponseTime[4];      // 각 포트별 유량 응답시간 [ms]
} ManifoldPhysicsState;

// [추론] 단동 밸브 물리 상태: 전기-공압 변환 특성
// - 솔레노이드의 전자기력과 스프링력의 평형 모델링
// - 밸브 개폐에 따른 유량 특성 변화 추적
typedef struct ValveSinglePhysicsState {
    // 전기적 특성 (솔레노이드)
    float solenoidVoltage;          // 솔레노이드 전압 [V]
    float solenoidCurrent;          // 솔레노이드 전류 [A]
    float solenoidResistance;       // 솔레노이드 저항 [Ω]
    float electromagneticForce;     // 전자기력 [N]
    bool solenoidEnergized;         // 솔레노이드 여자 상태
    
    // 기계적 특성
    float valvePosition;            // 밸브 위치 [mm] (0=닫힘, max=완전개방)
    float springForce;              // 스프링 복원력 [N]
    float springConstant;           // 스프링 상수 [N/mm]
    float frictionForce;            // 마찰력 [N]
    
    // 공압 특성 
    float inletPressure;            // 입구(P) 압력 [bar]
    float outletPressureA;          // 출구(A) 압력 [bar]  
    float exhaustPressure;          // 배기(R) 압력 [bar]
    float flowCoefficientPA;        // P→A 유량계수 Cv
    float flowCoefficientAR;        // A→R 유량계수 Cv
    
    // 동적 특성
    float responseTime;             // 응답시간 [ms]
    float switchingFrequency;       // 스위칭 주파수 [Hz]
    int operationCount;             // 동작 횟수
} ValveSinglePhysicsState;

// [추론] 복동 밸브 물리 상태: 2개 솔레노이드의 독립 제어 특성
// - 메모리 기능(래칭), 중간위치 제어 가능성 고려
// - A/B 양방향 압력 제어 및 크로스오버 특성 모델링
typedef struct ValveDoublePhysicsState {
    // 전기적 특성 (솔레노이드 A, B)
    float solenoidVoltageA;         // 솔레노이드A 전압 [V]
    float solenoidVoltageB;         // 솔레노이드B 전압 [V] 
    float solenoidCurrentA;         // 솔레노이드A 전류 [A]
    float solenoidCurrentB;         // 솔레노이드B 전류 [A]
    float electromagneticForceA;    // 전자기력A [N]
    float electromagneticForceB;    // 전자기력B [N]
    bool solenoidEnergizedA;        // 솔레노이드A 여자 상태
    bool solenoidEnergizedB;        // 솔레노이드B 여자 상태
    
    // 기계적 특성
    float valvePosition;            // 밸브 위치 [mm] (음수=A방향, 양수=B방향)
    float springForceA;             // 스프링A 복원력 [N]
    float springForceB;             // 스프링B 복원력 [N]
    float neutralPosition;          // 중립위치 [mm]
    
    // 공압 특성
    float inletPressure;            // 입구(P) 압력 [bar]
    float outletPressureA;          // 출구(A) 압력 [bar]
    float outletPressureB;          // 출구(B) 압력 [bar] 
    float exhaustPressure;          // 배기(R) 압력 [bar]
    float flowCoefficientPA;        // P→A 유량계수 Cv
    float flowCoefficientPB;        // P→B 유량계수 Cv
    float flowCoefficientAR;        // A→R 유량계수 Cv
    float flowCoefficientBR;        // B→R 유량계수 Cv
    
    // 제어 특성
    float lastActivePosition;       // 마지막 활성 위치 (메모리 기능)
    float positionHysteresis;       // 위치 히스테리시스 [mm]
    float responseTimeAB;           // A→B 응답시간 [ms]
    float responseTimeBA;           // B→A 응답시간 [ms]
} ValveDoublePhysicsState;

// ========== 기계 컴포넌트 물리 상태 ==========

// [추론] 실린더 물리 상태: 공압-기계 에너지 변환의 핵심
// - 뉴턴 운동법칙(F=ma) 기반 정확한 동역학 모델링
// - 압력→힘→가속도→속도→위치의 물리적 연쇄 반응 추적
// - 마찰, 관성, 탄성력 등 실제 물리 현상 반영
typedef struct CylinderPhysicsState {
    // 기계적 운동 상태 (뉴턴 역학)
    float position;                 // 피스톤 위치 [mm] (0=완전수축)
    float velocity;                 // 속도 [mm/s]
    float acceleration;             // 가속도 [mm/s²]
    float maxStroke;                // 최대 스트로크 [mm]
    
    // 역학적 특성
    float pistonMass;               // 피스톤+로드 질량 [kg]
    float loadMass;                 // 외부 부하 질량 [kg] 
    float totalMass;                // 총 이동 질량 [kg]
    float momentOfInertia;          // 관성모멘트 [kg⋅m²]
    
    // 힘 분석 (F=ma)
    float pneumaticForceA;          // A챔버 공압력 [N]
    float pneumaticForceB;          // B챔버 공압력 [N] 
    float netPneumaticForce;        // 순 공압력 [N]
    float frictionForce;            // 마찰력 [N]
    float gravityForce;             // 중력 [N]
    float externalForce;            // 외부 하중 [N]
    float totalForce;               // 총 작용력 [N]
    
    // 공압 특성
    float pressureA;                // A챔버 압력 [bar]
    float pressureB;                // B챔버 압력 [bar]
    float pistonAreaA;              // A챔버 피스톤 면적 [cm²]
    float pistonAreaB;              // B챔버 피스톤 면적 [cm²] (로드 단면 제외)
    float chamberVolumeA;           // A챔버 현재 체적 [cm³]
    float chamberVolumeB;           // B챔버 현재 체적 [cm³]
    
    // 마찰 특성 (쿨롱-점성 마찰 모델)
    float staticFriction;           // 정지마찰계수
    float kineticFriction;          // 운동마찰계수  
    float viscousDamping;           // 점성감쇠계수 [N⋅s/m]
    float stickyVelocity;           // 스틱 속도 임계값 [mm/s]
    
    // 탄성 특성
    float springConstant;           // 등가 스프링 상수 [N/m]
    float preloadForce;             // 예압 [N]
    float elasticDeformation;       // 탄성변형량 [mm]
    
    // 동적 응답 특성
    float naturalFrequency;         // 고유진동수 [Hz]
    float dampingRatio;             // 감쇠비 
    float responseTime;             // 응답시간 [ms]
    float settlingTime;             // 정착시간 [s]
    float overshoot;                // 오버슈트 [%]
    
    // 상태 플래그
    bool isExtending;               // 신장 중
    bool isRetracting;              // 수축 중
    bool isAtForwardLimit;          // 전진 한계 도달
    bool isAtBackwardLimit;         // 후진 한계 도달
    bool isStalled;                 // 정지 상태 (힘 평형)
    
    // 성능 특성
    int cycleCount;                 // 작동 사이클 수
    float totalDistance;            // 누적 이동거리 [m]
    float averageSpeed;             // 평균 속도 [mm/s]
    float maxAchievedSpeed;         // 최대 도달 속도 [mm/s]
} CylinderPhysicsState;

// ========== 유틸리티 구조체 ==========

// [추론] 물리 상태 공통 유틸리티
// - 모든 컴포넌트 상태를 통합 관리하기 위한 유니온
// - 타입 안전성을 유지하면서도 일반화된 처리 가능
typedef union ComponentPhysicsState {
    PLCPhysicsState plc;
    PowerSupplyPhysicsState powerSupply;
    ButtonUnitPhysicsState buttonUnit;
    LimitSwitchPhysicsState limitSwitch;
    SensorPhysicsState sensor;
    FRLPhysicsState frl;
    ManifoldPhysicsState manifold;
    ValveSinglePhysicsState valveSingle;
    ValveDoublePhysicsState valveDouble;
    CylinderPhysicsState cylinder;
} ComponentPhysicsState;

// [추론] 물리 상태 타입 식별자
// - 유니온 사용 시 현재 활성화된 상태 타입 식별
// - 타입 안전성 보장을 위한 런타임 타입 체크
typedef enum PhysicsStateType {
    PHYSICS_STATE_NONE = 0,
    PHYSICS_STATE_PLC,
    PHYSICS_STATE_POWER_SUPPLY,
    PHYSICS_STATE_BUTTON_UNIT,
    PHYSICS_STATE_LIMIT_SWITCH,
    PHYSICS_STATE_SENSOR,
    PHYSICS_STATE_FRL,
    PHYSICS_STATE_MANIFOLD,
    PHYSICS_STATE_VALVE_SINGLE,
    PHYSICS_STATE_VALVE_DOUBLE,
    PHYSICS_STATE_CYLINDER
} PhysicsStateType;

// [추론] 타입 안전한 물리 상태 래퍼
// - 유니온 + 타입식별자 조합으로 안전한 상태 관리
// - 잘못된 타입 접근 시 런타임 체크 가능
typedef struct TypedPhysicsState {
    PhysicsStateType type;
    ComponentPhysicsState state;
} TypedPhysicsState;

} // namespace plc