// physics_engine.h
//
// Multi-domain physics simulation engine.
// 다중 도메인 물리 시뮬레이션 엔진.

/**
 * @file PhysicsEngine.h
 * @brief Comprehensive C-style physics engine interface for industrial PLC
 * simulation
 * 산업용 PLC 시뮬레이션을 위한 포괄적인 C 스타일 물리 엔진 인터페이스
 *
 * CRITICAL SYSTEM ARCHITECTURE:
 * This header defines a unified physics engine that replaces hardcoded
 * simulation logic with accurate, real-world physics calculations for
 * electrical, pneumatic, and mechanical systems in industrial automation
 * environments.
 * 핵심 시스템 아키텍처:
 * 이 헤더는 하드코딩된 시뮬레이션 로직을 산업 자동화 환경의 전기, 공압, 기계 시스템에 대한
 * 정확한 실제 물리 계산으로 대체하는 통합 물리 엔진을 정의합니다.
 *
 * CRASH PREVENTION DESIGN:
 * - C-style function pointers provide memory-safe modular interfaces
 * - Comprehensive error code system enables robust error handling
 * - Bounds checking and validation throughout all data structures
 * - Memory pool management prevents allocation failures
 * - State validation functions prevent invalid operations
 * 충돌 방지 설계:
 * - C 스타일 함수 포인터는 메모리 안전 모듈식 인터페이스를 제공합니다.
 * - 포괄적인 오류 코드 시스템으로 강력한 오류 처리가 가능합니다.
 * - 모든 데이터 구조에 걸친 경계 검사 및 유효성 검증
 * - 메모리 풀 관리로 할당 실패를 방지합니다.
 * - 상태 유효성 검증 함수로 잘못된 연산을 방지합니다.
 *
 * INDUSTRIAL SAFETY FEATURES:
 * - Real-time performance monitoring and quality metrics
 * - Numerical convergence validation for stability
 * - Physical consistency checking
 * - Graceful degradation on component failures
 * 산업 안전 기능:
 * - 실시간 성능 모니터링 및 품질 메트릭
 * - 안정성을 위한 수치 수렴 유효성 검증
 * - 물리적 일관성 검사
 * - 구성 요소 장애 시 정상적인 성능 저하
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PHYSICS_PHYSICS_ENGINE_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PHYSICS_PHYSICS_ENGINE_H_

#include "plc_emulator/core/data_types.h"
#include "plc_emulator/physics/physics_network.h"
#include "plc_emulator/physics/physics_states.h"

namespace plc {

    struct PhysicsEngine;


/**
 * @brief Real-time simulation performance statistics for monitoring and
 * optimization
 * 모니터링 및 최적화를 위한 실시간 시뮬레이션 성능 통계
 *
 * CRITICAL PERFORMANCE MONITORING:
 * This structure provides comprehensive metrics for detecting performance
 * issues that could lead to real-time constraint violations in industrial
 * systems.
 * 중요 성능 모니터링:
 * 이 구조체는 산업 시스템에서 실시간 제약 조건 위반으로 이어질 수 있는 성능 문제를
 * 감지하기 위한 포괄적인 메트릭을 제공합니다.
 *
 * MONITORING PURPOSE:
 * - Early detection of numerical instability
 * - Performance degradation analysis
 * - Memory usage tracking
 * - Convergence failure detection
 * - System health assessment
 * 모니터링 목적:
 * - 수치적 불안정성의 조기 발견
 * - 성능 저하 분석
 * - 메모리 사용량 추적
 * - 수렴 실패 감지
 * - 시스템 상태 평가
 */
    typedef struct PhysicsPerformanceStats {
        // Computation time statistics
        // 계산 시간 통계
        float totalComputeTime;     // Total computation time [ms] // 총 계산시간 [ms]
        float electricalSolveTime;  // Electrical solve time [ms] // 전기 해석시간 [ms]
        float pneumaticSolveTime;   // Pneumatic solve time [ms] // 공압 해석시간 [ms]
        float mechanicalSolveTime;  // Mechanical solve time [ms] // 기계 해석시간 [ms]
        float networkBuildTime;     // Network build time [ms] // 네트워크 구성시간 [ms]

        // Convergence statistics
        // 수렴성 통계
        int electricalIterations;  // Number of electrical solver iterations // 전기 솔버 반복 횟수
        int pneumaticIterations;   // Number of pneumatic solver iterations // 공압 솔버 반복 횟수
        bool electricalConverged;  // Whether electrical solver converged // 전기 솔버 수렴 여부
        bool pneumaticConverged;   // Whether pneumatic solver converged // 공압 솔버 수렴 여부
        bool mechanicalStable;     // Mechanical system stability // 기계 시스템 안정성

        // Network size statistics
        // 네트워크 크기 통계
        int electricalNodes;  // Number of electrical network nodes // 전기 네트워크 노드 수
        int electricalEdges;  // Number of electrical network edges // 전기 네트워크 엣지 수
        int pneumaticNodes;   // Number of pneumatic network nodes // 공압 네트워크 노드 수
        int pneumaticEdges;   // Number of pneumatic network edges // 공압 네트워크 엣지 수
        int mechanicalNodes;  // Number of mechanical system nodes // 기계 시스템 노드 수
        int mechanicalEdges;  // Number of mechanical system edges // 기계 시스템 엣지 수

        // Quality metrics
        // 품질 지표
        float electricalResidual;  // Electrical solver residual error // 전기 솔버 잔여오차
        float pneumaticResidual;   // Pneumatic solver residual error // 공압 솔버 잔여오차
        float mechanicalEnergy;    // Total mechanical system energy [J] // 기계 시스템 총 에너지 [J]
        float powerConsumption;    // Total power consumption [W] // 총 소비전력 [W]
        float airConsumption;      // Total air consumption [L/min] // 총 공기소모량 [L/min]

        // Real-time performance
        // 실시간 성능
        float simulationFPS;  // Simulation FPS // 시뮬레이션 FPS
        float realTimeRatio;  // Simulation speed ratio to real-time // 실시간 대비 시뮬레이션 속도비
        float memoryUsage;    // Memory usage [MB] // 메모리 사용량 [MB]
    } PhysicsPerformanceStats;

