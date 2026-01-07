// application.h

//
// Main application class for the PLC emulator.
// PLC ?ë¨???‰ì” ?ê³—ì“½ ï§ë¶¿???ì¢ëµ†?±Ñ???ë€???€???¼ì—¯??ˆë–.
//
// This file manages the application lifecycle and coordinates mode switching.
// ?????”ª?? ?ì¢ëµ†?±Ñ???ë€????¸ì±¸äºŒì‡¨ë¦°ç‘œ??¿Â€?±Ñ‹ë¸¯??ï§â‘¤ë±?åª??ê¾ªì†š??è­°ê³—???¸ë•²??
//

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_APPLICATION_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_APPLICATION_H_

#include "imgui.h"
#include "plc_emulator/core/data_types.h"
#include "plc_emulator/core/plc_simulator_core.h"
#include "plc_emulator/physics/physics_engine.h"
#include "plc_emulator/programming/compiled_plc_executor.h"
#include "plc_emulator/programming/programming_mode.h"
#include "plc_emulator/project/project_file_manager.h"

#include <mutex>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

struct GLFWwindow;

namespace plc {

    class ProgrammingMode;

//
// The main class that orchestrates the entire PLC emulator application.
// ?ê¾©ê»œ PLC ?ë¨???‰ì” ???ì¢ëµ†?±Ñ???ë€???¥ì•·???ë’— ï§ë¶¿????€???¼ì—¯??ˆë–.
//
    class Application {
    public:
        /**
         * @brief Constructs the Application object and initializes default values.
         * Application åª›ì•¹ê»œç‘œ???¹ê½¦??í€?æ¹²ê³•??ª›ë¯ªì“£ ?¥ë‡ë¦?ë·€ë¹€??ˆë–.
         */
        explicit Application(bool enable_debug_mode);

        /**
         * @brief Destructor. Ensures all resources are cleaned up properly.
         * ???ˆ‡?? ï§â‘¤ë±??±ÑŠëƒ¼??? ??ì»?‘œ?¿ì¾¶ ?ëº£â”??ë£„æ¿?è¹‚ëŒ???¸ë•²??
         */
        ~Application();

        // Disable copy and assignment to prevent accidental duplication.
        // ??ë£„??? ??? è¹‚ë“­?£ç‘œ?è«›â‘¹???ë¦° ?ê¾ªë¹ è¹‚ë“­ê¶?è«??ì¢Šë–¦????¾ª??ê¹Šì†•??¸ë•²??
        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        /**
         * @brief Initializes all subsystems, including the window, GUI, and simulator.
         * ??ˆë£„?? GUI, ?????‰ì” ?ê³? ??ë¸??ï§â‘¤ë±???ì ??–ë’ª??–ì“£ ?¥ë‡ë¦?ë·€ë¹€??ˆë–.
         * @return True if initialization is successful, false otherwise.
         * ?¥ë‡ë¦?ë¶¿ë¿‰ ?ê¹ƒë‚¬??ãˆƒ true, æ´¹ëªƒ?ƒï§? ??†ì‘ï§?false??è«›ì„‘???¸ë•²??
         */
        bool Initialize();

        /**
         * @brief Starts and runs the main application loop.
         * ï§ë¶¿???ì¢ëµ†?±Ñ???ë€??·â‘¦ë´½ç‘œ???–ì˜‰??í€???½ë»¾??¸ë•²??
         */
        void Run();

        /**
         * @brief Shuts down the application and releases all resources.
         * ?ì¢ëµ†?±Ñ???ë€???«ë‚…ì¦??í€?ï§â‘¤ë±??±ÑŠëƒ¼??? ??ì £??¸ë•²??
         */
        void Shutdown();

        void UpdateTouchGesture(float zoom_delta, ImVec2 pan_delta,
                                bool active);
        void SetPanInputActive(bool active);
        void SetTouchAnchor(ImVec2 screen_pos);
        void RegisterWin32RightClick();
        void RegisterWin32SideClick();
        void RegisterWin32SideDown(bool is_down);
        bool IsPointInCanvas(ImVec2 screen_pos) const;
        bool IsDebugEnabled() const;
        void DebugLog(const std::string& message);
        void QueuePhysicsWarningDialog(const std::string& message);
        /**
         * @brief Saves the current project state to a .csv file.
         * ?ê¾©ì˜± ?ê¾¨ì¤ˆ??ºë“ƒ ?ê³¹ê¹­??.csv ???”ª?????Î½ë¹€??ˆë–.
         * @param filePath The path to save the project file.
         * filePath???ê¾¨ì¤ˆ??ºë“ƒ ???”ª?????Î½ë¸?å¯ƒìˆì¤??…ë•²??
         * @param projectName An optional name for the project.
         * projectName?? ?ê¾¨ì¤ˆ??ºë“ƒ???ì¢ê¹®????€ì«??…ë•²??
         * @return True on successful save, false otherwise.
         * ???Î¼ë¿??ê¹ƒë‚¬??ãˆƒ true, æ´¹ëªƒ?ƒï§? ??†ì‘ï§?false??è«›ì„‘???¸ë•²??
         */
        bool SaveProject(const std::string& filePath,
                         const std::string& projectName = "");

        /**
         * @brief Loads a project from a .csv file.
         * .csv ???”ªæ¿¡ì’•????ê¾¨ì¤ˆ??ºë“ƒ???ºëˆ???¬ë•²??
         * @param filePath The path of the project file to load.
         * filePath???ºëˆ????ê¾¨ì¤ˆ??ºë“ƒ ???”ª??å¯ƒìˆì¤??…ë•²??
         * @return True on successful load, false otherwise.
         * ?ºëˆ???ºë¦°???ê¹ƒë‚¬??ãˆƒ true, æ´¹ëªƒ?ƒï§? ??†ì‘ï§?false??è«›ì„‘???¸ë•²??
         */
        bool LoadProject(const std::string& filePath);

