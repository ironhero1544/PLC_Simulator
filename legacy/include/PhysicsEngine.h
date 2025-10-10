/**
 * @file PhysicsEngine.h
 * @brief Comprehensive C-style physics engine interface for industrial PLC simulation
 * 
 * CRITICAL SYSTEM ARCHITECTURE:
 * This header defines a unified physics engine that replaces hardcoded simulation
 * logic with accurate, real-world physics calculations for electrical, pneumatic,
 * and mechanical systems in industrial automation environments.
 * 
 * CRASH PREVENTION DESIGN:
 * - C-style function pointers provide memory-safe modular interfaces
 * - Comprehensive error code system enables robust error handling
 * - Bounds checking and validation throughout all data structures
 * - Memory pool management prevents allocation failures
 * - State validation functions prevent invalid operations
 * 
 * INDUSTRIAL SAFETY FEATURES:
 * - Real-time performance monitoring and quality metrics
 * - Numerical convergence validation for stability
 * - Physical consistency checking
 * - Graceful degradation on component failures
 */

#pragma once

#include "PhysicsNetwork.h"
#include "PhysicsStates.h" 
#include "DataTypes.h"

namespace plc {

// ========== FORWARD DECLARATIONS ==========
struct PhysicsEngine;

// ========== PHYSICS ENGINE RESULTS AND STATISTICS ==========

/**
 * @brief Real-time simulation performance statistics for monitoring and optimization
 * 
 * CRITICAL PERFORMANCE MONITORING:
 * This structure provides comprehensive metrics for detecting performance issues
 * that could lead to real-time constraint violations in industrial systems.
 * 
 * MONITORING PURPOSE:
 * - Early detection of numerical instability
 * - Performance degradation analysis
 * - Memory usage tracking
 * - Convergence failure detection
 * - System health assessment
 */
typedef struct PhysicsPerformanceStats {
    // 계산 시간 통계
    float totalComputeTime;         // 총 계산시간 [ms]
    float electricalSolveTime;      // 전기 해석시간 [ms] 
    float pneumaticSolveTime;       // 공압 해석시간 [ms]
    float mechanicalSolveTime;      // 기계 해석시간 [ms]
    float networkBuildTime;         // 네트워크 구성시간 [ms]
    
    // 수렴성 통계
    int electricalIterations;       // 전기 솔버 반복 횟수
    int pneumaticIterations;        // 공압 솔버 반복 횟수
    bool electricalConverged;       // 전기 솔버 수렴 여부
    bool pneumaticConverged;        // 공압 솔버 수렴 여부
    bool mechanicalStable;          // 기계 시스템 안정성
    
    // 네트워크 크기 통계
    int electricalNodes;            // 전기 네트워크 노드 수
    int electricalEdges;            // 전기 네트워크 엣지 수
    int pneumaticNodes;             // 공압 네트워크 노드 수
    int pneumaticEdges;             // 공압 네트워크 엣지 수
    int mechanicalNodes;            // 기계 시스템 노드 수
    int mechanicalEdges;            // 기계 시스템 엣지 수
    
    // 품질 지표
    float electricalResidual;       // 전기 솔버 잔여오차
    float pneumaticResidual;        // 공압 솔버 잔여오차
    float mechanicalEnergy;         // 기계 시스템 총 에너지 [J]
    float powerConsumption;         // 총 소비전력 [W]
    float airConsumption;           // 총 공기소모량 [L/min]
    
    // 실시간 성능 
    float simulationFPS;            // 시뮬레이션 FPS
    float realTimeRatio;            // 실시간 대비 시뮬레이션 속도비
    float memoryUsage;              // 메모리 사용량 [MB]
} PhysicsPerformanceStats;

/**
 * @brief Comprehensive simulation results for application interface
 * 
 * HIGH-LEVEL INTERFACE:
 * This structure provides aggregated physics simulation results that the
 * application can safely consume without accessing low-level physics data.
 * 
 * DATA SAFETY FEATURES:
 * - Validation flag prevents use of invalid results
 * - Component count bounds checking
 * - Separate arrays for different physics domains
 * - Performance statistics included for monitoring
 */
typedef struct PhysicsSimulationResult {
    // 전체 시뮬레이션 상태
    bool isValid;                   // 결과 유효성 여부
    float simulationTime;           // 현재 시뮬레이션 시간 [s]
    float deltaTime;                // 시간 간격 [s]
    int stepCount;                  // 시뮬레이션 스텝 수
    
    // 컴포넌트별 물리 상태 (Application과의 인터페이스)
    TypedPhysicsState* componentStates; // 각 컴포넌트의 물리 상태
    int componentCount;             // 컴포넌트 개수
    
    // 네트워크 상태 요약
    float* nodeVoltages;            // 각 전기 노드의 전압 [V]
    float* nodeCurrents;            // 각 전기 노드의 전류 [A]
    float* nodePressures;           // 각 공압 노드의 압력 [bar]
    float* nodeMassFlows;           // 각 공압 노드의 질량유량 [kg/s]
    float* nodePositions;           // 각 기계 노드의 위치 [mm]
    float* nodeVelocities;          // 각 기계 노드의 속도 [mm/s]
    
    // 성능 통계
    PhysicsPerformanceStats stats;  // 성능 및 품질 지표
} PhysicsSimulationResult;

// ========== C-STYLE FUNCTION POINTER INTERFACES ==========

/**
 * @brief Physics engine lifecycle management with error handling
 * 
 * MEMORY SAFETY ARCHITECTURE:
 * C-style function pointers provide modular, extensible interfaces while
 * maintaining strict memory safety and error handling protocols.
 * 
 * LIFECYCLE SAFETY FEATURES:
 * - Structured initialization prevents partial state corruption
 * - Parameter validation prevents invalid configurations
 * - Resource cleanup prevents memory leaks
 * - Error propagation enables recovery strategies
 */
typedef struct PhysicsEngineLifecycle {
    // 생성 및 초기화
    int (*Create)(struct PhysicsEngine* engine, int maxComponents, int maxWires);
    int (*Initialize)(struct PhysicsEngine* engine);
    int (*Reset)(struct PhysicsEngine* engine);
    int (*Destroy)(struct PhysicsEngine* engine);
    
    // 설정 및 파라미터 조정
    int (*SetTimeStep)(struct PhysicsEngine* engine, float deltaTime);
    int (*SetConvergenceTolerance)(struct PhysicsEngine* engine, float tolerance);
    int (*SetMaxIterations)(struct PhysicsEngine* engine, int maxIter);
    int (*EnableLogging)(struct PhysicsEngine* engine, bool enable);
} PhysicsEngineLifecycle;

/**
 * @brief Network topology management with automated validation
 * 
 * CRITICAL NETWORK SAFETY:
 * Automatic network construction from wiring data with comprehensive
 * validation prevents invalid topologies that could cause simulation
 * instability or incorrect results.
 * 
 * TOPOLOGY VALIDATION:
 * - Connectivity verification prevents isolated components
 * - Physical consistency checking
 * - Automatic optimization for performance
 * - Error recovery for invalid configurations
 */
typedef struct PhysicsEngineNetworking {
    // 네트워크 구성 (배선으로부터 자동 생성)
    int (*BuildNetworksFromWiring)(struct PhysicsEngine* engine,
                                  const Wire* wires, int wireCount,
                                  const PlacedComponent* components, int componentCount);
    int (*UpdateNetworkTopology)(struct PhysicsEngine* engine);
    int (*OptimizeNetworks)(struct PhysicsEngine* engine);
    
    // 개별 네트워크 관리
    int (*RebuildElectricalNetwork)(struct PhysicsEngine* engine);
    int (*RebuildPneumaticNetwork)(struct PhysicsEngine* engine);
    int (*RebuildMechanicalSystem)(struct PhysicsEngine* engine);
    
    // 네트워크 검증
    int (*ValidateNetworks)(struct PhysicsEngine* engine);
    int (*CheckConnectivity)(struct PhysicsEngine* engine);
} PhysicsEngineNetworking;

/**
 * @brief Real-time simulation control with safety mechanisms
 * 
 * REAL-TIME SAFETY FEATURES:
 * Industrial automation requires deterministic, bounded execution times.
 * These functions provide controlled simulation execution with error
 * containment and recovery mechanisms.
 * 
 * EXECUTION SAFETY:
 * - Bounded execution time monitoring
 * - Numerical stability checking
 * - State preservation during failures
 * - Graceful degradation options
 */
typedef struct PhysicsEngineSimulation {
    // 시뮬레이션 제어
    int (*RunSimulation)(struct PhysicsEngine* engine, float deltaTime);
    int (*RunSingleStep)(struct PhysicsEngine* engine);
    int (*PauseSimulation)(struct PhysicsEngine* engine);
    int (*ResumeSimulation)(struct PhysicsEngine* engine);
    int (*StopSimulation)(struct PhysicsEngine* engine);
    
    // 개별 시스템 시뮬레이션
    int (*SolveElectricalSystem)(struct PhysicsEngine* engine, float deltaTime);
    int (*SolvePneumaticSystem)(struct PhysicsEngine* engine, float deltaTime);
    int (*SolveMechanicalSystem)(struct PhysicsEngine* engine, float deltaTime);
    
    // 상태 관리
    int (*SaveState)(struct PhysicsEngine* engine, void** stateData, int* stateSize);
    int (*RestoreState)(struct PhysicsEngine* engine, const void* stateData, int stateSize);
} PhysicsEngineSimulation;

/**
 * @brief Bidirectional data synchronization with validation
 * 
 * CRITICAL DATA INTEGRITY:
 * Safe data exchange between the application and physics engine prevents
 * state corruption that could cause unpredictable behavior or crashes.
 * 
 * SYNCHRONIZATION SAFETY:
 * - Bidirectional validation prevents inconsistent states
 * - Atomic updates prevent partial state corruption
 * - Type checking prevents invalid data
 * - Bounds checking prevents buffer overflows
 */
typedef struct PhysicsEngineDataInterface {
    // Application → PhysicsEngine (입력)
    int (*UpdateComponentInputs)(struct PhysicsEngine* engine,
                               int componentId, const void* inputData);
    int (*SetComponentState)(struct PhysicsEngine* engine,
                           int componentId, const TypedPhysicsState* state);
    int (*ApplyExternalForces)(struct PhysicsEngine* engine,
                             int componentId, const float* forces);
    
    // PhysicsEngine → Application (출력)
    int (*GetComponentState)(struct PhysicsEngine* engine,
                           int componentId, TypedPhysicsState* state);
    int (*GetSimulationResult)(struct PhysicsEngine* engine,
                             PhysicsSimulationResult* result);
    int (*GetNetworkStates)(struct PhysicsEngine* engine,
                          float* voltages, float* currents,
                          float* pressures, float* flows);
    
    // 대량 데이터 동기화
    int (*SyncAllComponentStates)(struct PhysicsEngine* engine, 
                                PlacedComponent* components, int count);
    int (*UpdateInternalStates)(struct PhysicsEngine* engine,
                              PlacedComponent* components, int count);
} PhysicsEngineDataInterface;

/**
 * @brief Comprehensive debugging and monitoring capabilities
 * 
 * DIAGNOSTIC SAFETY FEATURES:
 * Advanced debugging capabilities for troubleshooting physics simulation
 * issues without compromising system stability or performance.
 * 
 * MONITORING CAPABILITIES:
 * - Real-time performance profiling
 * - Network visualization for debugging
 * - Physical consistency validation
 * - Detailed error reporting and logging
 */
typedef struct PhysicsEngineDebugging {
    // 성능 모니터링
    int (*GetPerformanceStats)(struct PhysicsEngine* engine,
                             PhysicsPerformanceStats* stats);
    int (*ResetPerformanceCounters)(struct PhysicsEngine* engine);
    int (*EnableProfiling)(struct PhysicsEngine* engine, bool enable);
    
    // 네트워크 시각화 (디버깅용)
    int (*ExportNetworkGraph)(struct PhysicsEngine* engine,
                            const char* filename, const char* format);
    int (*PrintNetworkInfo)(struct PhysicsEngine* engine);
    int (*ValidatePhysicalConsistency)(struct PhysicsEngine* engine);
    
    // 로깅 및 진단
    int (*SetLogLevel)(struct PhysicsEngine* engine, int level);
    int (*EnableDetailedLogging)(struct PhysicsEngine* engine, bool enable);
    const char* (*GetLastError)(struct PhysicsEngine* engine);
} PhysicsEngineDebugging;

// ========== MAIN PHYSICS ENGINE STRUCTURE ==========

/**
 * @brief Unified physics engine - Central manager for all physics systems
 * 
 * CRITICAL ARCHITECTURE DESIGN:
 * This structure serves as the central coordination point for all physics
 * simulations, providing unified management of electrical, pneumatic, and
 * mechanical systems with comprehensive error handling and safety features.
 * 
 * SAFETY ARCHITECTURE:
 * - Modular C-style interfaces prevent function pointer corruption
 * - State validation functions prevent invalid operations
 * - Memory pool management prevents allocation failures
 * - Comprehensive error tracking and recovery
 * - Real-time performance monitoring
 * 
 * INDUSTRIAL REQUIREMENTS:
 * - Deterministic execution times for real-time systems
 * - Fault tolerance and graceful degradation
 * - Performance optimization for complex networks
 * - Debugging capabilities for troubleshooting
 */
typedef struct PhysicsEngine {
    // ========== 네트워크 시스템들 ==========
    ElectricalNetwork* electricalNetwork;    // 전기 네트워크
    PneumaticNetwork* pneumaticNetwork;      // 공압 네트워크  
    MechanicalSystem* mechanicalSystem;      // 기계 시스템
    
    // ========== 컴포넌트 물리 상태 관리 ==========
    TypedPhysicsState* componentPhysicsStates; // 각 컴포넌트의 물리 상태
    int* componentIdToPhysicsIndex;          // 컴포넌트 ID → 물리상태 인덱스 매핑
    int maxComponents;                       // 최대 컴포넌트 수
    int activeComponents;                    // 활성 컴포넌트 수
    
    // ========== NETWORK MAPPING TABLES ==========
    /**
     * CRITICAL MAPPING SYSTEM:
     * Safe translation between component ports and physics network nodes
     * prevents invalid access and ensures data consistency.
     * 
     * MAPPING SAFETY:
     * - Bounds checking prevents array access violations
     * - Validation prevents invalid component/port combinations
     * - Null pointer protection for unconnected ports
     */
    int** componentPortToElectricalNode;     // [componentId][portId] → electricalNodeId
    int** componentPortToPneumaticNode;      // [componentId][portId] → pneumaticNodeId  
    int** componentPortToMechanicalNode;     // [componentId][portId] → mechanicalNodeId
    
    // ========== 시뮬레이션 제어 ==========
    bool isInitialized;                     // 초기화 완료 여부
    bool isRunning;                         // 시뮬레이션 실행 상태
    bool isPaused;                          // 일시정지 상태
    float currentTime;                      // 현재 시뮬레이션 시간 [s]
    float deltaTime;                        // 시간 간격 [s]  
    int stepCount;                          // 총 시뮬레이션 스텝 수
    
    // ========== PHYSICS ENGINE READINESS STATE TRACKING ==========
    bool isNetworkBuilt;                    // 네트워크 구성 완료 여부
    bool isReadyForSimulation;              // 시뮬레이션 실행 준비 완료 여부
    bool isElectricalNetworkReady;          // 전기 네트워크 준비 완료 여부
    bool isPneumaticNetworkReady;           // 공압 네트워크 준비 완료 여부
    bool isMechanicalSystemReady;           // 기계 시스템 준비 완료 여부
    
    // ========== 시뮬레이션 파라미터 ==========
    float convergenceTolerance;             // 수렴 허용오차
    int maxIterations;                      // 최대 반복 횟수
    bool enableLogging;                     // 로깅 활성화 여부
    bool enableProfiling;                   // 프로파일링 활성화 여부
    int logLevel;                           // 로그 레벨 (0=ERROR, 1=WARN, 2=INFO, 3=DEBUG)
    
    // ========== 성능 통계 ==========
    PhysicsPerformanceStats performanceStats; // 성능 통계 데이터
    float performanceUpdateInterval;        // 성능 통계 업데이트 주기 [s]
    float lastPerformanceUpdate;            // 마지막 성능 업데이트 시간 [s]
    
    // ========== 에러 처리 ==========
    char lastError[256];                    // 마지막 에러 메시지
    int lastErrorCode;                      // 마지막 에러 코드
    bool hasError;                          // 에러 발생 여부
    
    // ========== STATE VALIDATION FUNCTION POINTERS ==========
    bool (*IsEngineReady)(struct PhysicsEngine* engine);           // 엔진 준비 상태 검증
    bool (*IsNetworkValid)(struct PhysicsEngine* engine);          // 네트워크 유효성 검증
    bool (*IsSafeToRunSimulation)(struct PhysicsEngine* engine);   // 시뮬레이션 실행 안전성 검증
    
    // ========== 메모리 관리 ==========
    void* memoryPool;                       // 메모리 풀 (성능 최적화용)
    int memoryPoolSize;                     // 메모리 풀 크기 [bytes]
    int memoryPoolUsed;                     // 사용된 메모리 크기 [bytes]
    
    // ========== C-STYLE FUNCTION POINTER INTERFACES ==========
    PhysicsEngineLifecycle lifecycle;       // 생명주기 관리 함수들
    PhysicsEngineNetworking networking;     // 네트워크 구성 함수들
    PhysicsEngineSimulation simulation;     // 시뮬레이션 제어 함수들
    PhysicsEngineDataInterface dataInterface; // 데이터 동기화 함수들  
    PhysicsEngineDebugging debugging;       // 디버깅 및 모니터링 함수들
    
} PhysicsEngine;

// ========== PHYSICS ENGINE FACTORY FUNCTIONS ==========

/**
 * @brief C-style factory functions with comprehensive error handling
 * 
 * FACTORY SAFETY FEATURES:
 * Safe memory management and initialization for physics engine creation
 * with different configuration presets for various use cases.
 * 
 * MEMORY SAFETY:
 * - Validated parameter checking prevents invalid configurations
 * - Proper cleanup on initialization failure
 * - Memory allocation validation
 * - Function pointer initialization verification
 * 
 * FACTORY PATTERNS:
 * - Default: Balanced performance and stability
 * - High Performance: Optimized for large networks
 * - Debugging: Enhanced diagnostics and logging
 */
extern "C" {
    // 물리엔진 생성 및 초기화
    PhysicsEngine* CreatePhysicsEngine(int maxComponents, int maxWires);
    int InitializePhysicsEngine(PhysicsEngine* engine);
    void DestroyPhysicsEngine(PhysicsEngine* engine);
    
    // 기본 설정으로 물리엔진 생성 (편의 함수)
    PhysicsEngine* CreateDefaultPhysicsEngine(void);
    
    // 고성능 설정으로 물리엔진 생성  
    PhysicsEngine* CreateHighPerformancePhysicsEngine(int maxComponents, int maxWires);
    
    // 디버깅 설정으로 물리엔진 생성
    PhysicsEngine* CreateDebuggingPhysicsEngine(int maxComponents, int maxWires);
}

// ========== PHYSICS ENGINE CONSTANTS AND CONFIGURATION ==========

/**
 * @brief Default configuration values for reliable operation
 * 
 * CONFIGURATION SAFETY:
 * These carefully chosen default values provide stable, reliable operation
 * while allowing customization for specific industrial requirements.
 * 
 * VALUE SELECTION CRITERIA:
 * - Industrial automation compatibility
 * - Numerical stability assurance
 * - Performance optimization
 * - Real-time constraint satisfaction
 */
namespace PhysicsEngineDefaults {
    // 기본 시뮬레이션 파라미터
    const float DEFAULT_TIME_STEP = 1.0f / 60.0f;        // 60 FPS
    const float DEFAULT_CONVERGENCE_TOLERANCE = 1e-6f;    // 1 ppm 정확도
    const int DEFAULT_MAX_ITERATIONS = 100;               // 최대 100회 반복
    const int DEFAULT_MAX_COMPONENTS = 100;               // 최대 100개 컴포넌트
    const int DEFAULT_MAX_WIRES = 500;                    // 최대 500개 와이어
    