/**
 * @brief Comprehensive simulation results for application interface
 * 애플리케이션 인터페이스를 위한 포괄적인 시뮬레이션 결과
 *
 * HIGH-LEVEL INTERFACE:
 * This structure provides aggregated physics simulation results that the
 * application can safely consume without accessing low-level physics data.
 * 상위 수준 인터페이스:
 * 이 구조체는 애플리케이션이 하위 수준 물리 데이터에 접근하지 않고도 안전하게 사용할 수 있는
 * 집계된 물리 시뮬레이션 결과를 제공합니다.
 *
 * DATA SAFETY FEATURES:
 * - Validation flag prevents use of invalid results
 * - Component count bounds checking
 * - Separate arrays for different physics domains
 * - Performance statistics included for monitoring
 * 데이터 안전 기능:
 * - 유효성 검사 플래그는 잘못된 결과의 사용을 방지합니다.
 * - 컴포넌트 수 경계 검사
 * - 다른 물리 도메인을 위한 별도의 배열
 * - 모니터링을 위한 성능 통계 포함
 */
    typedef struct PhysicsSimulationResult {
        // Overall simulation state
        // 전체 시뮬레이션 상태
        bool isValid;          // Result validity flag // 결과 유효성 여부
        float simulationTime;  // Current simulation time [s] // 현재 시뮬레이션 시간 [s]
        float deltaTime;       // Time step [s] // 시간 간격 [s]
        int stepCount;         // Simulation step count // 시뮬레이션 스텝 수

        // Per-component physical states (Interface with Application)
        // 컴포넌트별 물리 상태 (Application과의 인터페이스)
        TypedPhysicsState* componentStates;  // Physical state of each component // 각 컴포넌트의 물리 상태
        int componentCount;                  // Number of components // 컴포넌트 개수

        // Network state summary
        // 네트워크 상태 요약
        float* nodeVoltages;    // Voltage of each electrical node [V] // 각 전기 노드의 전압 [V]
        float* nodeCurrents;    // Current of each electrical node [A] // 각 전기 노드의 전류 [A]
        float* nodePressures;   // Pressure of each pneumatic node [bar] // 각 공압 노드의 압력 [bar]
        float* nodeMassFlows;   // Mass flow of each pneumatic node [kg/s] // 각 공압 노드의 질량유량 [kg/s]
        float* nodePositions;   // Position of each mechanical node [mm] // 각 기계 노드의 위치 [mm]
        float* nodeVelocities;  // Velocity of each mechanical node [mm/s] // 각 기계 노드의 속도 [mm/s]

        // Performance statistics
        // 성능 통계
        PhysicsPerformanceStats stats;  // Performance and quality metrics // 성능 및 품질 지표
    } PhysicsSimulationResult;


