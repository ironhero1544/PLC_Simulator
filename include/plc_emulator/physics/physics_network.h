/*
 * physics_network.h
 *
 * 물리 도메인 네트워크 구조체 정의.
 * Network structure definitions for physics domains.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PHYSICS_PHYSICS_NETWORK_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PHYSICS_PHYSICS_NETWORK_H_

#include "plc_emulator/core/data_types.h"
#include "plc_emulator/physics/physics_states.h"

namespace plc {


/*
 * 공통 네트워크 노드 식별 정보.
 * Common network node identity data.
 */
typedef struct NetworkNodeBase {
  int componentId;
  int portId;
  int nodeId;
  bool isActive;
} NetworkNodeBase;

/*
 * 공통 네트워크 엣지 식별 정보.
 * Common network edge identity data.
 */
typedef struct NetworkEdgeBase {
  int edgeId;
  int fromNodeId;
  int toNodeId;
  int wireId;
  bool isActive;
} NetworkEdgeBase;


/*
 * 전기 네트워크 노드 상태.
 * Electrical network node state.
 */
typedef struct ElectricalNode {
  NetworkNodeBase base;

  float voltage;
  float current;
  float netCurrent;

  float nodeCapacitance;
  float nodeInductance;
  float leakageConductance;

  bool isVoltageSource;
  bool isCurrentSource;
  float sourceVoltage;
  float sourceCurrent;
  float sourceResistance;

  float voltageHistory[3];
  float currentHistory[3];
  float responseTime;

  float inputImpedance[16];
  float outputImpedance[16];
} ElectricalNode;

/*
 * 전기 네트워크 엣지 상태.
 * Electrical network edge state.
 */
typedef struct ElectricalEdge {
  NetworkEdgeBase base;

  float resistance;
  float reactance;
  float impedance;
  float conductance;

  float length;
  float crossSectionArea;
  float resistivity;
  float temperature;

  float current;
  float voltageDrop;
  float powerDissipation;

  float currentHistory[3];
  float maxCurrent;
  float thermalTimeConstant;
} ElectricalEdge;

/*
 * 전기 네트워크 그래프.
 * Electrical network graph container.
 */
typedef struct ElectricalNetwork {
  ElectricalNode* nodes;
  ElectricalEdge* edges;
  int nodeCount;
  int edgeCount;
  int maxNodes;
  int maxEdges;

  int** adjacencyMatrix;
  int** incidenceMatrix;
  int* nodeEdgeList;
  int* nodeEdgeCount;

  float** conductanceMatrix;
  float* currentVector;
  float* voltageVector;

  int groundNodeId;
  int* voltageSourceNodeIds;
  int voltageSourceCount;

  float convergenceTolerance;
  int maxIterations;
  int currentIteration;
  bool isConverged;
} ElectricalNetwork;


/*
 * 공압 네트워크 노드 상태.
 * Pneumatic network node state.
 */
typedef struct PneumaticNode {
  NetworkNodeBase base;

  float pressure;
  float gaugePressure;
  float temperature;
  float density;
  float volume;

  float massFlowRate;
  float netMassFlow;
  float enthalpyFlowRate;

  float compressibility;
  float soundSpeed;
  float machNumber;
  bool isChokedFlow;

  bool isPressureSource;
  bool isMassFlowSource;
  float sourcePressure;
  float sourceMassFlowRate;
  float sourceTemperature;

  float pressureHistory[3];
  float temperatureHistory[3];
  float capacitance;
} PneumaticNode;

/*
 * 공압 네트워크 엣지 상태.
 * Pneumatic network edge state.
 */
typedef struct PneumaticEdge {
  NetworkEdgeBase base;

  float diameter;
  float length;
  float crossSectionArea;
  float roughness;
  float curvature;

  float massFlowRate;
  float volumeFlowRate;
  float velocity;
  float reynoldsNumber;
  float density;

  float pressureDrop;
  float frictionFactor;
  float flowCoefficient;
  float dischargeCoefficent;

  float staticPressureDrop;
  float dynamicPressureDrop;
  float frictionPressureDrop;
  float localPressureDrop;

  float heatTransferCoefficient;
  float wallTemperature;
  float temperatureChange;

  float massFlowHistory[3];
  float acousticDelay;
  float responseTime;

  float maxMassFlowRate;
  float maxPressure;
  float cavitationPressure;
} PneumaticEdge;