    private:
        /**
         * @brief Initializes the GLFW window.
         * GLFW ??ˆë£„?ê³? ?¥ë‡ë¦?ë·€ë¹€??ˆë–.
         */
        bool InitializeWindow();

        /**
         * @brief Initializes the ImGui context.
         * ImGui ?Œâ‘¦???½ë“ƒ???¥ë‡ë¦?ë·€ë¹€??ˆë–.
         */
        bool InitializeImGui();

        /**
         * @brief Sets up a custom visual style for ImGui.
         * ImGui????????????ëº¤ì“½ ??¾©ï¼???????±ì“£ ??¼ì ™??¸ë•²??
         */
        void SetupCustomStyle();

        /**
         * @brief Processes user input for the current frame.
         * ?ê¾©ì˜± ?ê¾¨ì …?ê¾©ë¿‰ ???????????…ì °??ï§£ì„???¸ë•²??
         */
        void ProcessInput();

        /**
         * @brief Main update function called once per frame.
         * ?ê¾¨ì …?ê¾¨ë–¦ ??è¸??ëª„í…§??ë’— ï§ë¶¿????…ëœ²??„ë“ƒ ??¥ë‹”??…ë•²??
         */
        void Update();

        /**
         * @brief Updates the physics simulation state.
         * ?¾ì‡°???????‰ì” ???ê³¹ê¹­????…ëœ²??„ë“ƒ??¸ë•²??
         */
        void UpdatePhysics();

        /**
         * @brief Simulates basic physics phenomena, independent of the PLC state.
         * PLC ?ê³¹ê¹­?? ??…â”°?ê³¸ì‘æ¿?æ¹²ê³•??ê³¸ì”¤ ?¾ì‡°???ê¾©ê¸½???????‰ì” ??‘ë???ˆë–.
         */
        void UpdateBasicPhysics();

        /**
         * @brief Executes one scan of the loaded ladder logic program.
         * ?ºëˆ?????ëœ‘ æ¿¡ì’–ì­??ê¾¨ì¤ˆæ´¹ëªƒ???????¼í‹ª????½ë»¾??¸ë•²??
         */
        void SimulateLoadedLadder();

        /**
         * @brief Gets the current state of a PLC device (e.g., input, output).
         * PLC ?ë¶¾ì»®??ë’ª(?? ??…ì °, ?°ì’•?????ê¾©ì˜± ?ê³¹ê¹­??åª›Â€?ëª„ìƒƒ??ˆë–.
         */
        bool GetPlcDeviceState(const std::string& address);

        /**
         * @brief Sets the state of a PLC device.
         * PLC ?ë¶¾ì»®??ë’ª???ê³¹ê¹­????¼ì ™??¸ë•²??
         */
        void SetPlcDeviceState(const std::string& address, bool state);

        /**
         * @brief Loads a ladder program from a specified .ld file.
         * ï§Â€?ëº£ë§‚ .ld ???”ªæ¿¡ì’•?????ëœ‘ ?ê¾¨ì¤ˆæ´¹ëªƒ????ºëˆ???¬ë•²??
         * @param filepath Path to the .ld file.
         * filepath??.ld ???”ª??å¯ƒìˆì¤??…ë•²??
         */
        void LoadLadderProgramFromLD(const std::string& filepath);

        /**
         * @brief Converts a string representation of an instruction to its enum type.
         * ï§ë‚…ì¡??ì“½ ?¾ëª„?????—ì½????€????¿êµ…??????†ì‘æ¿?è¹‚Â€??‘ë???ˆë–.
         */
        LadderInstructionType StringToInstructionType(const std::string& str);

        /**
         * @brief Synchronizes the ladder program from the programming mode to the simulator.
         * ?ê¾¨ì¤ˆæ´¹ëªƒ?’è«›?ï§â‘¤ë±????ëœ‘ ?ê¾¨ì¤ˆæ´¹ëªƒ????????‰ì” ?ê³•ì¤ˆ ??†ë¦°?ë·€ë¹€??ˆë–.
         */
        void SyncLadderProgramFromProgrammingMode();

        /**
         * @brief Creates a default ladder program for testing and initialization.
         * ???’ª??è«??¥ë‡ë¦?ë¶? ?ê¾ªë¹ æ¹²ê³•????ëœ‘ ?ê¾¨ì¤ˆæ´¹ëªƒ?????¹ê½¦??¸ë•²??
         */
        void CreateDefaultTestLadderProgram();

        /**
         * @brief Compiles the current ladder program and loads it into the PLC executor.
         * ?ê¾©ì˜± ??ëœ‘ ?ê¾¨ì¤ˆæ´¹ëªƒ????ŒëŒ„???³ë¸¯??PLC ??½ë»¾æ¹²ê³—ë¿?æ¿¡ì’•ë±??¸ë•²??
         */
        void CompileAndLoadLadderProgram();

        /**
         * @brief Simulates the electrical network connecting components.
         * ?ŒëŒ„ë£??°ë“ƒ???ê³Œê»??ë’— ?ê¾§ë¦° ??½ë“ƒ??°ê²•???????‰ì” ??‘ë???ˆë–.
         */
        void SimulateElectrical();