/**
 * @brief Physics engine lifecycle management with error handling
 * 오류 처리를 포함한 물리 엔진 생명주기 관리
 *
 * MEMORY SAFETY ARCHITECTURE:
 * C-style function pointers provide modular, extensible interfaces while
 * maintaining strict memory safety and error handling protocols.
 * 메모리 안전 아키텍처:
 * C 스타일 함수 포인터는 엄격한 메모리 안전 및 오류 처리 프로토콜을 유지하면서
 * 모듈식의 확장 가능한 인터페이스를 제공합니다.
 *
 * LIFECYCLE SAFETY FEATURES:
 * - Structured initialization prevents partial state corruption
 * - Parameter validation prevents invalid configurations
 * - Resource cleanup prevents memory leaks
 * - Error propagation enables recovery strategies
 * 생명주기 안전 기능:
 * - 구조화된 초기화는 부분적인 상태 손상을 방지합니다.
 * - 파라미터 유효성 검증은 잘못된 구성을 방지합니다.
 * - 리소스 정리는 메모리 누수를 방지합니다.
 * - 오류 전파는 복구 전략을 가능하게 합니다.
 */
    typedef struct PhysicsEngineLifecycle {
        // Creation and initialization
        // 생성 및 초기화
        int (*Create)(struct PhysicsEngine* engine, int maxComponents, int maxWires);
        int (*Initialize)(struct PhysicsEngine* engine);
        int (*Reset)(struct PhysicsEngine* engine);
        int (*Destroy)(struct PhysicsEngine* engine);

        // Configuration and parameter adjustment
        // 설정 및 파라미터 조정
        int (*SetTimeStep)(struct PhysicsEngine* engine, float deltaTime);
        int (*SetConvergenceTolerance)(struct PhysicsEngine* engine, float tolerance);
        int (*SetMaxIterations)(struct PhysicsEngine* engine, int maxIter);
        int (*EnableLogging)(struct PhysicsEngine* engine, bool enable);
    } PhysicsEngineLifecycle;

/**
 * @brief Network topology management with automated validation
 * 자동 유효성 검증을 통한 네트워크 토폴로지 관리
 *
 * CRITICAL NETWORK SAFETY:
 * Automatic network construction from wiring data with comprehensive
 * validation prevents invalid topologies that could cause simulation
 * instability or incorrect results.
 * 중요 네트워크 안전:
 * 배선 데이터로부터 포괄적인 유효성 검증을 통한 자동 네트워크 구성은
 * 시뮬레이션 불안정성이나 부정확한 결과를 초래할 수 있는 잘못된 토폴로지를 방지합니다.
 *
 * TOPOLOGY VALIDATION:
 * - Connectivity verification prevents isolated components
 * - Physical consistency checking
 * - Automatic optimization for performance
 * - Error recovery for invalid configurations
 * 토폴로지 유효성 검증:
 * - 연결성 검증은 고립된 컴포넌트를 방지합니다.
 * - 물리적 일관성 검사
 * - 성능을 위한 자동 최적화
 * - 잘못된 구성에 대한 오류 복구
 */
    typedef struct PhysicsEngineNetworking {
        // Network construction (auto-generated from wiring)
        // 네트워크 구성 (배선으로부터 자동 생성)
        int (*BuildNetworksFromWiring)(struct PhysicsEngine* engine,
                                       const Wire* wires, int wireCount,
                                       const PlacedComponent* components,
                                       int componentCount);
        int (*UpdateNetworkTopology)(struct PhysicsEngine* engine);
        int (*OptimizeNetworks)(struct PhysicsEngine* engine);

        // Individual network management
        // 개별 네트워크 관리
        int (*RebuildElectricalNetwork)(struct PhysicsEngine* engine);
        int (*RebuildPneumaticNetwork)(struct PhysicsEngine* engine);
        int (*RebuildMechanicalSystem)(struct PhysicsEngine* engine);

        // Network validation
        // 네트워크 검증
        int (*ValidateNetworks)(struct PhysicsEngine* engine);
        int (*CheckConnectivity)(struct PhysicsEngine* engine);
    } PhysicsEngineNetworking;

