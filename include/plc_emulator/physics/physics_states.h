/*
 * physics_states.h
 *
 * 컴포넌트 물리 상태 구조체 정의.
 * Definitions for component physics state structures.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PHYSICS_PHYSICS_STATES_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PHYSICS_PHYSICS_STATES_H_

namespace plc {


/*
 * PLC 전기/상태 신호 물리값.
 * Physical values for PLC electrical/state signals.
 */
typedef struct PLCPhysicsState {
  float inputVoltages[16];
  float inputCurrents[16];
  float outputVoltages[16];
  float outputCurrents[16];

  bool inputStates[16];
  bool outputStates[16];

  float cpuLoad;
  float powerConsumption;
  float internalTemperature;

  float inputResponseTime[16];
  float outputResponseTime[16];
  float inputImpedance[16];
  float outputImpedance[16];
} PLCPhysicsState;

/*
 * 전원 공급 장치 물리 상태.
 * Power supply physical state.
 */
typedef struct PowerSupplyPhysicsState {
  float outputVoltage24V;
  float outputVoltage0V;
  float outputCurrent24V;
  float outputCurrent0V;
  float totalPowerOutput;

  float efficiency;
  float rippleVoltage;
  float internalTemperature;
  float loadRegulation;

  bool overvoltageProtection;
  bool overcurrentProtection;
  bool thermalProtection;
  float currentLimit;
} PowerSupplyPhysicsState;

/*
 * 버튼 유닛 물리 상태.
 * Button unit physical state.
 */
typedef struct ButtonUnitPhysicsState {
  float buttonVoltages[3];
  float buttonCurrents[3];
  float contactResistances[3];
  bool buttonStates[3];

  float ledVoltages[3];
  float ledCurrents[3];
  float ledBrightness[3];
  bool ledStates[3];

  float buttonForces[3];
  float contactWear[3];
  int operationCounts[3];
  float springConstants[3];
} ButtonUnitPhysicsState;

/*
 * 리미트 스위치 물리 상태.
 * Limit switch physical state.
 */
typedef struct LimitSwitchPhysicsState {
  float contactForce;
  float displacement;
  float springDeflection;
  bool isPhysicallyPressed;

  float contactResistanceNO;
  float contactResistanceNC;
  float contactVoltage;
  float contactCurrent;

  float responseTime;
  float hysteresis;
  float contactBounceTime;
  int operationCount;

  float ambientTemperature;
  float contactWear;
} LimitSwitchPhysicsState;

/*
 * 센서 물리 상태.
 * Sensor physical state.
 */
typedef struct SensorPhysicsState {
  float supplyVoltage;
  float supplyCurrent;
  float powerConsumption;

  float targetDistance;
  float detectionThreshold;
  float outputVoltage;
  float outputCurrent;
  bool detectionState;

  float sensitivity;
  float responseTime;
  float hysteresis;
  float temperatureDrift;

  float ambientTemperature;
  float electromagneticNoise;
} SensorPhysicsState;



/*
 * FRL 물리 상태.
 * FRL physical state.
 */
typedef struct FRLPhysicsState {
  float inputPressure;
  float outputPressure;
  float setPressure;
  float pressureRipple;

  float massFlowRate;
  float volumeFlowRate;
  float flowCoefficient;
  float pressureDrop;

  float filterEfficiency;
  float filterPressureDrop;
  float contaminationLevel;

  float regulatorGain;
  float regulatorTimeConstant;
  float regulatorHysteresis;

  float responseTime;
  float settlingTime;
} FRLPhysicsState;


/*
 * 매니폴드 물리 상태.
 * Manifold physical state.
 */
typedef struct ManifoldPhysicsState {
  float inputPressure;
  float inputFlowRate;
  float inputTemperature;

  float outputPressures[4];
  float outputFlowRates[4];
  float outputTemperatures[4];

  float internalVolume;
  float internalPressure;
  float pressureLoss;
  float heatTransferCoeff;

  float pressureResponseTime[4];
  float flowResponseTime[4];
} ManifoldPhysicsState;


/*
 * 단솔레노이드 밸브 물리 상태.
 * Single-solenoid valve physical state.
 */
typedef struct ValveSinglePhysicsState {
  float solenoidVoltage;
  float solenoidCurrent;
  float solenoidResistance;
  float electromagneticForce;
  bool solenoidEnergized;

  float valvePosition;
  float springForce;
  float springConstant;
  float frictionForce;

  float inletPressure;
  float outletPressureA;
  float exhaustPressure;
  float flowCoefficientPA;
  float flowCoefficientAR;

  float responseTime;
  float switchingFrequency;
  int operationCount;
} ValveSinglePhysicsState;

/*
 * 양솔레노이드 밸브 물리 상태.
 * Double-solenoid valve physical state.
 */
typedef struct ValveDoublePhysicsState {
  float solenoidVoltageA;
  float solenoidVoltageB;
  float solenoidCurrentA;
  float solenoidCurrentB;
  float electromagneticForceA;
  float electromagneticForceB;
  bool solenoidEnergizedA;
  bool solenoidEnergizedB;

  float valvePosition;
  float springForceA;
  float springForceB;
  float neutralPosition;

  float inletPressure;
  float outletPressureA;
  float outletPressureB;
  float exhaustPressure;
  float flowCoefficientPA;
  float flowCoefficientPB;
  float flowCoefficientAR;
  float flowCoefficientBR;

  float lastActivePosition;
  float positionHysteresis;
  float responseTimeAB;
  float responseTimeBA;
} ValveDoublePhysicsState;



/*
 * 실린더 물리 상태.
 * Cylinder physical state.
 */
typedef struct CylinderPhysicsState {
  float position;
  float velocity;
  float acceleration;
  float maxStroke;

  float pistonMass;
  float loadMass;
  float totalMass;
  float momentOfInertia;

  float pneumaticForceA;
  float pneumaticForceB;
  float netPneumaticForce;
  float frictionForce;
  float gravityForce;
  float externalForce;
  float totalForce;

  float pressureA;
  float pressureB;
  float pistonAreaA;
  float pistonAreaB;
  float chamberVolumeA;
  float chamberVolumeB;

  float staticFriction;
  float kineticFriction;
  float viscousDamping;
  float stickyVelocity;

  float springConstant;
  float preloadForce;
  float elasticDeformation;

  float naturalFrequency;
  float dampingRatio;
  float responseTime;
  float settlingTime;
  float overshoot;

  bool isExtending;
  bool isRetracting;
  bool isAtForwardLimit;
  bool isAtBackwardLimit;
  bool isStalled;

  int cycleCount;
  float totalDistance;
  float averageSpeed;
  float maxAchievedSpeed;
} CylinderPhysicsState;


/*
 * 컴포넌트 물리 상태 유니온. (고속 동기화 로직이므로 std::variant를 사용하지 않음)
 * Union of component physics states. (Since it is a fast synchronization logic, std::variant is not used)
 */
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

/*
 * 물리 상태 타입 식별자.
 * Physics state type identifier.
 */
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

/*
 * 타입 태그가 있는 물리 상태.
 * Physics state with a type tag.
 */
typedef struct TypedPhysicsState {
  PhysicsStateType type;
  ComponentPhysicsState state;
} TypedPhysicsState;

}  /* namespace plc */
#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PHYSICS_PHYSICS_STATES_H_ */