        /**
         * @brief Updates the internal logic of placed components.
         * è«›ê³—????ŒëŒ„ë£??°ë“ƒ????€? æ¿¡ì’–ì­????…ëœ²??„ë“ƒ??¸ë•²??
         */
        void UpdateComponentLogic();

        /**
         * @brief Simulates the pneumatic network connecting components.
         * ?ŒëŒ„ë£??°ë“ƒ???ê³Œê»??ë’— ?¨ë“­ë¸???½ë“ƒ??°ê²•???????‰ì” ??‘ë???ˆë–.
         */
        void SimulatePneumatic();

        /**
         * @brief Updates the state of actuators (e.g., cylinders) based on simulation.
         * ?????‰ì” ??å¯ƒê³Œ????ê³•ì”ª ??ªí…›?ë¨?” ???? ??»â”›?????ê³¹ê¹­????…ëœ²??„ë“ƒ??¸ë•²??
         */
        void UpdateActuators();

        /**
         * @brief Retrieves the port information for a given component.
         * äºŒì‡±ë¼±ï§??ŒëŒ„ë£??°ë“ƒ???????ëº£ë‚«??åª›Â€?ëª„ìƒƒ??ˆë–.
         */
        std::vector<std::pair<int, bool>> GetPortsForComponent(
                const PlacedComponent& comp);

        /**
         * @brief Synchronizes PLC output states to the physics engine's electrical network.
         * PLC ?°ì’•???ê³¹ê¹­???¾ì‡°???ë¶¿ì­Š???ê¾§ë¦° ??½ë“ƒ??°ê²•?? ??†ë¦°?ë·€ë¹€??ˆë–.
         */
        void SyncPLCOutputsToPhysicsEngine();

        /**
         * @brief Synchronizes results from the physics engine back to the application state.
         * ?¾ì‡°???ë¶¿ì­Š??å¯ƒê³Œ?µç‘œ??ì¢ëµ†?±Ñ???ë€??ê³¹ê¹­æ¿???¼ë–† ??†ë¦°?ë·€ë¹€??ˆë–.
         */
        void SyncPhysicsEngineToApplication();

        /**
         * @brief Updates and displays physics engine performance statistics.
         * ?¾ì‡°???ë¶¿ì­Š???ê¹…ë’« ???€ç‘œ???…ëœ²??„ë“ƒ??í€???–ë–†??¸ë•²??
         */
        void UpdatePhysicsPerformanceStats();

        /**
         * @brief Renders the entire application scene for the current frame.
         * ?ê¾©ì˜± ?ê¾¨ì …?ê¾©ë¿‰ ?????ê¾©ê»œ ?ì¢ëµ†?±Ñ???ë€??Î»??????œ‘ï§ê³¹ë¹€??ˆë–.
         */
        void Render();

        /**
         * @brief Cleans up resources before shutting down.
         * ?«ë‚…ì¦??ë¦° ?ê¾©ë¿‰ ?±ÑŠëƒ¼??? ?ëº£â”??¸ë•²??
         */
        void Cleanup();

        /**
         * @brief Updates the screen positions of all component ports.
         * ï§â‘¤ë±??ŒëŒ„ë£??°ë“ƒ ??????ë¶¾ãˆƒ ?ê¾©íŠ‚????…ëœ²??„ë“ƒ??¸ë•²??
         */
        void UpdatePortPositions();

        /**
         * @brief Renders the main user interface using ImGui.
         * ImGui???????ë¿¬ ï§ë¶¿????????ëª…ê½£??ì” ??? ???œ‘ï§ê³¹ë¹€??ˆë–.
         */
        void RenderUI();

        /**
         * @brief Renders the UI specific to the wiring mode.
         * è«›ê³—ê½?ï§â‘¤ë±???ë±ì†•??UI?????œ‘ï§ê³¹ë¹€??ˆë–.
         */
        void RenderWiringModeUI();

        /**
         * @brief Renders the application header bar.
         * ?ì¢ëµ†?±Ñ???ë€???»ëœ‘ è«›ë¶¾? ???œ‘ï§ê³¹ë¹€??ˆë–.
         */
        void RenderHeader();

        /**
         * @brief Renders the toolbar with mode and tool selection.
         * ï§â‘¤ë±?è«??ê¾§ë„ ?ì¢ê¹® æ¹²ê³•?????ˆë’— ??€ì»?‘œ????œ‘ï§ê³¹ë¹€??ˆë–.
         */
        void RenderToolbar();

        /**
         * @brief Renders the main content area of the application.
         * ?ì¢ëµ†?±Ñ???ë€??äº??„ì„‘?—ï§¥??ê³¸ë¿­?????œ‘ï§ê³¹ë¹€??ˆë–.
         */
        void RenderMainArea();

        /**
         * @brief Renders the real-time PLC debug panel.
         * ??¼ë–†åª?PLC ?ë¶¾ì¾­æ´???¤ê¼¸?????œ‘ï§ê³¹ë¹€??ˆë–.
         */
        void RenderPLCDebugPanel();
        void RenderPhysicsWarningDialog();
        /**
         * @brief Converts world coordinates to screen coordinates.
         * ?ë¶¾ë±¶ ?«ëš°ëª´ç‘œ??ë¶¾ãˆƒ ?«ëš°ëª´æ¿¡?è¹‚Â€??‘ë???ˆë–.
         */
        ImVec2 WorldToScreen(const ImVec2& world_pos) const;

        /**
         * @brief Converts screen coordinates to world coordinates.
         * ?ë¶¾ãˆƒ ?«ëš°ëª´ç‘œ??ë¶¾ë±¶ ?«ëš°ëª´æ¿¡?è¹‚Â€??‘ë???ˆë–.
         */
        ImVec2 ScreenToWorld(const ImVec2& screen_pos) const;