/**
 * @brief Real-time simulation control with safety mechanisms
 * 안전 메커니즘을 갖춘 실시간 시뮬레이션 제어
 *
 * REAL-TIME SAFETY FEATURES:
 * Industrial automation requires deterministic, bounded execution times.
 * These functions provide controlled simulation execution with error
 * containment and recovery mechanisms.
 * 실시간 안전 기능:
 * 산업 자동화는 결정론적이고 제한된 실행 시간을 요구합니다.
 * 이러한 함수들은 오류 억제 및 복구 메커니즘을 통해 제어된 시뮬레이션 실행을 제공합니다.
 *
 * EXECUTION SAFETY:
 * - Bounded execution time monitoring
 * - Numerical stability checking
 * - State preservation during failures
 * - Graceful degradation options
 * 실행 안전:
 * - 제한된 실행 시간 모니터링
 * - 수치적 안정성 검사
 * - 실패 시 상태 보존
 * - 정상적인 성능 저하 옵션
 */
    typedef struct PhysicsEngineSimulation {
        // Simulation control
        // 시뮬레이션 제어
        int (*RunSimulation)(struct PhysicsEngine* engine, float deltaTime);
        int (*RunSingleStep)(struct PhysicsEngine* engine);
        int (*PauseSimulation)(struct PhysicsEngine* engine);
        int (*ResumeSimulation)(struct PhysicsEngine* engine);
        int (*StopSimulation)(struct PhysicsEngine* engine);

        // Individual system simulation
        // 개별 시스템 시뮬레이션
        int (*SolveElectricalSystem)(struct PhysicsEngine* engine, float deltaTime);
        int (*SolvePneumaticSystem)(struct PhysicsEngine* engine, float deltaTime);
        int (*SolveMechanicalSystem)(struct PhysicsEngine* engine, float deltaTime);

        // State management
        // 상태 관리
        int (*SaveState)(struct PhysicsEngine* engine, void** stateData,
                         int* stateSize);
        int (*RestoreState)(struct PhysicsEngine* engine, const void* stateData,
                            int stateSize);
    } PhysicsEngineSimulation;

/**
 * @brief Bidirectional data synchronization with validation
 * 유효성 검증을 통한 양방향 데이터 동기화
 *
 * CRITICAL DATA INTEGRITY:
 * Safe data exchange between the application and physics engine prevents
 * state corruption that could cause unpredictable behavior or crashes.
 * 중요 데이터 무결성:
 * 애플리케이션과 물리 엔진 간의 안전한 데이터 교환은 예측 불가능한 동작이나
 * 충돌을 유발할 수 있는 상태 손상을 방지합니다.
 *
 * SYNCHRONIZATION SAFETY:
 * - Bidirectional validation prevents inconsistent states
 * - Atomic updates prevent partial state corruption
 * - Type checking prevents invalid data
 * - Bounds checking prevents buffer overflows
 * 동기화 안전:
 * - 양방향 유효성 검증은 일관성 없는 상태를 방지합니다.
 * - 원자적 업데이트는 부분적인 상태 손상을 방지합니다.
 * - 타입 검사는 잘못된 데이터를 방지합니다.
 * - 경계 검사는 버퍼 오버플로우를 방지합니다.
 */
    typedef struct PhysicsEngineDataInterface {
        // Application → PhysicsEngine (Input)
        // Application → PhysicsEngine (입력)
        int (*UpdateComponentInputs)(struct PhysicsEngine* engine, int componentId,
                                     const void* inputData);
        int (*SetComponentState)(struct PhysicsEngine* engine, int componentId,
                                 const TypedPhysicsState* state);
        int (*ApplyExternalForces)(struct PhysicsEngine* engine, int componentId,
                                   const float* forces);

        // PhysicsEngine → Application (Output)
        // PhysicsEngine → Application (출력)
        int (*GetComponentState)(struct PhysicsEngine* engine, int componentId,
                                 TypedPhysicsState* state);
        int (*GetSimulationResult)(struct PhysicsEngine* engine,
                                   PhysicsSimulationResult* result);
        int (*GetNetworkStates)(struct PhysicsEngine* engine, float* voltages,
                                float* currents, float* pressures, float* flows);

        // Bulk data synchronization
        // 대량 데이터 동기화
        int (*SyncAllComponentStates)(struct PhysicsEngine* engine,
                                      PlacedComponent* components, int count);
        int (*UpdateInternalStates)(struct PhysicsEngine* engine,
                                    PlacedComponent* components, int count);
    } PhysicsEngineDataInterface;

/**
 * @brief Comprehensive debugging and monitoring capabilities
 * 포괄적인 디버깅 및 모니터링 기능
 *
 * DIAGNOSTIC SAFETY FEATURES:
 * Advanced debugging capabilities for troubleshooting physics simulation
 * issues without compromising system stability or performance.
 * 진단 안전 기능:
 * 시스템 안정성이나 성능을 저하시키지 않으면서 물리 시뮬레이션 문제를 해결하기 위한
 * 고급 디버깅 기능입니다.
 *
 * MONITORING CAPABILITIES:
 * - Real-time performance profiling
 * - Network visualization for debugging
 * - Physical consistency validation
 * - Detailed error reporting and logging
 * 모니터링 기능:
 * - 실시간 성능 프로파일링
 * - 디버깅을 위한 네트워크 시각화
 * - 물리적 일관성 유효성 검증
 * - 상세한 오류 보고 및 로깅
 */
    typedef struct PhysicsEngineDebugging {
        // Performance monitoring
        // 성능 모니터링
        int (*GetPerformanceStats)(struct PhysicsEngine* engine,
                                   PhysicsPerformanceStats* stats);
        int (*ResetPerformanceCounters)(struct PhysicsEngine* engine);
        int (*EnableProfiling)(struct PhysicsEngine* engine, bool enable);

        // Network visualization (for debugging)
        // 네트워크 시각화 (디버깅용)
        int (*ExportNetworkGraph)(struct PhysicsEngine* engine, const char* filename,
                                  const char* format);
        int (*PrintNetworkInfo)(struct PhysicsEngine* engine);
        int (*ValidatePhysicalConsistency)(struct PhysicsEngine* engine);

        // Logging and diagnostics
        // 로깅 및 진단
        int (*SetLogLevel)(struct PhysicsEngine* engine, int level);
        int (*EnableDetailedLogging)(struct PhysicsEngine* engine, bool enable);
        const char* (*GetLastError)(struct PhysicsEngine* engine);
    } PhysicsEngineDebugging;


