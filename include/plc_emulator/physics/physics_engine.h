/*
 * physics_engine.h
 *
 * 다중 도메인 물리 엔진 인터페이스 선언.
 * Declarations for the multi-domain physics engine interface.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PHYSICS_PHYSICS_ENGINE_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PHYSICS_PHYSICS_ENGINE_H_

#include "plc_emulator/core/data_types.h"
#include "plc_emulator/physics/physics_network.h"
#include "plc_emulator/physics/physics_states.h"

namespace plc {
    struct PhysicsEngine;

    /*
     * 물리 경고 콜백 타입과 등록 함수.
     * Physics warning callback type and registration helpers.
     */
    typedef void (*PhysicsWarningCallback)(const char* message);
    void SetPhysicsWarningCallback(PhysicsWarningCallback callback);
    void NotifyPhysicsWarning(const char* message);
    /*
     * 실시간 성능 통계 구조체.
     * Real-time performance statistics.
     */
    typedef struct PhysicsPerformanceStats {
        float totalComputeTime;
        float electricalSolveTime;
        float pneumaticSolveTime;
        float mechanicalSolveTime;
        float networkBuildTime;

        int electricalIterations;
        int pneumaticIterations;
        bool electricalConverged;
        bool pneumaticConverged;
        bool mechanicalStable;


        int electricalNodes;
        int electricalEdges;
        int pneumaticNodes;
        int pneumaticEdges;
        int mechanicalNodes;
        int mechanicalEdges;

        float electricalResidual;
        float pneumaticResidual;
        float mechanicalEnergy;
        float powerConsumption;
        float airConsumption;

        float simulationFPS;
        float realTimeRatio;
        float memoryUsage;
    } PhysicsPerformanceStats;

    /*
     * 시뮬레이션 결과 스냅샷.
     * Simulation result snapshot.
     */
    typedef struct PhysicsSimulationResult {
        bool isValid;
        float simulationTime;
        float deltaTime;
        int stepCount;

        TypedPhysicsState* componentStates;
        int componentCount;

        float* nodeVoltages;
        float* nodeCurrents;
        float* nodePressures;
        float* nodeMassFlows;
        float* nodePositions;
        float* nodeVelocities;

        PhysicsPerformanceStats stats;
    } PhysicsSimulationResult;

    /*
     * 엔진 수명주기 함수 테이블.
     * Function table for engine lifecycle.
     */
    typedef struct PhysicsEngineLifecycle {
        int (*Create)(struct PhysicsEngine* engine, int maxComponents, int maxWires);
        int (*Initialize)(struct PhysicsEngine* engine);
        int (*Reset)(struct PhysicsEngine* engine);
        int (*Destroy)(struct PhysicsEngine* engine);

        int (*SetTimeStep)(struct PhysicsEngine* engine, float deltaTime);
        int (*SetConvergenceTolerance)(struct PhysicsEngine* engine, float tolerance);
        int (*SetMaxIterations)(struct PhysicsEngine* engine, int maxIter);
        int (*EnableLogging)(struct PhysicsEngine* engine, bool enable);
    } PhysicsEngineLifecycle;

    /*
     * 네트워크 빌드/검증 인터페이스.
     * Interface for network build and validation.
     */
    typedef struct PhysicsEngineNetworking {
        int (*BuildNetworksFromWiring)(struct PhysicsEngine* engine,
                                       const Wire* wires, int wireCount,
                                       const PlacedComponent* components,
                                       int componentCount);
        int (*UpdateNetworkTopology)(struct PhysicsEngine* engine);
        int (*OptimizeNetworks)(struct PhysicsEngine* engine);

        int (*RebuildElectricalNetwork)(struct PhysicsEngine* engine);
        int (*RebuildPneumaticNetwork)(struct PhysicsEngine* engine);
        int (*RebuildMechanicalSystem)(struct PhysicsEngine* engine);

        int (*ValidateNetworks)(struct PhysicsEngine* engine);
        int (*CheckConnectivity)(struct PhysicsEngine* engine);
    } PhysicsEngineNetworking;

    /*
     * 시뮬레이션 실행 인터페이스.
     * Interface for simulation execution.
     */
    typedef struct PhysicsEngineSimulation {
        int (*RunSimulation)(struct PhysicsEngine* engine, float deltaTime);
        int (*RunSingleStep)(struct PhysicsEngine* engine);
        int (*PauseSimulation)(struct PhysicsEngine* engine);
        int (*ResumeSimulation)(struct PhysicsEngine* engine);
        int (*StopSimulation)(struct PhysicsEngine* engine);

        int (*SolveElectricalSystem)(struct PhysicsEngine* engine, float deltaTime);
        int (*SolvePneumaticSystem)(struct PhysicsEngine* engine, float deltaTime);
        int (*SolveMechanicalSystem)(struct PhysicsEngine* engine, float deltaTime);

        int (*SaveState)(struct PhysicsEngine* engine, void** stateData,
                         int* stateSize);
        int (*RestoreState)(struct PhysicsEngine* engine, const void* stateData,
                            int stateSize);
    } PhysicsEngineSimulation;

    /*
     * 컴포넌트 상태 교환 인터페이스.
     * Interface for component state exchange.
     */
    typedef struct PhysicsEngineDataInterface {
        int (*UpdateComponentInputs)(struct PhysicsEngine* engine, int componentId,
                                     const void* inputData);
        int (*SetComponentState)(struct PhysicsEngine* engine, int componentId,
                                 const TypedPhysicsState* state);
        int (*ApplyExternalForces)(struct PhysicsEngine* engine, int componentId,
                                   const float* forces);

        int (*GetComponentState)(struct PhysicsEngine* engine, int componentId,
                                 TypedPhysicsState* state);
        int (*GetSimulationResult)(struct PhysicsEngine* engine,
                                   PhysicsSimulationResult* result);
        int (*GetNetworkStates)(struct PhysicsEngine* engine, float* voltages,
                                float* currents, float* pressures, float* flows);

        int (*SyncAllComponentStates)(struct PhysicsEngine* engine,
                                      PlacedComponent* components, int count);
        int (*UpdateInternalStates)(struct PhysicsEngine* engine,
                                    PlacedComponent* components, int count);
    } PhysicsEngineDataInterface;

    /*
     * 디버그/프로파일링 인터페이스.
     * Interface for debugging and profiling.
     */
    typedef struct PhysicsEngineDebugging {
        int (*GetPerformanceStats)(struct PhysicsEngine* engine,
                                   PhysicsPerformanceStats* stats);
        int (*ResetPerformanceCounters)(struct PhysicsEngine* engine);
        int (*EnableProfiling)(struct PhysicsEngine* engine, bool enable);

        int (*ExportNetworkGraph)(struct PhysicsEngine* engine, const char* filename,
                                  const char* format);
        int (*PrintNetworkInfo)(struct PhysicsEngine* engine);
        int (*ValidatePhysicalConsistency)(struct PhysicsEngine* engine);

        int (*SetLogLevel)(struct PhysicsEngine* engine, int level);
        int (*EnableDetailedLogging)(struct PhysicsEngine* engine, bool enable);
        const char* (*GetLastError)(struct PhysicsEngine* engine);
    } PhysicsEngineDebugging;

    /*
     * 물리 엔진 상태와 함수 테이블.
     * Physics engine state and function tables.
     */
    typedef struct PhysicsEngine {
        ElectricalNetwork* electricalNetwork;
        PneumaticNetwork* pneumaticNetwork;
        MechanicalSystem* mechanicalSystem;

        TypedPhysicsState* componentPhysicsStates;
        int* componentIdToPhysicsIndex;
        int maxComponents;
        int activeComponents;

        int** componentPortToElectricalNode;
        int**
                componentPortToPneumaticNode;
        int** componentPortToMechanicalNode;

        bool isInitialized;
        bool isRunning;
        bool isPaused;
        float currentTime;
        float deltaTime;
        int stepCount;

        bool isNetworkBuilt;
        bool isReadyForSimulation;
        bool isElectricalNetworkReady;
        bool isPneumaticNetworkReady;
        bool isMechanicalSystemReady;

        float convergenceTolerance;
        int maxIterations;
        bool enableLogging;
        bool enableProfiling;
        int logLevel;

        PhysicsPerformanceStats performanceStats;
        float performanceUpdateInterval;
        float lastPerformanceUpdate;

        char lastError[256];
        int lastErrorCode;
        bool hasError;

        bool (*IsEngineReady)(struct PhysicsEngine* engine);
        bool (*IsNetworkValid)(struct PhysicsEngine* engine);
        bool (*IsSafeToRunSimulation)(
                struct PhysicsEngine* engine);

        void* memoryPool;
        int memoryPoolSize;
        int memoryPoolUsed;

        PhysicsEngineLifecycle lifecycle;
        PhysicsEngineNetworking networking;
        PhysicsEngineSimulation simulation;
        PhysicsEngineDataInterface dataInterface;
        PhysicsEngineDebugging debugging;

    } PhysicsEngine;

    /*
     * C API 진입점.
     * C API entry points.
     */
    extern "C" {
    PhysicsEngine* CreatePhysicsEngine(int maxComponents, int maxWires);
    int InitializePhysicsEngine(PhysicsEngine* engine);
    void DestroyPhysicsEngine(PhysicsEngine* engine);

    PhysicsEngine* CreateDefaultPhysicsEngine(void);

    PhysicsEngine* CreateHighPerformancePhysicsEngine(int maxComponents,
                                                      int maxWires);

    PhysicsEngine* CreateDebuggingPhysicsEngine(int maxComponents, int maxWires);
    }

    /*
     * 엔진 기본 설정값.
     * Default engine settings.
     */
    namespace PhysicsEngineDefaults {
        const float DEFAULT_TIME_STEP = 1.0f / 60.0f;
        const float DEFAULT_CONVERGENCE_TOLERANCE = 1e-6f;
        const int DEFAULT_MAX_ITERATIONS = 100;
        const int DEFAULT_MAX_COMPONENTS = 100;
        const int DEFAULT_MAX_WIRES = 500;

        const float DEFAULT_PERFORMANCE_UPDATE_INTERVAL =
                1.0f;
        const int DEFAULT_MEMORY_POOL_SIZE = 1024 * 1024;
        const int DEFAULT_LOG_LEVEL = 2;

        const float ATMOSPHERIC_PRESSURE = 101325.0f;
        const float AIR_DENSITY_STP = 1.225f;
        const float AIR_VISCOSITY = 1.8e-5f;
        const float GRAVITY = 9.81f;
        const float GAS_CONSTANT = 287.0f;
        const float SPECIFIC_HEAT_RATIO = 1.4f;
    }

    /*
     * 물리 엔진 오류 코드.
     * Physics engine error codes.
     */
    typedef enum PhysicsEngineError {
        PHYSICS_ENGINE_SUCCESS = 0,

        PHYSICS_ENGINE_ERROR_INITIALIZATION = 100,
        PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
        PHYSICS_ENGINE_ERROR_INVALID_PARAMETER,
        PHYSICS_ENGINE_ERROR_NOT_INITIALIZED,

        PHYSICS_ENGINE_ERROR_NETWORK_BUILD = 200,
        PHYSICS_ENGINE_ERROR_INVALID_TOPOLOGY,
        PHYSICS_ENGINE_ERROR_DISCONNECTED_NETWORK,
        PHYSICS_ENGINE_ERROR_COMPONENT_NOT_FOUND,

        PHYSICS_ENGINE_ERROR_SIMULATION_FAILED = 300,
        PHYSICS_ENGINE_ERROR_CONVERGENCE_FAILED,
        PHYSICS_ENGINE_ERROR_NUMERICAL_INSTABILITY,
        PHYSICS_ENGINE_ERROR_PHYSICAL_INCONSISTENCY,

        PHYSICS_ENGINE_ERROR_DATA_SYNC = 400,
        PHYSICS_ENGINE_ERROR_INVALID_STATE,
        PHYSICS_ENGINE_ERROR_BUFFER_OVERFLOW,
        PHYSICS_ENGINE_ERROR_TYPE_MISMATCH,

        PHYSICS_ENGINE_ERROR_SYSTEM = 500,
        PHYSICS_ENGINE_ERROR_THREAD_FAILURE,
        PHYSICS_ENGINE_ERROR_IO_ERROR,
        PHYSICS_ENGINE_ERROR_UNKNOWN
    } PhysicsEngineError;

    /*
     * 오류 코드를 문자열로 변환합니다.
     * Converts an error code to a string.
     */
    const char* PhysicsEngineErrorToString(PhysicsEngineError error);

}  /* namespace plc */
#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PHYSICS_PHYSICS_ENGINE_H_ */