        /**
         * @brief Renders the list of available components.
         * ????åª›Â€?Î½ë¸??ŒëŒ„ë£??°ë“ƒ ï§â‘¸ì¤?????œ‘ï§ê³¹ë¹€??ˆë–.
         */
        void RenderComponentList();

        /**
         * @brief Renders all components placed in the workspace.
         * ?ë¬’ë¾½ ?¨ë“¦ì»??è«›ê³—???ï§â‘¤ë±??ŒëŒ„ë£??°ë“ƒ?????œ‘ï§ê³¹ë¹€??ˆë–.
         */
        void RenderPlacedComponents(ImDrawList* draw_list);

        /**
         * @brief Handles the start of a component drag operation from the component list.
         * ?ŒëŒ„ë£??°ë“ƒ ï§â‘¸ì¤?ë¨?½Œ ?ŒëŒ„ë£??°ë“ƒ ??•ì˜’æ´??ë¬’ë¾½ ??–ì˜‰??ï§£ì„???¸ë•²??
         */
        void HandleComponentDragStart(int componentIndex);

        /**
         * @brief Handles the ongoing component drag operation.
         * ï§ê¾ªë»?ä»¥ë¬’???ŒëŒ„ë£??°ë“ƒ ??•ì˜’æ´??ë¬’ë¾½??ï§£ì„???¸ë•²??
         */
        void HandleComponentDrag();

        /**
         * @brief Handles dropping a component onto the workspace.
         * ?ë¬’ë¾½ ?¨ë“¦ì»???ŒëŒ„ë£??°ë“ƒ????•âˆ¼??ë’— ?ë¬’ë¾½??ï§£ì„???¸ë•²??
         */
        void HandleComponentDrop(Position position);

        /**
         * @brief Handles the selection of a placed component.
         * è«›ê³—????ŒëŒ„ë£??°ë“ƒ???ì¢ê¹®??ï§£ì„???¸ë•²??
         */
        void HandleComponentSelection(int instanceId);

        /**
         * @brief Deletes the currently selected component.
         * ?ê¾©ì˜± ?ì¢ê¹®???ŒëŒ„ë£??°ë“ƒ???????¸ë•²??
         */
        void DeleteSelectedComponent();

        /**
         * @brief Handles the start of moving an already placed component.
         * ??€? è«›ê³—????ŒëŒ„ë£??°ë“ƒ????€ë£???–ì˜‰??ï§£ì„???¸ë•²??
         */
        void HandleComponentMoveStart(int instanceId, ImVec2 mousePos);

        /**
         * @brief Handles the end of a component move operation.
         * ?ŒëŒ„ë£??°ë“ƒ ??€ë£??ë¬’ë¾½???«ë‚…ì¦ºç‘œ?ï§£ì„???¸ë•²??
         */
        void HandleComponentMoveEnd();

        /**
         * @brief Renders a PLC component.
         * PLC ?ŒëŒ„ë£??°ë“ƒ?????œ‘ï§ê³¹ë¹€??ˆë–.
         */
        void RenderPLCComponent(ImDrawList* draw_list, const PlacedComponent& comp,
                                ImVec2 screen_pos);
        /**
         * @brief Renders an FRL (Filter, Regulator, Lubricator) unit component.
         * FRL(?ê¾ªê½£, ??‡ë ??‰ì” ?? ??½ì†¢æ¹? ?ì¢Šë–… ?ŒëŒ„ë£??°ë“ƒ?????œ‘ï§ê³¹ë¹€??ˆë–.
         */
        void RenderFRLComponent(ImDrawList* draw_list, const PlacedComponent& comp,
                                ImVec2 screen_pos);
        /**
         * @brief Renders a manifold component.
         * ï§ã…»???€ë±??ŒëŒ„ë£??°ë“ƒ?????œ‘ï§ê³¹ë¹€??ˆë–.
         */
        void RenderManifoldComponent(ImDrawList* draw_list,
                                     const PlacedComponent& comp, ImVec2 screen_pos);
        /**
         * @brief Renders a limit switch component.
         * ?±Ñ?????¼ìç§??ŒëŒ„ë£??°ë“ƒ?????œ‘ï§ê³¹ë¹€??ˆë–.
         */
        void RenderLimitSwitchComponent(ImDrawList* draw_list,
                                        const PlacedComponent& comp,
                                        ImVec2 screen_pos);
        /**
         * @brief Renders a generic sensor component.
         * ??°ì»² ??±ê½Œ ?ŒëŒ„ë£??°ë“ƒ?????œ‘ï§ê³¹ë¹€??ˆë–.
         */
        void RenderSensorComponent(ImDrawList* draw_list, const PlacedComponent& comp,
                                   ImVec2 screen_pos);
        /**
         * @brief Renders a cylinder component.
         * ??»â”›???ŒëŒ„ë£??°ë“ƒ?????œ‘ï§ê³¹ë¹€??ˆë–.
         */
        void RenderCylinderComponent(ImDrawList* draw_list,
                                     const PlacedComponent& comp, ImVec2 screen_pos);
        /**
         * @brief Renders a single-solenoid valve component.
         * ??¤ë£ ?ë¶¾ì …?ëª„ì” ??è«›ëªƒ???ŒëŒ„ë£??°ë“ƒ?????œ‘ï§ê³¹ë¹€??ˆë–.
         */
        void RenderValveSingleComponent(ImDrawList* draw_list,
                                        const PlacedComponent& comp,
                                        ImVec2 screen_pos);
        /**
         * @brief Renders a double-solenoid valve component.
         * è¹‚ë“¬ë£??ë¶¾ì …?ëª„ì” ??è«›ëªƒ???ŒëŒ„ë£??°ë“ƒ?????œ‘ï§ê³¹ë¹€??ˆë–.
         */
        void RenderValveDoubleComponent(ImDrawList* draw_list,
                                        const PlacedComponent& comp,
                                        ImVec2 screen_pos);
        /**
         * @brief Renders a button unit component.
         * è¸°ê¾ª???ì¢Šë–… ?ŒëŒ„ë£??°ë“ƒ?????œ‘ï§ê³¹ë¹€??ˆë–.
         */
        void RenderButtonUnitComponent(ImDrawList* draw_list,
                                       const PlacedComponent& comp,
                                       ImVec2 screen_pos);
        /**
         * @brief Renders a power supply component.
         * ?ê¾©ì ?¨ë“¦???Î¼???ŒëŒ„ë£??°ë“ƒ?????œ‘ï§ê³¹ë¹€??ˆë–.
         */
        void RenderPowerSupplyComponent(ImDrawList* draw_list,
                                        const PlacedComponent& comp,
                                        ImVec2 screen_pos);