/**
 * @brief Unified physics engine - Central manager for all physics systems
 * 통합 물리 엔진 - 모든 물리 시스템의 중앙 관리자
 *
 * CRITICAL ARCHITECTURE DESIGN:
 * This structure serves as the central coordination point for all physics
 * simulations, providing unified management of electrical, pneumatic, and
 * mechanical systems with comprehensive error handling and safety features.
 * 핵심 아키텍처 설계:
 * 이 구조체는 모든 물리 시뮬레이션의 중앙 조정 지점 역할을 하며,
 * 포괄적인 오류 처리 및 안전 기능을 갖춘 전기, 공압, 기계 시스템의 통합 관리를 제공합니다.
 *
 * SAFETY ARCHITECTURE:
 * - Modular C-style interfaces prevent function pointer corruption
 * - State validation functions prevent invalid operations
 * - Memory pool management prevents allocation failures
 * - Comprehensive error tracking and recovery
 * - Real-time performance monitoring
 * 안전 아키텍처:
 * - 모듈식 C 스타일 인터페이스는 함수 포인터 손상을 방지합니다.
 * - 상태 유효성 검증 함수는 잘못된 연산을 방지합니다.
 * - 메모리 풀 관리는 할당 실패를 방지합니다.
 * - 포괄적인 오류 추적 및 복구
 * - 실시간 성능 모니터링
 *
 * INDUSTRIAL REQUIREMENTS:
 * - Deterministic execution times for real-time systems
 * - Fault tolerance and graceful degradation
 * - Performance optimization for complex networks
 * - Debugging capabilities for troubleshooting
 * 산업 요구사항:
 * - 실시간 시스템을 위한 결정론적 실행 시간
 * - 결함 허용 및 정상적인 성능 저하
 * - 복잡한 네트워크에 대한 성능 최적화
 * - 문제 해결을 위한 디버깅 기능
 */
    typedef struct PhysicsEngine {
        ElectricalNetwork* electricalNetwork;  // Electrical network // 전기 네트워크
        PneumaticNetwork* pneumaticNetwork;    // Pneumatic network // 공압 네트워크
        MechanicalSystem* mechanicalSystem;    // Mechanical system // 기계 시스템

        TypedPhysicsState* componentPhysicsStates;  // Physical state of each component // 각 컴포넌트의 물리 상태
        int* componentIdToPhysicsIndex;  // Component ID to physics state index mapping // 컴포넌트 ID → 물리상태 인덱스 매핑
        int maxComponents;               // Maximum number of components // 최대 컴포넌트 수
        int activeComponents;            // Number of active components // 활성 컴포넌트 수

        /**
         * CRITICAL MAPPING SYSTEM:
         * Safe translation between component ports and physics network nodes
         * prevents invalid access and ensures data consistency.
         * 중요 매핑 시스템:
         * 컴포넌트 포트와 물리 네트워크 노드 간의 안전한 변환은
         * 잘못된 접근을 방지하고 데이터 일관성을 보장합니다.
         *
         * MAPPING SAFETY:
         * - Bounds checking prevents array access violations
         * - Validation prevents invalid component/port combinations
         * - Null pointer protection for unconnected ports
         * 매핑 안전:
         * - 경계 검사는 배열 접근 위반을 방지합니다.
         * - 유효성 검증은 잘못된 컴포넌트/포트 조합을 방지합니다.
         * - 연결되지 않은 포트에 대한 널 포인터 보호
         */
        int** componentPortToElectricalNode;  // [componentId][portId] → electricalNodeId
        int**
                componentPortToPneumaticNode;  // [componentId][portId] → pneumaticNodeId
        int** componentPortToMechanicalNode;  // [componentId][portId] → mechanicalNodeId

        bool isInitialized;  // Initialization completed flag // 초기화 완료 여부
        bool isRunning;      // Simulation running state // 시뮬레이션 실행 상태
        bool isPaused;       // Paused state // 일시정지 상태
        float currentTime;   // Current simulation time [s] // 현재 시뮬레이션 시간 [s]
        float deltaTime;     // Time step [s] // 시간 간격 [s]
        int stepCount;       // Total simulation step count // 총 시뮬레이션 스텝 수

        bool isNetworkBuilt;            // Network built flag // 네트워크 구성 완료 여부
        bool isReadyForSimulation;      // Ready for simulation flag // 시뮬레이션 실행 준비 완료 여부
        bool isElectricalNetworkReady;  // Electrical network ready flag // 전기 네트워크 준비 완료 여부
        bool isPneumaticNetworkReady;   // Pneumatic network ready flag // 공압 네트워크 준비 완료 여부
        bool isMechanicalSystemReady;   // Mechanical system ready flag // 기계 시스템 준비 완료 여부

        float convergenceTolerance;  // Convergence tolerance // 수렴 허용오차
        int maxIterations;           // Maximum iterations // 최대 반복 횟수
        bool enableLogging;          // Logging enabled flag // 로깅 활성화 여부
        bool enableProfiling;        // Profiling enabled flag // 프로파일링 활성화 여부
        int logLevel;                // Log level (0=ERROR, 1=WARN, 2=INFO, 3=DEBUG) // 로그 레벨 (0=ERROR, 1=WARN, 2=INFO, 3=DEBUG)

        PhysicsPerformanceStats performanceStats;  // Performance statistics data // 성능 통계 데이터
        float performanceUpdateInterval;           // Performance stats update interval [s] // 성능 통계 업데이트 주기 [s]
        float lastPerformanceUpdate;               // Last performance update time [s] // 마지막 성능 업데이트 시간 [s]

        char lastError[256];  // Last error message // 마지막 에러 메시지
        int lastErrorCode;    // Last error code // 마지막 에러 코드
        bool hasError;        // Error occurred flag // 에러 발생 여부

        bool (*IsEngineReady)(struct PhysicsEngine* engine);   // Verify engine ready state // 엔진 준비 상태 검증
        bool (*IsNetworkValid)(struct PhysicsEngine* engine);  // Verify network validity // 네트워크 유효성 검증
        bool (*IsSafeToRunSimulation)(
                struct PhysicsEngine* engine);  // Verify simulation run safety // 시뮬레이션 실행 안전성 검증

        void* memoryPool;    // Memory pool (for performance optimization) // 메모리 풀 (성능 최적화용)
        int memoryPoolSize;  // Memory pool size [bytes] // 메모리 풀 크기 [bytes]
        int memoryPoolUsed;  // Used memory size [bytes] // 사용된 메모리 크기 [bytes]

        PhysicsEngineLifecycle lifecycle;          // Lifecycle management functions // 생명주기 관리 함수들
        PhysicsEngineNetworking networking;        // Network construction functions // 네트워크 구성 함수들
        PhysicsEngineSimulation simulation;        // Simulation control functions // 시뮬레이션 제어 함수들
        PhysicsEngineDataInterface dataInterface;  // Data synchronization functions // 데이터 동기화 함수들
        PhysicsEngineDebugging debugging;          // Debugging and monitoring functions // 디버깅 및 모니터링 함수들

    } PhysicsEngine;


