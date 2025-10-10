// include/PhysicsNetwork.h
// [추론] 물리 네트워크 그래프 구조 정의
// 키르히호프 법칙, 베르누이 방정식, 뉴턴 역학을 적용하기 위한 네트워크 토폴로지
// 노드-엣지 그래프로 실제 물리 시스템의 연결성과 상호작용을 수학적으로 모델링

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PHYSICS_PHYSICS_NETWORK_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PHYSICS_PHYSICS_NETWORK_H_

#include "plc_emulator/core/data_types.h"
#include "plc_emulator/physics/physics_states.h"

namespace plc {

// ========== 공통 네트워크 기본 구조 ==========

// [추론] 네트워크 노드 기본 정보
// - 모든 물리 네트워크에서 공통으로 사용되는 노드 식별 정보
// - 컴포넌트ID + 포트ID 조합으로 고유한 노드 식별
typedef struct NetworkNodeBase {
  int componentId;  // 컴포넌트 인스턴스 ID
  int portId;       // 포트 ID
  int nodeId;       // 네트워크 내 고유 노드 ID
  bool isActive;    // 노드 활성화 상태
} NetworkNodeBase;

// [추론] 네트워크 엣지 기본 정보
// - 두 노드 간의 연결을 나타내는 엣지 정보
// - Wire 구조체로부터 생성되며 물리적 연결 특성 포함
typedef struct NetworkEdgeBase {
  int edgeId;      // 엣지 고유 ID
  int fromNodeId;  // 시작 노드 ID
  int toNodeId;    // 끝 노드 ID
  int wireId;      // 원본 와이어 ID (추적용)
  bool isActive;   // 엣지 활성화 상태
} NetworkEdgeBase;

// ========== 전기 네트워크 구조 ==========

// [추론] 전기 네트워크 노드
// - 키르히호프 전압/전류 법칙 적용을 위한 전기적 특성
// - 각 노드는 전위(전압)를 가지며, 엣지를 통해 전류가 흐름
// - 노드에서의 전류 보존 법칙 (ΣI = 0) 적용
typedef struct ElectricalNode {
  NetworkNodeBase base;  // 기본 노드 정보

  // 전기적 상태
  float voltage;     // 노드 전압 [V] (기준점 대비)
  float current;     // 노드로 유입되는 총 전류 [A]
  float netCurrent;  // 순 전류 (키르히호프 전류 법칙 체크용)

  // 노드 특성
  float nodeCapacitance;     // 노드 등가 커패시턴스 [F]
  float nodeInductance;      // 노드 등가 인덕턴스 [H]
  float leakageConductance;  // 누설 컨덕턴스 [S]

  // 소스/싱크 특성
  bool isVoltageSource;    // 전압원 여부 (예: 전원공급장치)
  bool isCurrentSource;    // 전류원 여부
  float sourceVoltage;     // 소스 전압 [V]
  float sourceCurrent;     // 소스 전류 [A]
  float sourceResistance;  // 소스 내부저항 [Ω]

  // 동적 특성
  float voltageHistory[3];  // 이전 3단계 전압 (수치해석용)
  float currentHistory[3];  // 이전 3단계 전류 (수치해석용)
  float responseTime;       // 응답시간 상수 [s]

  // 임피던스 특성 (누락된 멤버 추가)
  float inputImpedance[16];   // 입력 임피던스 [Ω] (포트별)
  float outputImpedance[16];  // 출력 임피던스 [Ω] (포트별)
} ElectricalNode;

// [추론] 전기 네트워크 엣지
// - 두 전기 노드 간의 저항성 연결 (옴의 법칙 적용)
// - 와이어의 물리적 특성(길이, 단면적, 재질)을 전기적 특성으로 변환
// - V = I × R 관계를 통해 전압강하 계산
typedef struct ElectricalEdge {
  NetworkEdgeBase base;  // 기본 엣지 정보

  // 전기적 특성
  float resistance;   // 저항 [Ω]
  float reactance;    // 리액턴스 [Ω] (AC 해석용)
  float impedance;    // 임피던스 [Ω] (|Z| = √(R² + X²))
  float conductance;  // 컨덕턴스 [S] (G = 1/R)

  // 물리적 특성 (와이어 특성으로부터 계산)
  float length;            // 와이어 길이 [mm]
  float crossSectionArea;  // 단면적 [mm²]
  float resistivity;       // 비저항 [Ω⋅mm] (재질별)
  float temperature;       // 온도 [°C] (저항 온도계수 적용)

  // 전류 특성
  float current;           // 엣지를 흐르는 전류 [A]
  float voltageDrop;       // 전압강하 [V] (V = I × R)
  float powerDissipation;  // 소모전력 [W] (P = I² × R)

  // 동적 특성
  float currentHistory[3];    // 이전 3단계 전류 (수치해석용)
  float maxCurrent;           // 최대 허용전류 [A] (과전류 보호)
  float thermalTimeConstant;  // 열적 시정수 [s]
} ElectricalEdge;

// [추론] 전기 네트워크 전체 구조
// - 키르히호프 법칙을 적용하기 위한 노드-엣지 그래프
// - 인접 리스트로 연결성 표현, 행렬 형태로 연립방정식 구성
// - 가우스-자이델 반복법으로 전압/전류 분포 계산
typedef struct ElectricalNetwork {
  // 네트워크 토폴로지
  ElectricalNode* nodes;  // 노드 배열
  ElectricalEdge* edges;  // 엣지 배열
  int nodeCount;          // 노드 개수
  int edgeCount;          // 엣지 개수
  int maxNodes;           // 최대 노드 수 (메모리 할당용)
  int maxEdges;           // 최대 엣지 수

  // 인접성 정보
  int** adjacencyMatrix;  // 인접 행렬 [nodeCount][nodeCount]
  int** incidenceMatrix;  // 접속 행렬 [nodeCount][edgeCount]
  int* nodeEdgeList;      // 각 노드별 연결된 엣지 리스트
  int* nodeEdgeCount;     // 각 노드별 연결된 엣지 개수

  // 수치해석 행렬 (키르히호프 방정식 해)
  float** conductanceMatrix;  // 컨덕턴스 행렬 G [nodeCount][nodeCount]
  float* currentVector;       // 전류 벡터 I [nodeCount]
  float* voltageVector;       // 전압 벡터 V [nodeCount] (해)

  // 기준점 및 경계조건
  int groundNodeId;           // 접지 노드 ID (V=0 기준)
  int* voltageSourceNodeIds;  // 전압원 노드 ID 목록
  int voltageSourceCount;     // 전압원 개수

  // 수렴 제어
  float convergenceTolerance;  // 수렴 허용오차
  int maxIterations;           // 최대 반복 횟수
  int currentIteration;        // 현재 반복 횟수
  bool isConverged;            // 수렴 여부
} ElectricalNetwork;

// ========== 공압 네트워크 구조 ==========

// [추론] 공압 네트워크 노드
// - 베르누이 방정식, 연속성 방정식 적용을 위한 유체역학적 특성
// - 각 노드는 압력, 온도, 밀도를 가지며 질량보존 법칙 적용
// - 압축성 유체(공기)의 상태방정식 (PV=nRT) 고려
typedef struct PneumaticNode {
  NetworkNodeBase base;  // 기본 노드 정보

  // 유체역학적 상태
  float pressure;       // 절대압력 [Pa] (gauge + atmospheric)
  float gaugePressure;  // 게이지압력 [bar] (절대압력 - 대기압)
  float temperature;    // 온도 [K]
  float density;        // 밀도 [kg/m³]
  float volume;         // 노드 등가 체적 [L]

  // 질량 및 에너지 보존
  float massFlowRate;      // 노드로 유입되는 총 질량유량 [kg/s]
  float netMassFlow;       // 순 질량유량 (연속성 방정식 체크용)
  float enthalpyFlowRate;  // 엔탈피 유량 [J/s] (에너지 보존)

  // 압축성 유체 특성
  float compressibility;  // 압축률 [Pa⁻¹]
  float soundSpeed;       // 음속 [m/s] (c = √(γRT))
  float machNumber;       // 마하수 (유속/음속)
  bool isChokedFlow;      // 초킹 유동 여부

  // 소스/싱크 특성
  bool isPressureSource;     // 압력원 여부 (예: 컴프레서)
  bool isMassFlowSource;     // 질량유량원 여부
  float sourcePressure;      // 소스 압력 [bar]
  float sourceMassFlowRate;  // 소스 질량유량 [kg/s]
  float sourceTemperature;   // 소스 온도 [K]

  // 동적 특성
  float pressureHistory[3];     // 이전 3단계 압력 (수치해석용)
  float temperatureHistory[3];  // 이전 3단계 온도 (수치해석용)
  float capacitance;            // 공압 커패시턴스 [kg/Pa⋅s²]
} PneumaticNode;

// [추론] 공압 네트워크 엣지
// - 두 공압 노드 간의 유동 특성 (베르누이 방정식 적용)
// - 배관/호스의 물리적 특성을 유체역학적 특성으로 변환
// - 층류/난류 판별, 압력손실 계산, 초킹 현상 고려
typedef struct PneumaticEdge {
  NetworkEdgeBase base;  // 기본 엣지 정보

  // 기하학적 특성
  float diameter;          // 내경 [mm]
  float length;            // 길이 [mm]
  float crossSectionArea;  // 단면적 [mm²]
  float roughness;         // 표면거칠기 [μm]
  float curvature;         // 곡률 (굽힘 손실용)

  // 유동 특성
  float massFlowRate;    // 질량유량 [kg/s]
  float volumeFlowRate;  // 체적유량 [L/min] @ 표준상태
  float velocity;        // 평균 유속 [m/s]
  float reynoldsNumber;  // 레이놀즈 수 (Re = ρvD/μ)
  float density;         // 유체 밀도 [kg/m³]

  // 압력 특성
  float pressureDrop;         // 압력강하 [Pa]
  float frictionFactor;       // 마찰계수 (f)
  float flowCoefficient;      // 유량계수 (Cv)
  float dischargeCoefficent;  // 유출계수 (Cd)

  // 베르누이 방정식 항목
  float staticPressureDrop;    // 정압 강하 [Pa]
  float dynamicPressureDrop;   // 동압 강하 [Pa] (½ρv²)
  float frictionPressureDrop;  // 마찰 압력강하 [Pa]
  float localPressureDrop;     // 국소 압력강하 [Pa] (굽힘, 확대/축소)

  // 열전달 특성
  float heatTransferCoefficient;  // 열전달계수 [W/m²⋅K]
  float wallTemperature;          // 관벽 온도 [K]
  float temperatureChange;        // 온도 변화 [K]

  // 동적 특성
  float massFlowHistory[3];  // 이전 3단계 질량유량 (수치해석용)
  float acousticDelay;       // 음향 지연시간 [s] (L/c)
  float responseTime;        // 응답시간 상수 [s]

  // 제한 특성
  float maxMassFlowRate;     // 최대 질량유량 [kg/s] (초킹 제한)
  float maxPressure;         // 최대 압력 [bar] (안전 제한)
  float cavitationPressure;  // 캐비테이션 한계압력 [bar]
} PneumaticEdge;

// [추론] 공압 네트워크 전체 구조
// - 연속성 방정식과 베르누이 방정식을 적용하기 위한 유체 네트워크
// - 압축성 유체의 상태방정식과 함께 비선형 연립방정식 구성
// - 뉴턴-랩슨 방법으로 압력/유량 분포 계산
typedef struct PneumaticNetwork {
  // 네트워크 토폴로지
  PneumaticNode* nodes;  // 노드 배열
  PneumaticEdge* edges;  // 엣지 배열
  int nodeCount;         // 노드 개수
  int edgeCount;         // 엣지 개수
  int maxNodes;          // 최대 노드 수
  int maxEdges;          // 최대 엣지 수

  // 인접성 정보
  int** adjacencyMatrix;  // 인접 행렬
  int** incidenceMatrix;  // 접속 행렬
  int* nodeEdgeList;      // 각 노드별 연결된 엣지 리스트
  int* nodeEdgeCount;     // 각 노드별 연결된 엣지 개수

  // 유체역학 행렬 (연속성 + 베르누이 방정식)
  float** massBalanceMatrix;       // 질량보존 행렬 [nodeCount][edgeCount]
  float** pressureEquationMatrix;  // 압력 방정식 행렬 [edgeCount][nodeCount]
  float* massFlowVector;           // 질량유량 벡터 [edgeCount] (해)
  float* pressureVector;           // 압력 벡터 [nodeCount] (해)

  // 경계조건
  int atmosphereNodeId;        // 대기압 기준 노드 ID
  int* pressureSourceNodeIds;  // 압력원 노드 ID 목록
  int pressureSourceCount;     // 압력원 개수

  // 환경 조건
  float atmosphericPressure;  // 대기압 [Pa]
  float ambientTemperature;   // 주변 온도 [K]
  float airMolecularWeight;   // 공기 분자량 [kg/mol]
  float gasConstant;          // 기체상수 R [J/mol⋅K]
  float specificHeatRatio;    // 비열비 γ (Cp/Cv)

  // 수렴 제어
  float convergenceTolerance;  // 수렴 허용오차
  int maxIterations;           // 최대 반복 횟수
  int currentIteration;        // 현재 반복 횟수
  bool isConverged;            // 수렴 여부
} PneumaticNetwork;

// ========== 기계 시스템 구조 ==========

// [추론] 기계 시스템 노드 (질점)
// - 뉴턴 역학 법칙 적용을 위한 질량-위치-속도 상태
// - 각 노드는 3차원 공간의 질점으로 F=ma 적용
// - 라그랑주 역학이나 해밀턴 역학으로 확장 가능
typedef struct MechanicalNode {
  NetworkNodeBase base;  // 기본 노드 정보

  // 운동학적 상태 (3차원)
  float position[3];      // 위치 [mm] (x, y, z)
  float velocity[3];      // 속도 [mm/s] (vx, vy, vz)
  float acceleration[3];  // 가속도 [mm/s²] (ax, ay, az)

  // 동역학적 특성
  float mass;                    // 질량 [kg]
  float momentOfInertia[3];      // 관성모멘트 [kg⋅mm²] (Ix, Iy, Iz)
  float angularPosition[3];      // 각위치 [rad] (θx, θy, θz)
  float angularVelocity[3];      // 각속도 [rad/s] (ωx, ωy, ωz)
  float angularAcceleration[3];  // 각가속도 [rad/s²] (αx, αy, αz)

  // 힘과 모멘트
  float appliedForce[3];    // 외력 [N] (Fx, Fy, Fz)
  float appliedMoment[3];   // 외부 모멘트 [N⋅mm] (Mx, My, Mz)
  float reactionForce[3];   // 반력 [N] (구속조건에 의한)
  float reactionMoment[3];  // 반모멘트 [N⋅mm]

  // 구속조건
  bool isFixed[6];               // 자유도 구속 [x,y,z,θx,θy,θz]
  float constraintStiffness[6];  // 구속 강성 [N/mm, N⋅mm/rad]
  float constraintDamping[6];    // 구속 댐핑 [N⋅s/mm, N⋅mm⋅s/rad]

  // 동적 특성
  float positionHistory[3][3];  // 이전 3단계 위치 (수치해석용)
  float velocityHistory[3][3];  // 이전 3단계 속도 (수치해석용)
  float naturalFrequency[6];    // 각 자유도별 고유진동수 [Hz]
  float dampingRatio[6];        // 각 자유도별 감쇠비
} MechanicalNode;

// [추론] 기계 시스템 엣지 (연결요소)
// - 두 질점 간의 탄성/점성 연결 (스프링-댐퍼)
// - 훅의 법칙 F = kx, 점성 댐핑 F = cv 적용
// - 비선형 스프링, 마찰력, 접촉력 등 복잡한 상호작용 모델링
typedef struct MechanicalEdge {
  NetworkEdgeBase base;  // 기본 엣지 정보

  // 연결 형태
  enum {
    MECHANICAL_SPRING,         // 스프링 연결
    MECHANICAL_DAMPER,         // 댐퍼 연결
    MECHANICAL_SPRING_DAMPER,  // 스프링-댐퍼 병렬
    MECHANICAL_RIGID,          // 강체 연결
    MECHANICAL_CONTACT,        // 접촉 (일방향)
    MECHANICAL_FRICTION        // 마찰 연결
  } connectionType;

  // 탄성 특성 (스프링)
  float springConstant[6];        // 스프링 상수 [N/mm, N⋅mm/rad]
  float naturalLength[6];         // 자연 길이/각도 [mm, rad]
  float preload[6];               // 예압 [N, N⋅mm]
  bool isNonlinearSpring;         // 비선형 스프링 여부
  float nonlinearCoefficient[6];  // 비선형 계수 (F = kx + k₂x² + k₃x³)

  // 댐핑 특성
  float dampingCoefficient[6];  // 댐핑 계수 [N⋅s/mm, N⋅mm⋅s/rad]
  bool isViscousDamping;        // 점성 댐핑 여부
  bool isCoulombDamping;        // 쿨롱 댐핑 여부 (마찰)
  float coulombFriction[6];     // 쿨롱 마찰계수

  // 현재 상태
  float displacement[6];      // 현재 변위 [mm, rad]
  float relativeVelocity[6];  // 상대 속도 [mm/s, rad/s]
  float springForce[6];       // 스프링 힘/모멘트 [N, N⋅mm]
  float damperForce[6];       // 댐퍼 힘/모멘트 [N, N⋅mm]
  float totalForce[6];        // 총 힘/모멘트 [N, N⋅mm]

  // 접촉/충돌 특성
  bool isInContact;        // 접촉 상태 여부
  float contactStiffness;  // 접촉 강성 [N/mm]
  float contactDamping;    // 접촉 댐핑 [N⋅s/mm]
  float penetrationDepth;  // 관입 깊이 [mm]
  float contactForce;      // 접촉력 [N]

  // 한계값
  float maxForce[6];         // 최대 허용 힘/모멘트 [N, N⋅mm]
  float maxDisplacement[6];  // 최대 허용 변위 [mm, rad]
  bool isYielding;           // 항복 여부
  bool isFracture;           // 파괴 여부

  // 동적 특성
  float forceHistory[6][3];  // 이전 3단계 힘 (수치해석용)
  float energyStored;        // 저장 에너지 [J] (½kx²)
  float energyDissipated;    // 소산 에너지 [J] (∫cv²dt)
} MechanicalEdge;

// [추론] 기계 시스템 전체 구조
// - 뉴턴 역학 F=ma를 적용하기 위한 질량-스프링-댐퍼 네트워크
// - 라그랑주 방정식 d/dt(∂L/∂q̇) - ∂L/∂q = Q로 확장 가능
// - 룽게-쿠타 방법으로 운동방정식 수치해석
typedef struct MechanicalSystem {
  // 네트워크 토폴로지
  MechanicalNode* nodes;  // 노드 배열
  MechanicalEdge* edges;  // 엣지 배열
  int nodeCount;          // 노드 개수
  int edgeCount;          // 엣지 개수
  int maxNodes;           // 최대 노드 수
  int maxEdges;           // 최대 엣지 수

  // 인접성 정보
  int** adjacencyMatrix;  // 인접 행렬
  int** incidenceMatrix;  // 접속 행렬
  int* nodeEdgeList;      // 각 노드별 연결된 엣지 리스트
  int* nodeEdgeCount;     // 각 노드별 연결된 엣지 개수

  // 동역학 행렬 (M q̈ + C q̇ + K q = F)
  float** massMatrix;         // 질량 행렬 M [6*nodeCount][6*nodeCount]
  float** dampingMatrix;      // 댐핑 행렬 C [6*nodeCount][6*nodeCount]
  float** stiffnessMatrix;    // 강성 행렬 K [6*nodeCount][6*nodeCount]
  float* displacementVector;  // 변위 벡터 q [6*nodeCount]
  float* velocityVector;      // 속도 벡터 q̇ [6*nodeCount]
  float* accelerationVector;  // 가속도 벡터 q̈ [6*nodeCount] (해)
  float* forceVector;         // 외력 벡터 F [6*nodeCount]

  // 중력 및 환경
  float gravity[3];   // 중력가속도 [m/s²] (gx, gy, gz)
  float airDensity;   // 공기밀도 [kg/m³] (공기저항용)
  float temperature;  // 환경온도 [°C] (재료특성용)

  // 시뮬레이션 제어
  float timeStep;     // 시간 간격 [s]
  float currentTime;  // 현재 시간 [s]
  int totalSteps;     // 총 시뮬레이션 단계 수
  int currentStep;    // 현재 단계

  // 안정성 제어
  float maxTimeStep;           // 최대 허용 시간 간격 [s] (안정성)
  float dampingStabilization;  // 안정화 댐핑 (수치적)
  bool isStable;               // 시스템 안정성 여부
} MechanicalSystem;

// ========== 네트워크 유틸리티 함수 포인터 ==========

// [추론] C언어 스타일 네트워크 관리 함수들
// - 각 네트워크의 생성, 해제, 업데이트를 위한 함수 포인터
// - 메모리 관리와 네트워크 토폴로지 변경을 안전하게 처리

// 전기 네트워크 관리 함수 포인터
typedef struct ElectricalNetworkFunctions {
  int (*Create)(ElectricalNetwork* network, int maxNodes, int maxEdges);
  int (*Destroy)(ElectricalNetwork* network);
  int (*AddNode)(ElectricalNetwork* network, const NetworkNodeBase* nodeBase,
                 const ElectricalNode* electricalNode);
  int (*RemoveNode)(ElectricalNetwork* network, int nodeId);
  int (*AddEdge)(ElectricalNetwork* network, const NetworkEdgeBase* edgeBase,
                 const ElectricalEdge* electricalEdge);
  int (*RemoveEdge)(ElectricalNetwork* network, int edgeId);
  int (*UpdateTopology)(ElectricalNetwork* network);
  int (*BuildMatrices)(ElectricalNetwork* network);
} ElectricalNetworkFunctions;

// 공압 네트워크 관리 함수 포인터
typedef struct PneumaticNetworkFunctions {
  int (*Create)(PneumaticNetwork* network, int maxNodes, int maxEdges);
  int (*Destroy)(PneumaticNetwork* network);
  int (*AddNode)(PneumaticNetwork* network, const NetworkNodeBase* nodeBase,
                 const PneumaticNode* pneumaticNode);
  int (*RemoveNode)(PneumaticNetwork* network, int nodeId);
  int (*AddEdge)(PneumaticNetwork* network, const NetworkEdgeBase* edgeBase,
                 const PneumaticEdge* pneumaticEdge);
  int (*RemoveEdge)(PneumaticNetwork* network, int edgeId);
  int (*UpdateTopology)(PneumaticNetwork* network);
  int (*BuildMatrices)(PneumaticNetwork* network);
} PneumaticNetworkFunctions;

// 기계 시스템 관리 함수 포인터
typedef struct MechanicalSystemFunctions {
  int (*Create)(MechanicalSystem* system, int maxNodes, int maxEdges);
  int (*Destroy)(MechanicalSystem* system);
  int (*AddNode)(MechanicalSystem* system, const NetworkNodeBase* nodeBase,
                 const MechanicalNode* mechanicalNode);
  int (*RemoveNode)(MechanicalSystem* system, int nodeId);
  int (*AddEdge)(MechanicalSystem* system, const NetworkEdgeBase* edgeBase,
                 const MechanicalEdge* mechanicalEdge);
  int (*RemoveEdge)(MechanicalSystem* system, int edgeId);
  int (*UpdateTopology)(MechanicalSystem* system);
  int (*BuildMatrices)(MechanicalSystem* system);
} MechanicalSystemFunctions;

}  // namespace plc
#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PHYSICS_PHYSICS_NETWORK_H_