        /**
         * @brief Renders the main canvas for wiring and component placement.
         * è«›ê³—ê½?è«??ŒëŒ„ë£??°ë“ƒ è«›ê³—?‚ç‘œ??ê¾ªë¸³ ï§ë¶¿??ï§?¶¾ì¾??? ???œ‘ï§ê³¹ë¹€??ˆë–.
         */
        void RenderWiringCanvas();

        // RenderWiringCanvas helper functions
        // RenderWiringCanvas ??????¥ë‹”??

        /**
         * @brief Handles user interaction for adjusting FRL pressure.
         * FRL ?ëº£ì ° è­°ê³—????ê¾ªë¸³ ??????ê³¹ìƒ‡?ë¬’ìŠœ??ï§£ì„???¸ë•²??
         */
        bool HandleFRLPressureAdjustment(ImVec2 mouse_world_pos, const ImGuiIO& io);

        /**
         * @brief Handles camera zooming and panning on the canvas.
         * ï§?¶¾ì¾??¼ë¿‰??–ì“½ ç§»ë?ì°???ëº?/?°ëº¤??è«???€ë£??ï§£ì„???¸ë•²??
         */
        void HandleZoomAndPan(bool is_canvas_hovered, const ImGuiIO& io,
                              bool frl_handled, bool& wheel_handled);

        /**
         * @brief Renders the background grid on the canvas.
         * ï§?¶¾ì¾??¼ë¿‰ è«›ê³Œê¼?æ´¹ëªƒ???? ???œ‘ï§ê³¹ë¹€??ˆë–.
         */
        void RenderGrid(ImDrawList* draw_list);

        /**
         * @brief Handles component dropping and wire deletion based on user input.
         * ???????…ì °???ê³•â…¨ ?ŒëŒ„ë£??°ë“ƒ ??•âˆ¼ è«??ê¾©ê½‘ ???£ç‘œ?ï§£ì„???¸ë•²??
         */
        void HandleComponentDropAndWireDelete(bool is_canvas_hovered,
                                              ImVec2 mouse_world_pos,
                                              const ImGuiIO& io);

        /**
         * @brief Handles user interactions in wire editing mode.
         * ?ê¾©ê½‘ ?ëª„ì­› ï§â‘¤ë±?ë¨?½Œ????????ê³¹ìƒ‡?ë¬’ìŠœ??ï§£ì„???¸ë•²??
         */
        void HandleWireEditMode(ImVec2 mouse_world_pos);

        /**
         * @brief Handles user interactions in selection mode.
         * ?ì¢ê¹® ï§â‘¤ë±?ë¨?½Œ????????ê³¹ìƒ‡?ë¬’ìŠœ??ï§£ì„???¸ë•²??
         */
        void HandleSelectMode(bool is_canvas_hovered, ImVec2 mouse_world_pos);

        /**
         * @brief Handles user interactions in wire connection mode.
         * ?ê¾©ê½‘ ?ê³Œê» ï§â‘¤ë±?ë¨?½Œ????????ê³¹ìƒ‡?ë¬’ìŠœ??ï§£ì„???¸ë•²??
         */
        void HandleWireConnectionMode(bool is_canvas_hovered, ImVec2 mouse_world_pos);

        /**
         * @brief Renders the main content of the canvas (components, wires, etc.).
         * ï§?¶¾ì¾??¼ì“½ äº??„ì„‘?—ï§¥??ŒëŒ„ë£??°ë“ƒ, ?ê¾©ê½‘ ???????œ‘ï§ê³¹ë¹€??ˆë–.
         */
        void RenderCanvasContent(ImDrawList* draw_list, ImVec2 mouse_world_pos);

        /**
         * @brief Shows a tooltip with information about the port under the cursor.
         * ?Œã…¼ê½??ê¾¨ì˜’????ˆë’— ??????????ëº£ë‚«åª›Â€ ??¿ë¦¿ ??„ë˜»????–ë–†??¸ë•²??
         */
        void ShowPortTooltip(bool is_canvas_hovered, ImVec2 mouse_world_pos);

        /**
         * @brief Displays an overlay with help text for camera controls.
         * ç§»ë?ì°????–ë¼±???????ê¾?ï§???¿ë’ª?ë©? ??ˆë’— ??»ì¾­??‰ì” ????–ë–†??¸ë•²??
         */
        void ShowCameraHelpOverlay();