/**
 * @brief C-style factory functions with comprehensive error handling
 * 포괄적인 오류 처리를 갖춘 C 스타일 팩토리 함수
 *
 * FACTORY SAFETY FEATURES:
 * Safe memory management and initialization for physics engine creation
 * with different configuration presets for various use cases.
 * 팩토리 안전 기능:
 * 다양한 사용 사례에 대한 여러 구성 프리셋을 사용하여 물리 엔진을 생성하기 위한
 * 안전한 메모리 관리 및 초기화.
 *
 * MEMORY SAFETY:
 * - Validated parameter checking prevents invalid configurations
 * - Proper cleanup on initialization failure
 * - Memory allocation validation
 * - Function pointer initialization verification
 * 메모리 안전:
 * - 유효성 검증된 파라미터 확인은 잘못된 구성을 방지합니다.
 * - 초기화 실패 시 적절한 정리
 * - 메모리 할당 유효성 검증
 * - 함수 포인터 초기화 검증
 *
 * FACTORY PATTERNS:
 * - Default: Balanced performance and stability
 * - High Performance: Optimized for large networks
 * - Debugging: Enhanced diagnostics and logging
 * 팩토리 패턴:
 * - Default: 균형 잡힌 성능 및 안정성
 * - High Performance: 대규모 네트워크에 최적화
 * - Debugging: 향상된 진단 및 로깅
 */
    extern "C" {
// Create and initialize the physics engine
// 물리엔진 생성 및 초기화
    PhysicsEngine* CreatePhysicsEngine(int maxComponents, int maxWires);
    int InitializePhysicsEngine(PhysicsEngine* engine);
    void DestroyPhysicsEngine(PhysicsEngine* engine);

// Create physics engine with default settings (convenience function)
// 기본 설정으로 물리엔진 생성 (편의 함수)
    PhysicsEngine* CreateDefaultPhysicsEngine(void);

// Create physics engine with high-performance settings
// 고성능 설정으로 물리엔진 생성
    PhysicsEngine* CreateHighPerformancePhysicsEngine(int maxComponents,
                                                      int maxWires);

// Create physics engine with debugging settings
// 디버깅 설정으로 물리엔진 생성
    PhysicsEngine* CreateDebuggingPhysicsEngine(int maxComponents, int maxWires);
    }