/*
 * 공압 네트워크 그래프.
 * Pneumatic network graph container.
 */
typedef struct PneumaticNetwork {
  PneumaticNode* nodes;
  PneumaticEdge* edges;
  int nodeCount;
  int edgeCount;
  int maxNodes;
  int maxEdges;

  int** adjacencyMatrix;
  int** incidenceMatrix;
  int* nodeEdgeList;
  int* nodeEdgeCount;

  float** massBalanceMatrix;
  float** pressureEquationMatrix;
  float* massFlowVector;
  float* pressureVector;

  int atmosphereNodeId;
  int* pressureSourceNodeIds;
  int pressureSourceCount;

  float atmosphericPressure;
  float ambientTemperature;
  float airMolecularWeight;
  float gasConstant;
  float specificHeatRatio;

  float convergenceTolerance;
  int maxIterations;
  int currentIteration;
  bool isConverged;
} PneumaticNetwork;


/*
 * 기계 네트워크 노드 상태.
 * Mechanical network node state.
 */
typedef struct MechanicalNode {
  NetworkNodeBase base;

  float position[3];
  float velocity[3];
  float acceleration[3];

  float mass;
  float momentOfInertia[3];
  float angularPosition[3];
  float angularVelocity[3];
  float angularAcceleration[3];

  float appliedForce[3];
  float appliedMoment[3];
  float reactionForce[3];
  float reactionMoment[3];

  bool isFixed[6];
  float constraintStiffness[6];
  float constraintDamping[6];

  float positionHistory[3][3];
  float velocityHistory[3][3];
  float naturalFrequency[6];
  float dampingRatio[6];
} MechanicalNode;

/*
 * 기계 네트워크 엣지 상태.
 * Mechanical network edge state.
 */
typedef struct MechanicalEdge {
  NetworkEdgeBase base;

  enum {
    MECHANICAL_SPRING,
    MECHANICAL_DAMPER,
    MECHANICAL_SPRING_DAMPER,
    MECHANICAL_RIGID,
    MECHANICAL_CONTACT,
    MECHANICAL_FRICTION
  } connectionType;

  float springConstant[6];
  float naturalLength[6];
  float preload[6];
  bool isNonlinearSpring;
  float nonlinearCoefficient[6];

  float dampingCoefficient[6];
  bool isViscousDamping;
  bool isCoulombDamping;
  float coulombFriction[6];

  float displacement[6];
  float relativeVelocity[6];
  float springForce[6];
  float damperForce[6];
  float totalForce[6];

  bool isInContact;
  float contactStiffness;
  float contactDamping;
  float penetrationDepth;
  float contactForce;

  float maxForce[6];
  float maxDisplacement[6];
  bool isYielding;
  bool isFracture;

  float forceHistory[6][3];
  float energyStored;
  float energyDissipated;
} MechanicalEdge;

/*
 * 기계 네트워크 그래프.
 * Mechanical network graph container.
 */
typedef struct MechanicalSystem {
  MechanicalNode* nodes;
  MechanicalEdge* edges;
  int nodeCount;
  int edgeCount;
  int maxNodes;
  int maxEdges;

  int** adjacencyMatrix;
  int** incidenceMatrix;
  int* nodeEdgeList;
  int* nodeEdgeCount;

  float** massMatrix;
  float** dampingMatrix;
  float** stiffnessMatrix;
  float* displacementVector;
  float* velocityVector;
  float* accelerationVector;
  float* forceVector;

  float gravity[3];
  float airDensity;
  float temperature;

  float timeStep;
  float currentTime;
  int totalSteps;
  int currentStep;

  float maxTimeStep;
  float dampingStabilization;
  bool isStable;
} MechanicalSystem;



/*
 * 전기 네트워크 관리 함수 테이블.
 * Function table for electrical network management.
 */
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

/*
 * 공압 네트워크 관리 함수 테이블.
 * Function table for pneumatic network management.
 */
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

/*
 * 기계 네트워크 관리 함수 테이블.
 * Function table for mechanical system management.
 */
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

}  /* namespace plc */
#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PHYSICS_PHYSICS_NETWORK_H_ */