        /**
         * @brief Starts the process of drawing a new wire from a specified port.
         * ï§Â€?ëº£ë§‚ ????ë¨?½Œ ???ê¾©ê½‘??æ´¹ëªƒ????ê¾¨ì¤ˆ?ëª„ë’ª????–ì˜‰??¸ë•²??
         */
        void StartWireConnection(int componentId, int portId, ImVec2 portWorldPos);

        /**
         * @brief Updates the endpoint of the wire being drawn to the mouse position.
         * æ´¹ëªƒ??§?????ˆë’— ?ê¾©ê½‘????¹ì ??ï§ë‰????ê¾©íŠ‚æ¿???…ëœ²??„ë“ƒ??¸ë•²??
         */
        void UpdateWireDrawing(ImVec2 mousePos);

        /**
         * @brief Completes a wire connection to a target port.
         * ??????????????ê¾©ê½‘ ?ê³Œê»???ê¾¨ì¦º??¸ë•²??
         */
        void CompleteWireConnection(int componentId, int portId, ImVec2 portWorldPos);

        /**
         * @brief Renders all established wires on the canvas.
         * ï§?¶¾ì¾??¼ë¿‰ ??¼ì ™??ï§â‘¤ë±??ê¾©ê½‘?????œ‘ï§ê³¹ë¹€??ˆë–.
         */
        void RenderWires(ImDrawList* draw_list);

        /**
         * @brief Deletes a wire by its ID.
         * IDæ¿??ê¾©ê½‘???????¸ë•²??
         */
        void DeleteWire(int wireId);

        /**
         * @brief Handles a click on a waypoint of a wire.
         * ?ê¾©ê½‘????¥ì” ?????å¯ƒìŒ?€?? ??€???ï§£ì„???¸ë•²??
         */
        void HandleWayPointClick(ImVec2 worldPos, bool use_port_snap_only);

        /**
         * @brief Adds a new waypoint to the wire currently being drawn.
         * ?ê¾©ì˜± æ´¹ëªƒ??§?????ˆë’— ?ê¾©ê½‘??????¥ì” ????ëª? ?°ë¶½???¸ë•²??
         */
        void AddWayPoint(ImVec2 position);

        /**
         * @brief Renders the temporary wire being drawn by the user.
         * ????ë¨? æ´¹ëªƒ?æ€???ˆë’— ?ê¾©ë–† ?ê¾©ê½‘?????œ‘ï§ê³¹ë¹€??ˆë–.
         */
        void RenderTemporaryWire(ImDrawList* draw_list);

        /**
         * @brief Handles the selection of a wire.
         * ?ê¾©ê½‘ ?ì¢ê¹®??ï§£ì„???¸ë•²??
         */
        void HandleWireSelection(ImVec2 worldPos);

        /**
         * @brief Finds a wire at a given world position.
         * äºŒì‡±ë¼±ï§??ë¶¾ë±¶ ?ê¾©íŠ‚?ë¨?½Œ ?ê¾©ê½‘??ï§¡ì– ???ˆë–.
         */
        Wire* FindWireAtPosition(ImVec2 worldPos, float tolerance = 5.0f);

        /**
         * @brief Finds a waypoint in a wire at a given world position.
         * äºŒì‡±ë¼±ï§??ë¶¾ë±¶ ?ê¾©íŠ‚?ë¨?½Œ ?ê¾©ê½‘????¥ì” ????ëª? ï§¡ì– ???ˆë–.
         */
        int FindWayPointAtPosition(Wire& wire, ImVec2 worldPos, float radius = 8.0f);

        /**
         * @brief Checks if a connection between two ports is valid.
         * ??????åª›ê¾©???ê³Œê»???ì¢ìŠš??? ?ëº¤ì”¤??¸ë•²??
         */
        bool IsValidWireConnection(const Port& fromPort, const Port& toPort);

        /**
         * @brief Finds a port at a given world position.
         * äºŒì‡±ë¼±ï§??ë¶¾ë±¶ ?ê¾©íŠ‚?ë¨?½Œ ???ƒç‘œ?ï§¡ì– ???ˆë–.
         */
        Port* FindPortAtPosition(ImVec2 worldPos, int& outComponentId);

        /**
         * @brief Applies all enabled snap settings to a position.
         * ??–ê½¦?ë¶¾ë§‚ ï§â‘¤ë±???»ê¹„ ??¼ì ™???ê¾©íŠ‚???ê³¸ìŠœ??¸ë•²??
         */
        ImVec2 ApplySnap(ImVec2 position, bool isWirePoint = false);

        /**
         * @brief Snaps a position to the grid.
         * ?ê¾©íŠ‚??æ´¹ëªƒ???–ë¿‰ ??»ê¹„??¸ë•²??
         */
        ImVec2 ApplyGridSnap(ImVec2 position);

        /**
         * @brief Snaps a position to the nearest component port.
         * ?ê¾©íŠ‚??åª›Â€??åª›Â€æºëš¯???ŒëŒ„ë£??°ë“ƒ ???????»ê¹„??¸ë•²??
         */
        ImVec2 ApplyPortSnap(ImVec2 position);

        /**
         * @brief Snaps a position to align horizontally or vertically with a reference point.
         * æ¹²ê³—??ë¨?‚µ ??‘ë£Š ?ë¨?’— ??ì­…??°ì¤ˆ ?ëº£ì ¹??ë£„æ¿??ê¾©íŠ‚????»ê¹„??¸ë•²??
         */
        ImVec2 ApplyLineSnap(ImVec2 position, ImVec2 referencePoint,
                             bool force_orthogonal);