/**
 * @brief Default configuration values for reliable operation
 * 안정적인 작동을 위한 기본 구성 값
 *
 * CONFIGURATION SAFETY:
 * These carefully chosen default values provide stable, reliable operation
 * while allowing customization for specific industrial requirements.
 * 구성 안전:
 * 신중하게 선택된 이 기본값들은 특정 산업 요구사항에 대한 사용자 정의를 허용하면서
 * 안정적이고 신뢰할 수 있는 작동을 제공합니다.
 *
 * VALUE SELECTION CRITERIA:
 * - Industrial automation compatibility
 * - Numerical stability assurance
 * - Performance optimization
 * - Real-time constraint satisfaction
 * 값 선택 기준:
 * - 산업 자동화 호환성
 * - 수치적 안정성 보장
 * - 성능 최적화
 * - 실시간 제약 조건 만족
 */
    namespace PhysicsEngineDefaults {
// Default simulation parameters
// 기본 시뮬레이션 파라미터
        const float DEFAULT_TIME_STEP = 1.0f / 60.0f;       // 60 FPS
        const float DEFAULT_CONVERGENCE_TOLERANCE = 1e-6f;  // 1 ppm accuracy // 1 ppm 정확도
        const int DEFAULT_MAX_ITERATIONS = 100;             // Max 100 iterations // 최대 100회 반복
        const int DEFAULT_MAX_COMPONENTS = 100;             // Max 100 components // 최대 100개 컴포넌트
        const int DEFAULT_MAX_WIRES = 500;                  // Max 500 wires // 최대 500개 와이어

// Performance-related settings
// 성능 관련 설정
        const float DEFAULT_PERFORMANCE_UPDATE_INTERVAL =
                1.0f;                                          // Update performance every 1 second // 1초마다 성능 업데이트
        const int DEFAULT_MEMORY_POOL_SIZE = 1024 * 1024;  // 1MB memory pool // 1MB 메모리 풀
        const int DEFAULT_LOG_LEVEL = 2;                   // INFO level // INFO 레벨

// Physical constants
// 물리적 상수들
        const float ATMOSPHERIC_PRESSURE = 101325.0f;  // Standard atmospheric pressure [Pa] // 표준 대기압 [Pa]
        const float AIR_DENSITY_STP = 1.225f;          // Air density at STP [kg/m³] // 표준상태 공기밀도 [kg/m³]
        const float AIR_VISCOSITY = 1.8e-5f;           // Air viscosity [Pa⋅s] // 공기 점성계수 [Pa⋅s]
        const float GRAVITY = 9.81f;                   // Gravitational acceleration [m/s²] // 중력가속도 [m/s²]
        const float GAS_CONSTANT = 287.0f;             // Air gas constant [J/kg⋅K] // 공기 기체상수 [J/kg⋅K]
        const float SPECIFIC_HEAT_RATIO = 1.4f;        // Air specific heat ratio [-] // 공기 비열비 [-]
    }  // namespace PhysicsEngineDefaults