    // 성능 관련 설정
    const float DEFAULT_PERFORMANCE_UPDATE_INTERVAL = 1.0f; // 1초마다 성능 업데이트
    const int DEFAULT_MEMORY_POOL_SIZE = 1024 * 1024;      // 1MB 메모리 풀
    const int DEFAULT_LOG_LEVEL = 2;                       // INFO 레벨
    
    // 물리적 상수들
    const float ATMOSPHERIC_PRESSURE = 101325.0f;         // 표준 대기압 [Pa]
    const float AIR_DENSITY_STP = 1.225f;                 // 표준상태 공기밀도 [kg/m³]
    const float AIR_VISCOSITY = 1.8e-5f;                  // 공기 점성계수 [Pa⋅s]
    const float GRAVITY = 9.81f;                          // 중력가속도 [m/s²]
    const float GAS_CONSTANT = 287.0f;                    // 공기 기체상수 [J/kg⋅K]
    const float SPECIFIC_HEAT_RATIO = 1.4f;               // 공기 비열비 [-]
}

// ========== ERROR CODE DEFINITIONS ==========

/**
 * @brief Comprehensive error code system for robust error handling
 * 
 * CRITICAL ERROR MANAGEMENT:
 * Systematic error codes enable proper error handling and recovery
 * strategies essential for industrial automation safety.
 * 
 * ERROR CODE CLASSIFICATION:
 * - 100s: Initialization and memory errors
 * - 200s: Network topology errors
 * - 300s: Simulation execution errors
 * - 400s: Data synchronization errors
 * - 500s: System-level errors
 * 
 * ERROR HANDLING STRATEGY:
 * Each error code corresponds to specific recovery procedures,
 * enabling graceful degradation and system stability maintenance.
 */
typedef enum PhysicsEngineError {
    PHYSICS_ENGINE_SUCCESS = 0,                // 성공
    
    // 초기화 및 메모리 관련 에러 (100번대)
    PHYSICS_ENGINE_ERROR_INITIALIZATION = 100, // 초기화 실패
    PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,    // 메모리 할당 실패  
    PHYSICS_ENGINE_ERROR_INVALID_PARAMETER,    // 잘못된 파라미터
    PHYSICS_ENGINE_ERROR_NOT_INITIALIZED,      // 초기화되지 않음
    
    // 네트워크 구성 관련 에러 (200번대)
    PHYSICS_ENGINE_ERROR_NETWORK_BUILD = 200,  // 네트워크 구성 실패
    PHYSICS_ENGINE_ERROR_INVALID_TOPOLOGY,     // 잘못된 토폴로지
    PHYSICS_ENGINE_ERROR_DISCONNECTED_NETWORK, // 연결되지 않은 네트워크
    PHYSICS_ENGINE_ERROR_COMPONENT_NOT_FOUND,  // 컴포넌트를 찾을 수 없음
    
    // 시뮬레이션 관련 에러 (300번대)  
    PHYSICS_ENGINE_ERROR_SIMULATION_FAILED = 300, // 시뮬레이션 실패
    PHYSICS_ENGINE_ERROR_CONVERGENCE_FAILED,      // 수렴 실패
    PHYSICS_ENGINE_ERROR_NUMERICAL_INSTABILITY,   // 수치적 불안정성
    PHYSICS_ENGINE_ERROR_PHYSICAL_INCONSISTENCY,  // 물리적 모순
    
    // 데이터 관련 에러 (400번대)
    PHYSICS_ENGINE_ERROR_DATA_SYNC = 400,      // 데이터 동기화 실패
    PHYSICS_ENGINE_ERROR_INVALID_STATE,        // 잘못된 상태
    PHYSICS_ENGINE_ERROR_BUFFER_OVERFLOW,      // 버퍼 오버플로우
    PHYSICS_ENGINE_ERROR_TYPE_MISMATCH,        // 타입 불일치
    
    // 시스템 관련 에러 (500번대)
    PHYSICS_ENGINE_ERROR_SYSTEM = 500,         // 시스템 에러
    PHYSICS_ENGINE_ERROR_THREAD_FAILURE,       // 쓰레드 실패
    PHYSICS_ENGINE_ERROR_IO_ERROR,             // 입출력 에러
    PHYSICS_ENGINE_ERROR_UNKNOWN               // 알 수 없는 에러
} PhysicsEngineError;

/**
 * @brief Error code to string conversion utility
 * @param error Error code to convert
 * @return Human-readable error description
 * 
 * DEBUGGING SUPPORT:
 * Provides clear, actionable error messages for troubleshooting
 * and maintenance in industrial environments.
 */
const char* PhysicsEngineErrorToString(PhysicsEngineError error);

} // namespace plc