        /**
         * @brief Renders visual guides for snapping.
         * ??»ê¹„???ê¾ªë¸³ ??“ì»–??åª›Â€??€ë±¶ç‘œ????œ‘ï§ê³¹ë¹€??ˆë–.
         */
        void RenderSnapGuides(ImDrawList* draw_list, ImVec2 worldSnapPos);

        // Pointer to the main GLFW window.
        // ï§ë¶¿??GLFW ??ˆë£„?ê³—ë¿‰ ????????ê³—ì—¯??ˆë–.
        GLFWwindow* window_;

        // Current operating mode (e.g., Wiring, Programming).
        // ?ê¾©ì˜± ?ë¬ë£ ï§â‘¤ë±??? è«›ê³—ê½? ?ê¾¨ì¤ˆæ´¹ëªƒ?’è«›???…ë•²??
        Mode current_mode_;

        // Currently selected tool (e.g., Select, Wire).
        // ?ê¾©ì˜± ?ì¢ê¹®???ê¾§ë„(?? ?ì¢ê¹®, ?ê¾©ê½‘)??…ë•²??
        ToolType current_tool_;

        // Flag indicating if the main application loop is running.
        // ï§ë¶¿???ì¢ëµ†?±Ñ???ë€??·â‘¦ë´½åª›? ??½ë»¾ ä»¥ë¬’?¤ï§? ?????€?????˜’æ´¹ëª„???ˆë–.
        bool running_;

        // Flag indicating if the PLC simulation is running.
        // PLC ?????‰ì” ??ì”  ??½ë»¾ ä»¥ë¬’?¤ï§? ?????€?????˜’æ´¹ëª„???ˆë–.
        bool is_plc_running_;

        // Default window width and height constants.
        // æ¹²ê³•????ˆë£„????ˆí‰¬ è«??ë¯ªì”  ?ê³¸ë‹”??…ë•²??
        static constexpr int kWindowWidth = 1440;
        static constexpr int kWindowHeight = 1024;

        // Collection of all components placed in the workspace.
        // ?ë¬’ë¾½ ?¨ë“¦ì»??è«›ê³—???ï§â‘¤ë±??ŒëŒ„ë£??°ë“ƒ???ŒÑ‰ì †??ì—¯??ˆë–.
        std::vector<PlacedComponent> placed_components_;
        // ID of the currently selected component.
        // ?ê¾©ì˜± ?ì¢ê¹®???ŒëŒ„ë£??°ë“ƒ??ID??…ë•²??
        int selected_component_id_;
        // Counter to generate unique instance IDs for new components.
        // ???ŒëŒ„ë£??°ë“ƒ???ê¾ªë¸³ ?¨ì¢?€ ?ëª„ë’ª??ë’ª ID????¹ê½¦??ë’— ç§»ëŒ??ê³—ì—¯??ˆë–.
        int next_instance_id_;

        // State variables for component drag-and-drop and movement.
        // ?ŒëŒ„ë£??°ë“ƒ ??•ì˜’æ´?????•âˆ¼ è«???€ë£???ê¾ªë¸³ ?ê³¹ê¹­ è¹‚Â€??ë±¾??…ë•²??
        bool is_dragging_;
        int dragged_component_index_;
        bool is_moving_component_;
        int moving_component_id_;
        ImVec2 drag_start_offset_;

        // Collection of all wires in the workspace.
        // ?ë¬’ë¾½ ?¨ë“¦ì»????ˆë’— ï§â‘¤ë±??ê¾©ê½‘???ŒÑ‰ì †??ì—¯??ˆë–.
        std::vector<Wire> wires_;
        // Counter for generating unique wire IDs.
        // ?¨ì¢?€???ê¾©ê½‘ ID????¹ê½¦??ë¦° ?ê¾ªë¸³ ç§»ëŒ??ê³—ì—¯??ˆë–.
        int next_wire_id_;
        // ID of the currently selected wire.
        // ?ê¾©ì˜± ?ì¢ê¹®???ê¾©ê½‘??ID??…ë•²??
        int selected_wire_id_;

        // State variables for the interactive wire creation process.
        // ?ê³¹ìƒ‡?ë¬’ìŠœ?ê³¸ì”¤ ?ê¾©ê½‘ ??¹ê½¦ ?¨ì‡±????ê¾ªë¸³ ?ê³¹ê¹­ è¹‚Â€??ë±¾??…ë•²??
        bool is_connecting_;
        int wire_start_component_id_;
        int wire_start_port_id_;
        ImVec2 wire_start_pos_;
        ImVec2 wire_current_pos_;
        std::vector<Position> current_way_points_;
        Port temp_port_buffer_;

        // State variables for editing an existing wire's waypoints.
        // æ¹²ê³—???ê¾©ê½‘????¥ì” ????ëª? ?ëª„ì­›??ë¦° ?ê¾ªë¸³ ?ê³¹ê¹­ è¹‚Â€??ë±¾??…ë•²??
        WireEditMode wire_edit_mode_;
        int editing_wire_id_;
        int editing_point_index_;

        // State for the workspace camera's pan and zoom.
        // ?ë¬’ë¾½ ?¨ë“¦ì»?ç§»ë?ì°??±ì“½ ??€ë£?è«??ëº?/?°ëº¤?¼ç‘œ??ê¾ªë¸³ ?ê³¹ê¹­??…ë•²??
        ImVec2 camera_offset_;
        float camera_zoom_;
        ImVec2 canvas_top_left_;
        ImVec2 canvas_size_;