/**
 * @brief Comprehensive error code system for robust error handling
 * 강력한 오류 처리를 위한 포괄적인 오류 코드 시스템
 *
 * CRITICAL ERROR MANAGEMENT:
 * Systematic error codes enable proper error handling and recovery
 * strategies essential for industrial automation safety.
 * 중요 오류 관리:
 * 체계적인 오류 코드는 산업 자동화 안전에 필수적인 적절한 오류 처리 및 복구 전략을
 * 가능하게 합니다.
 *
 * ERROR CODE CLASSIFICATION:
 * - 100s: Initialization and memory errors
 * - 200s: Network topology errors
 * - 300s: Simulation execution errors
 * - 400s: Data synchronization errors
 * - 500s: System-level errors
 * 오류 코드 분류:
 * - 100번대: 초기화 및 메모리 오류
 * - 200번대: 네트워크 토폴로지 오류
 * - 300번대: 시뮬레이션 실행 오류
 * - 400번대: 데이터 동기화 오류
 * - 500번대: 시스템 수준 오류
 *
 * ERROR HANDLING STRATEGY:
 * Each error code corresponds to specific recovery procedures,
 * enabling graceful degradation and system stability maintenance.
 * 오류 처리 전략:
 * 각 오류 코드는 특정 복구 절차에 해당하며, 정상적인 성능 저하 및 시스템 안정성 유지를
 * 가능하게 합니다.
 */
    typedef enum PhysicsEngineError {
        PHYSICS_ENGINE_SUCCESS = 0,  // Success // 성공

        // Initialization and memory-related errors (100s)
        // 초기화 및 메모리 관련 에러 (100번대)
        PHYSICS_ENGINE_ERROR_INITIALIZATION = 100,  // Initialization failed // 초기화 실패
        PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,     // Memory allocation failed // 메모리 할당 실패
        PHYSICS_ENGINE_ERROR_INVALID_PARAMETER,     // Invalid parameter // 잘못된 파라미터
        PHYSICS_ENGINE_ERROR_NOT_INITIALIZED,       // Not initialized // 초기화되지 않음

        // Network construction-related errors (200s)
        // 네트워크 구성 관련 에러 (200번대)
        PHYSICS_ENGINE_ERROR_NETWORK_BUILD = 200,   // Network build failed // 네트워크 구성 실패
        PHYSICS_ENGINE_ERROR_INVALID_TOPOLOGY,      // Invalid topology // 잘못된 토폴로지
        PHYSICS_ENGINE_ERROR_DISCONNECTED_NETWORK,  // Disconnected network // 연결되지 않은 네트워크
        PHYSICS_ENGINE_ERROR_COMPONENT_NOT_FOUND,   // Component not found // 컴포넌트를 찾을 수 없음

        // Simulation-related errors (300s)
        // 시뮬레이션 관련 에러 (300번대)
        PHYSICS_ENGINE_ERROR_SIMULATION_FAILED = 300,  // Simulation failed // 시뮬레이션 실패
        PHYSICS_ENGINE_ERROR_CONVERGENCE_FAILED,       // Convergence failed // 수렴 실패
        PHYSICS_ENGINE_ERROR_NUMERICAL_INSTABILITY,    // Numerical instability // 수치적 불안정성
        PHYSICS_ENGINE_ERROR_PHYSICAL_INCONSISTENCY,   // Physical inconsistency // 물리적 모순

        // Data-related errors (400s)
        // 데이터 관련 에러 (400번대)
        PHYSICS_ENGINE_ERROR_DATA_SYNC = 400,  // Data sync failed // 데이터 동기화 실패
        PHYSICS_ENGINE_ERROR_INVALID_STATE,    // Invalid state // 잘못된 상태
        PHYSICS_ENGINE_ERROR_BUFFER_OVERFLOW,  // Buffer overflow // 버퍼 오버플로우
        PHYSICS_ENGINE_ERROR_TYPE_MISMATCH,    // Type mismatch // 타입 불일치

        // System-related errors (500s)
        // 시스템 관련 에러 (500번대)
        PHYSICS_ENGINE_ERROR_SYSTEM = 500,    // System error // 시스템 에러
        PHYSICS_ENGINE_ERROR_THREAD_FAILURE,  // Thread failure // 쓰레드 실패
        PHYSICS_ENGINE_ERROR_IO_ERROR,        // I/O error // 입출력 에러
        PHYSICS_ENGINE_ERROR_UNKNOWN          // Unknown error // 알 수 없는 에러
    } PhysicsEngineError;

/**
 * @brief Error code to string conversion utility
 * @param error Error code to convert
 * @return Human-readable error description
 * 오류 코드를 문자열로 변환하는 유틸리티
 * @param error 변환할 오류 코드
 * @return 사람이 읽을 수 있는 오류 설명
 *
 * DEBUGGING SUPPORT:
 * Provides clear, actionable error messages for troubleshooting
 * and maintenance in industrial environments.
 * 디버깅 지원:
 * 산업 환경에서의 문제 해결 및 유지보수를 위해 명확하고 실행 가능한 오류 메시지를 제공합니다.
 */
    const char* PhysicsEngineErrorToString(PhysicsEngineError error);

}  // namespace plc
#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PHYSICS_PHYSICS_ENGINE_H_