        // Configuration for snapping behavior.
        // ??»ê¹„ ??ˆì˜‰???????´ÑŠê½¦??…ë•²??
        SnapSettings snap_settings_;

        // Maps storing simulation state data for each component port.
        // åª??ŒëŒ„ë£??°ë“ƒ ??????????????‰ì” ???ê³¹ê¹­ ?ê³—ì” ?ê³? ???Î½ë¸??ï§ë“­???ˆë–.
        std::map<std::pair<int, int>, Position> port_positions_;
        std::map<std::pair<int, int>, float> port_voltages_;
        std::map<std::pair<int, int>, float> port_pressures_;

        // Data for the loaded ladder logic and the live state of PLC devices.
        // ?ºëˆ?????ëœ‘ æ¿¡ì’–ì­…æ€?PLC ?ë¶¾ì»®??ë’ª????¼ë–†åª??ê³¹ê¹­???????ê³—ì” ?ê³—ì—¯??ˆë–.
        LadderProgram loaded_ladder_program_;
        std::map<std::string, bool> plc_device_states_;
        std::map<std::string, TimerState> plc_timer_states_;
        std::map<std::string, CounterState> plc_counter_states_;

        // Smart pointers to the major subsystems of the application.
        // ?ì¢ëµ†?±Ñ???ë€??äºŒì‡±????ì ??–ë’ª??–ì“£ åª›Â€?±Ñ‹ê¶????»ì­??????ê³—ì—¯??ˆë–.
        std::unique_ptr<ProgrammingMode> programming_mode_;
        std::unique_ptr<PLCSimulatorCore> simulator_core_;
        PhysicsEngine* physics_engine_;
        std::unique_ptr<CompiledPLCExecutor> compiled_plc_executor_;
        std::unique_ptr<ProjectFileManager> project_file_manager_;

        // State variables for the real-time debugging and logging system.
        // ??¼ë–†åª??ë¶¾ì¾­æº?è«?æ¿¡ì’“????–ë’ª??–ì“£ ?ê¾ªë¸³ ?ê³¹ê¹­ è¹‚Â€??ë±¾??…ë•²??
        bool debug_mode_requested_;
        bool debug_mode_;
        bool enable_debug_logging_;
        int debug_update_counter_;
        std::string debug_log_buffer_;
        bool show_physics_warning_dialog_;
        std::string physics_warning_message_;
        std::mutex physics_warning_mutex_;
        ImVec2 last_pointer_world_pos_;
        double last_pointer_move_time_;
        double last_auto_waypoint_time_;
        bool touch_gesture_active_;
        float touch_zoom_delta_;
        ImVec2 touch_pan_delta_;
        bool last_pointer_is_pan_input_;
        ImVec2 touch_anchor_screen_pos_;
        bool prev_right_button_down_;
        bool prev_side_button_down_;
        bool win32_right_click_;
        bool win32_side_click_;
        bool win32_side_down_;

        /**
         * @brief Toggles the comprehensive debug mode on or off.
         * ?«ë‚‡ë¹€ ?ë¶¾ì¾­æ´?ï§â‘¤ë±¶ç‘œ??³ì’“êµ???ëº£ë•²??
         */
        void ToggleDebugMode();

        /**
         * @brief Updates and logs debug information to the console if enabled.
         * ??–ê½¦?ë¶¾ë§‚ å¯ƒìŒ???„ì„????ë¶¾ì¾­æ´??ëº£ë‚«????…ëœ²??„ë“ƒ??í€?æ¹²ê³•ì¤??¸ë•²??
         */
        void UpdateDebugLogging();

        /**
         * @brief Logs the state of all placed components to the debug buffer.
         * è«›ê³—???ï§â‘¤ë±??ŒëŒ„ë£??°ë“ƒ???ê³¹ê¹­???ë¶¾ì¾­æ´?è¸°ê¾ª???æ¹²ê³•ì¤??¸ë•²??
         */
        void LogComponentStates();

        /**
         * @brief Logs the overall state of the physics engine to the debug buffer.
         * ?¾ì‡°???ë¶¿ì­Š???ê¾¨ì»²?ê³¸ì”¤ ?ê³¹ê¹­???ë¶¾ì¾­æ´?è¸°ê¾ª???æ¹²ê³•ì¤??¸ë•²??
         */
        void LogPhysicsEngineStates();

        /**
         * @brief Logs the state of the electrical network to the debug buffer.
         * ?ê¾§ë¦° ??½ë“ƒ??°ê²•???ê³¹ê¹­???ë¶¾ì¾­æ´?è¸°ê¾ª???æ¹²ê³•ì¤??¸ë•²??
         */
        void LogElectricalNetwork();

        /**
         * @brief Logs the state of the pneumatic network to the debug buffer.
         * ?¨ë“­ë¸???½ë“ƒ??°ê²•???ê³¹ê¹­???ë¶¾ì¾­æ´?è¸°ê¾ª???æ¹²ê³•ì¤??¸ë•²??
         */
        void LogPneumaticNetwork();

        /**
         * @brief Logs the state of the mechanical systems (e.g., cylinders) to the debug buffer.
         * æ¹²ê³Œ????–ë’ª???? ??»â”›?????ê³¹ê¹­???ë¶¾ì¾­æ´?è¸°ê¾ª???æ¹²ê³•ì¤??¸ë•²??
         */
        void LogMechanicalSystem();

        /**
         * @brief Prints a message to the system console.
         * ??–ë’ª???„ì„???ï§ë¶¿?†ï§????°ì’•???¸ë•²??
         */
        void PrintDebugToConsole(const std::string& message);
    };

}  // namespace plc
#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_APPLICATION_H_
