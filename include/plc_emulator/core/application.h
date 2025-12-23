// application.h

//
// Main application class for the PLC emulator.
// PLC ?먮??덉씠?곗쓽 硫붿씤 ?좏뵆由ъ??댁뀡 ?대옒?ㅼ엯?덈떎.
//
// This file manages the application lifecycle and coordinates mode switching.
// ???뚯씪? ?좏뵆由ъ??댁뀡???앸챸二쇨린瑜?愿由ы븯怨?紐⑤뱶 媛??꾪솚??議곗젙?⑸땲??
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
// ?꾩껜 PLC ?먮??덉씠???좏뵆由ъ??댁뀡??珥앷큵?섎뒗 硫붿씤 ?대옒?ㅼ엯?덈떎.
//
    class Application {
    public:
        /**
         * @brief Constructs the Application object and initializes default values.
         * Application 媛앹껜瑜??앹꽦?섍퀬 湲곕낯媛믪쓣 珥덇린?뷀빀?덈떎.
         */
        Application();

        /**
         * @brief Destructor. Ensures all resources are cleaned up properly.
         * ?뚮㈇?? 紐⑤뱺 由ъ냼?ㅺ? ?щ컮瑜닿쾶 ?뺣━?섎룄濡?蹂댁옣?⑸땲??
         */
        ~Application();

        // Disable copy and assignment to prevent accidental duplication.
        // ?섎룄?섏? ?딆? 蹂듭젣瑜?諛⑹??섍린 ?꾪빐 蹂듭궗 諛??좊떦??鍮꾪솢?깊솕?⑸땲??
        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        /**
         * @brief Initializes all subsystems, including the window, GUI, and simulator.
         * ?덈룄?? GUI, ?쒕??덉씠?곕? ?ы븿??紐⑤뱺 ?섏쐞 ?쒖뒪?쒖쓣 珥덇린?뷀빀?덈떎.
         * @return True if initialization is successful, false otherwise.
         * 珥덇린?붿뿉 ?깃났?섎㈃ true, 洹몃젃吏 ?딆쑝硫?false瑜?諛섑솚?⑸땲??
         */
        bool Initialize();

        /**
         * @brief Starts and runs the main application loop.
         * 硫붿씤 ?좏뵆由ъ??댁뀡 猷⑦봽瑜??쒖옉?섍퀬 ?ㅽ뻾?⑸땲??
         */
        void Run();

        /**
         * @brief Shuts down the application and releases all resources.
         * ?좏뵆由ъ??댁뀡??醫낅즺?섍퀬 紐⑤뱺 由ъ냼?ㅻ? ?댁젣?⑸땲??
         */
        void Shutdown();

        /**
         * @brief Saves the current project state to a .csv file.
         * ?꾩옱 ?꾨줈?앺듃 ?곹깭瑜?.csv ?뚯씪????ν빀?덈떎.
         * @param filePath The path to save the project file.
         * filePath???꾨줈?앺듃 ?뚯씪????ν븷 寃쎈줈?낅땲??
         * @param projectName An optional name for the project.
         * projectName? ?꾨줈?앺듃???좏깮???대쫫?낅땲??
         * @return True on successful save, false otherwise.
         * ??μ뿉 ?깃났?섎㈃ true, 洹몃젃吏 ?딆쑝硫?false瑜?諛섑솚?⑸땲??
         */
        bool SaveProject(const std::string& filePath,
                         const std::string& projectName = "");

        /**
         * @brief Loads a project from a .csv file.
         * .csv ?뚯씪濡쒕????꾨줈?앺듃瑜?遺덈윭?듬땲??
         * @param filePath The path of the project file to load.
         * filePath??遺덈윭???꾨줈?앺듃 ?뚯씪??寃쎈줈?낅땲??
         * @return True on successful load, false otherwise.
         * 遺덈윭?ㅺ린???깃났?섎㈃ true, 洹몃젃吏 ?딆쑝硫?false瑜?諛섑솚?⑸땲??
         */
        bool LoadProject(const std::string& filePath);

    private:
        /**
         * @brief Initializes the GLFW window.
         * GLFW ?덈룄?곕? 珥덇린?뷀빀?덈떎.
         */
        bool InitializeWindow();

        /**
         * @brief Initializes the ImGui context.
         * ImGui 而⑦뀓?ㅽ듃瑜?珥덇린?뷀빀?덈떎.
         */
        bool InitializeImGui();

        /**
         * @brief Sets up a custom visual style for ImGui.
         * ImGui??????ъ슜???뺤쓽 鍮꾩＜???ㅽ??쇱쓣 ?ㅼ젙?⑸땲??
         */
        void SetupCustomStyle();

        /**
         * @brief Processes user input for the current frame.
         * ?꾩옱 ?꾨젅?꾩뿉 ????ъ슜???낅젰??泥섎━?⑸땲??
         */
        void ProcessInput();

        /**
         * @brief Main update function called once per frame.
         * ?꾨젅?꾨떦 ??踰??몄텧?섎뒗 硫붿씤 ?낅뜲?댄듃 ?⑥닔?낅땲??
         */
        void Update();

        /**
         * @brief Updates the physics simulation state.
         * 臾쇰━ ?쒕??덉씠???곹깭瑜??낅뜲?댄듃?⑸땲??
         */
        void UpdatePhysics();

        /**
         * @brief Simulates basic physics phenomena, independent of the PLC state.
         * PLC ?곹깭? ?낅┰?곸쑝濡?湲곕낯?곸씤 臾쇰━ ?꾩긽???쒕??덉씠?섑빀?덈떎.
         */
        void UpdateBasicPhysics();

        /**
         * @brief Executes one scan of the loaded ladder logic program.
         * 遺덈윭???섎뜑 濡쒖쭅 ?꾨줈洹몃옩?????ㅼ틪???ㅽ뻾?⑸땲??
         */
        void SimulateLoadedLadder();

        /**
         * @brief Gets the current state of a PLC device (e.g., input, output).
         * PLC ?붾컮?댁뒪(?? ?낅젰, 異쒕젰)???꾩옱 ?곹깭瑜?媛?몄샃?덈떎.
         */
        bool GetPlcDeviceState(const std::string& address);

        /**
         * @brief Sets the state of a PLC device.
         * PLC ?붾컮?댁뒪???곹깭瑜??ㅼ젙?⑸땲??
         */
        void SetPlcDeviceState(const std::string& address, bool state);

        /**
         * @brief Loads a ladder program from a specified .ld file.
         * 吏?뺣맂 .ld ?뚯씪濡쒕????섎뜑 ?꾨줈洹몃옩??遺덈윭?듬땲??
         * @param filepath Path to the .ld file.
         * filepath??.ld ?뚯씪??寃쎈줈?낅땲??
         */
        void LoadLadderProgramFromLD(const std::string& filepath);

        /**
         * @brief Converts a string representation of an instruction to its enum type.
         * 紐낅졊?댁쓽 臾몄옄???쒗쁽???대떦 ?닿굅????낆쑝濡?蹂?섑빀?덈떎.
         */
        LadderInstructionType StringToInstructionType(const std::string& str);

        /**
         * @brief Synchronizes the ladder program from the programming mode to the simulator.
         * ?꾨줈洹몃옒諛?紐⑤뱶???섎뜑 ?꾨줈洹몃옩???쒕??덉씠?곕줈 ?숆린?뷀빀?덈떎.
         */
        void SyncLadderProgramFromProgrammingMode();

        /**
         * @brief Creates a default ladder program for testing and initialization.
         * ?뚯뒪??諛?珥덇린?붾? ?꾪빐 湲곕낯 ?섎뜑 ?꾨줈洹몃옩???앹꽦?⑸땲??
         */
        void CreateDefaultTestLadderProgram();

        /**
         * @brief Compiles the current ladder program and loads it into the PLC executor.
         * ?꾩옱 ?섎뜑 ?꾨줈洹몃옩??而댄뙆?쇳븯怨?PLC ?ㅽ뻾湲곗뿉 濡쒕뱶?⑸땲??
         */
        void CompileAndLoadLadderProgram();

        /**
         * @brief Simulates the electrical network connecting components.
         * 而댄룷?뚰듃瑜??곌껐?섎뒗 ?꾧린 ?ㅽ듃?뚰겕瑜??쒕??덉씠?섑빀?덈떎.
         */
        void SimulateElectrical();

        /**
         * @brief Updates the internal logic of placed components.
         * 諛곗튂??而댄룷?뚰듃???대? 濡쒖쭅???낅뜲?댄듃?⑸땲??
         */
        void UpdateComponentLogic();

        /**
         * @brief Simulates the pneumatic network connecting components.
         * 而댄룷?뚰듃瑜??곌껐?섎뒗 怨듭븬 ?ㅽ듃?뚰겕瑜??쒕??덉씠?섑빀?덈떎.
         */
        void SimulatePneumatic();

        /**
         * @brief Updates the state of actuators (e.g., cylinders) based on simulation.
         * ?쒕??덉씠??寃곌낵???곕씪 ?≪텛?먯씠???? ?ㅻ┛?????곹깭瑜??낅뜲?댄듃?⑸땲??
         */
        void UpdateActuators();

        /**
         * @brief Retrieves the port information for a given component.
         * 二쇱뼱吏?而댄룷?뚰듃???ы듃 ?뺣낫瑜?媛?몄샃?덈떎.
         */
        std::vector<std::pair<int, bool>> GetPortsForComponent(
                const PlacedComponent& comp);

        /**
         * @brief Synchronizes PLC output states to the physics engine's electrical network.
         * PLC 異쒕젰 ?곹깭瑜?臾쇰━ ?붿쭊???꾧린 ?ㅽ듃?뚰겕? ?숆린?뷀빀?덈떎.
         */
        void SyncPLCOutputsToPhysicsEngine();

        /**
         * @brief Synchronizes results from the physics engine back to the application state.
         * 臾쇰━ ?붿쭊??寃곌낵瑜??좏뵆由ъ??댁뀡 ?곹깭濡??ㅼ떆 ?숆린?뷀빀?덈떎.
         */
        void SyncPhysicsEngineToApplication();

        /**
         * @brief Updates and displays physics engine performance statistics.
         * 臾쇰━ ?붿쭊???깅뒫 ?듦퀎瑜??낅뜲?댄듃?섍퀬 ?쒖떆?⑸땲??
         */
        void UpdatePhysicsPerformanceStats();

        /**
         * @brief Renders the entire application scene for the current frame.
         * ?꾩옱 ?꾨젅?꾩뿉 ????꾩껜 ?좏뵆由ъ??댁뀡 ?λ㈃???뚮뜑留곹빀?덈떎.
         */
        void Render();

        /**
         * @brief Cleans up resources before shutting down.
         * 醫낅즺?섍린 ?꾩뿉 由ъ냼?ㅻ? ?뺣━?⑸땲??
         */
        void Cleanup();

        /**
         * @brief Updates the screen positions of all component ports.
         * 紐⑤뱺 而댄룷?뚰듃 ?ы듃???붾㈃ ?꾩튂瑜??낅뜲?댄듃?⑸땲??
         */
        void UpdatePortPositions();

        /**
         * @brief Renders the main user interface using ImGui.
         * ImGui瑜??ъ슜?섏뿬 硫붿씤 ?ъ슜???명꽣?섏씠?ㅻ? ?뚮뜑留곹빀?덈떎.
         */
        void RenderUI();

        /**
         * @brief Renders the UI specific to the wiring mode.
         * 諛곗꽑 紐⑤뱶???뱁솕??UI瑜??뚮뜑留곹빀?덈떎.
         */
        void RenderWiringModeUI();

        /**
         * @brief Renders the application header bar.
         * ?좏뵆由ъ??댁뀡 ?ㅻ뜑 諛붾? ?뚮뜑留곹빀?덈떎.
         */
        void RenderHeader();

        /**
         * @brief Renders the toolbar with mode and tool selection.
         * 紐⑤뱶 諛??꾧뎄 ?좏깮 湲곕뒫???덈뒗 ?대컮瑜??뚮뜑留곹빀?덈떎.
         */
        void RenderToolbar();

        /**
         * @brief Renders the main content area of the application.
         * ?좏뵆由ъ??댁뀡??二?肄섑뀗痢??곸뿭???뚮뜑留곹빀?덈떎.
         */
        void RenderMainArea();

        /**
         * @brief Renders the real-time PLC debug panel.
         * ?ㅼ떆媛?PLC ?붾쾭洹??⑤꼸???뚮뜑留곹빀?덈떎.
         */
        void RenderPLCDebugPanel();

        /**
         * @brief Converts world coordinates to screen coordinates.
         * ?붾뱶 醫뚰몴瑜??붾㈃ 醫뚰몴濡?蹂?섑빀?덈떎.
         */
        ImVec2 WorldToScreen(const ImVec2& world_pos) const;

        /**
         * @brief Converts screen coordinates to world coordinates.
         * ?붾㈃ 醫뚰몴瑜??붾뱶 醫뚰몴濡?蹂?섑빀?덈떎.
         */
        ImVec2 ScreenToWorld(const ImVec2& screen_pos) const;

        /**
         * @brief Renders the list of available components.
         * ?ъ슜 媛?ν븳 而댄룷?뚰듃 紐⑸줉???뚮뜑留곹빀?덈떎.
         */
        void RenderComponentList();

        /**
         * @brief Renders all components placed in the workspace.
         * ?묒뾽 怨듦컙??諛곗튂??紐⑤뱺 而댄룷?뚰듃瑜??뚮뜑留곹빀?덈떎.
         */
        void RenderPlacedComponents(ImDrawList* draw_list);

        /**
         * @brief Handles the start of a component drag operation from the component list.
         * 而댄룷?뚰듃 紐⑸줉?먯꽌 而댄룷?뚰듃 ?쒕옒洹??묒뾽 ?쒖옉??泥섎━?⑸땲??
         */
        void HandleComponentDragStart(int componentIndex);

        /**
         * @brief Handles the ongoing component drag operation.
         * 吏꾪뻾 以묒씤 而댄룷?뚰듃 ?쒕옒洹??묒뾽??泥섎━?⑸땲??
         */
        void HandleComponentDrag();

        /**
         * @brief Handles dropping a component onto the workspace.
         * ?묒뾽 怨듦컙??而댄룷?뚰듃瑜??쒕∼?섎뒗 ?묒뾽??泥섎━?⑸땲??
         */
        void HandleComponentDrop(Position position);

        /**
         * @brief Handles the selection of a placed component.
         * 諛곗튂??而댄룷?뚰듃???좏깮??泥섎━?⑸땲??
         */
        void HandleComponentSelection(int instanceId);

        /**
         * @brief Deletes the currently selected component.
         * ?꾩옱 ?좏깮??而댄룷?뚰듃瑜???젣?⑸땲??
         */
        void DeleteSelectedComponent();

        /**
         * @brief Handles the start of moving an already placed component.
         * ?대? 諛곗튂??而댄룷?뚰듃???대룞 ?쒖옉??泥섎━?⑸땲??
         */
        void HandleComponentMoveStart(int instanceId, ImVec2 mousePos);

        /**
         * @brief Handles the end of a component move operation.
         * 而댄룷?뚰듃 ?대룞 ?묒뾽??醫낅즺瑜?泥섎━?⑸땲??
         */
        void HandleComponentMoveEnd();

        /**
         * @brief Renders a PLC component.
         * PLC 而댄룷?뚰듃瑜??뚮뜑留곹빀?덈떎.
         */
        void RenderPLCComponent(ImDrawList* draw_list, const PlacedComponent& comp,
                                ImVec2 screen_pos);
        /**
         * @brief Renders an FRL (Filter, Regulator, Lubricator) unit component.
         * FRL(?꾪꽣, ?덇랠?덉씠?? ?ㅽ솢湲? ?좊떅 而댄룷?뚰듃瑜??뚮뜑留곹빀?덈떎.
         */
        void RenderFRLComponent(ImDrawList* draw_list, const PlacedComponent& comp,
                                ImVec2 screen_pos);
        /**
         * @brief Renders a manifold component.
         * 留ㅻ땲?대뱶 而댄룷?뚰듃瑜??뚮뜑留곹빀?덈떎.
         */
        void RenderManifoldComponent(ImDrawList* draw_list,
                                     const PlacedComponent& comp, ImVec2 screen_pos);
        /**
         * @brief Renders a limit switch component.
         * 由щ????ㅼ쐞移?而댄룷?뚰듃瑜??뚮뜑留곹빀?덈떎.
         */
        void RenderLimitSwitchComponent(ImDrawList* draw_list,
                                        const PlacedComponent& comp,
                                        ImVec2 screen_pos);
        /**
         * @brief Renders a generic sensor component.
         * ?쇰컲 ?쇱꽌 而댄룷?뚰듃瑜??뚮뜑留곹빀?덈떎.
         */
        void RenderSensorComponent(ImDrawList* draw_list, const PlacedComponent& comp,
                                   ImVec2 screen_pos);
        /**
         * @brief Renders a cylinder component.
         * ?ㅻ┛??而댄룷?뚰듃瑜??뚮뜑留곹빀?덈떎.
         */
        void RenderCylinderComponent(ImDrawList* draw_list,
                                     const PlacedComponent& comp, ImVec2 screen_pos);
        /**
         * @brief Renders a single-solenoid valve component.
         * ?⑤룞 ?붾젅?몄씠??諛몃툕 而댄룷?뚰듃瑜??뚮뜑留곹빀?덈떎.
         */
        void RenderValveSingleComponent(ImDrawList* draw_list,
                                        const PlacedComponent& comp,
                                        ImVec2 screen_pos);
        /**
         * @brief Renders a double-solenoid valve component.
         * 蹂듬룞 ?붾젅?몄씠??諛몃툕 而댄룷?뚰듃瑜??뚮뜑留곹빀?덈떎.
         */
        void RenderValveDoubleComponent(ImDrawList* draw_list,
                                        const PlacedComponent& comp,
                                        ImVec2 screen_pos);
        /**
         * @brief Renders a button unit component.
         * 踰꾪듉 ?좊떅 而댄룷?뚰듃瑜??뚮뜑留곹빀?덈떎.
         */
        void RenderButtonUnitComponent(ImDrawList* draw_list,
                                       const PlacedComponent& comp,
                                       ImVec2 screen_pos);
        /**
         * @brief Renders a power supply component.
         * ?꾩썝 怨듦툒 ?μ튂 而댄룷?뚰듃瑜??뚮뜑留곹빀?덈떎.
         */
        void RenderPowerSupplyComponent(ImDrawList* draw_list,
                                        const PlacedComponent& comp,
                                        ImVec2 screen_pos);

        /**
         * @brief Renders the main canvas for wiring and component placement.
         * 諛곗꽑 諛?而댄룷?뚰듃 諛곗튂瑜??꾪븳 硫붿씤 罹붾쾭?ㅻ? ?뚮뜑留곹빀?덈떎.
         */
        void RenderWiringCanvas();

        // RenderWiringCanvas helper functions
        // RenderWiringCanvas ?ы띁 ?⑥닔??

        /**
         * @brief Handles user interaction for adjusting FRL pressure.
         * FRL ?뺣젰 議곗젙???꾪븳 ?ъ슜???곹샇?묒슜??泥섎━?⑸땲??
         */
        bool HandleFRLPressureAdjustment(ImVec2 mouse_world_pos, const ImGuiIO& io);

        /**
         * @brief Handles camera zooming and panning on the canvas.
         * 罹붾쾭?ㅼ뿉?쒖쓽 移대찓???뺣?/異뺤냼 諛??대룞??泥섎━?⑸땲??
         */
        void HandleZoomAndPan(bool is_canvas_hovered, const ImGuiIO& io,
                              bool frl_handled, bool& wheel_handled);

        /**
         * @brief Renders the background grid on the canvas.
         * 罹붾쾭?ㅼ뿉 諛곌꼍 洹몃━?쒕? ?뚮뜑留곹빀?덈떎.
         */
        void RenderGrid(ImDrawList* draw_list);

        /**
         * @brief Handles component dropping and wire deletion based on user input.
         * ?ъ슜???낅젰???곕Ⅸ 而댄룷?뚰듃 ?쒕∼ 諛??꾩꽑 ??젣瑜?泥섎━?⑸땲??
         */
        void HandleComponentDropAndWireDelete(bool is_canvas_hovered,
                                              ImVec2 mouse_world_pos);

        /**
         * @brief Handles user interactions in wire editing mode.
         * ?꾩꽑 ?몄쭛 紐⑤뱶?먯꽌???ъ슜???곹샇?묒슜??泥섎━?⑸땲??
         */
        void HandleWireEditMode(ImVec2 mouse_world_pos);

        /**
         * @brief Handles user interactions in selection mode.
         * ?좏깮 紐⑤뱶?먯꽌???ъ슜???곹샇?묒슜??泥섎━?⑸땲??
         */
        void HandleSelectMode(bool is_canvas_hovered, ImVec2 mouse_world_pos);

        /**
         * @brief Handles user interactions in wire connection mode.
         * ?꾩꽑 ?곌껐 紐⑤뱶?먯꽌???ъ슜???곹샇?묒슜??泥섎━?⑸땲??
         */
        void HandleWireConnectionMode(bool is_canvas_hovered, ImVec2 mouse_world_pos);

        /**
         * @brief Renders the main content of the canvas (components, wires, etc.).
         * 罹붾쾭?ㅼ쓽 二?肄섑뀗痢?而댄룷?뚰듃, ?꾩꽑 ??瑜??뚮뜑留곹빀?덈떎.
         */
        void RenderCanvasContent(ImDrawList* draw_list, ImVec2 mouse_world_pos);

        /**
         * @brief Shows a tooltip with information about the port under the cursor.
         * 而ㅼ꽌 ?꾨옒???덈뒗 ?ы듃??????뺣낫媛 ?닿릿 ?댄똻???쒖떆?⑸땲??
         */
        void ShowPortTooltip(bool is_canvas_hovered, ImVec2 mouse_world_pos);

        /**
         * @brief Displays an overlay with help text for camera controls.
         * 移대찓???쒖뼱??????꾩?留??띿뒪?멸? ?덈뒗 ?ㅻ쾭?덉씠瑜??쒖떆?⑸땲??
         */
        void ShowCameraHelpOverlay();

        /**
         * @brief Starts the process of drawing a new wire from a specified port.
         * 吏?뺣맂 ?ы듃?먯꽌 ???꾩꽑??洹몃━???꾨줈?몄뒪瑜??쒖옉?⑸땲??
         */
        void StartWireConnection(int componentId, int portId, ImVec2 portWorldPos);

        /**
         * @brief Updates the endpoint of the wire being drawn to the mouse position.
         * 洹몃젮吏怨??덈뒗 ?꾩꽑???앹젏??留덉슦???꾩튂濡??낅뜲?댄듃?⑸땲??
         */
        void UpdateWireDrawing(ImVec2 mousePos);

        /**
         * @brief Completes a wire connection to a target port.
         * ????ы듃??????꾩꽑 ?곌껐???꾨즺?⑸땲??
         */
        void CompleteWireConnection(int componentId, int portId, ImVec2 portWorldPos);

        /**
         * @brief Renders all established wires on the canvas.
         * 罹붾쾭?ㅼ뿉 ?ㅼ젙??紐⑤뱺 ?꾩꽑???뚮뜑留곹빀?덈떎.
         */
        void RenderWires(ImDrawList* draw_list);

        /**
         * @brief Deletes a wire by its ID.
         * ID濡??꾩꽑????젣?⑸땲??
         */
        void DeleteWire(int wireId);

        /**
         * @brief Handles a click on a waypoint of a wire.
         * ?꾩꽑???⑥씠?ъ씤??寃쎌쑀?? ?대┃??泥섎━?⑸땲??
         */
        void HandleWayPointClick(ImVec2 worldPos);

        /**
         * @brief Adds a new waypoint to the wire currently being drawn.
         * ?꾩옱 洹몃젮吏怨??덈뒗 ?꾩꽑?????⑥씠?ъ씤?몃? 異붽??⑸땲??
         */
        void AddWayPoint(ImVec2 position);

        /**
         * @brief Renders the temporary wire being drawn by the user.
         * ?ъ슜?먭? 洹몃━怨??덈뒗 ?꾩떆 ?꾩꽑???뚮뜑留곹빀?덈떎.
         */
        void RenderTemporaryWire(ImDrawList* draw_list);

        /**
         * @brief Handles the selection of a wire.
         * ?꾩꽑 ?좏깮??泥섎━?⑸땲??
         */
        void HandleWireSelection(ImVec2 worldPos);

        /**
         * @brief Finds a wire at a given world position.
         * 二쇱뼱吏??붾뱶 ?꾩튂?먯꽌 ?꾩꽑??李얠뒿?덈떎.
         */
        Wire* FindWireAtPosition(ImVec2 worldPos, float tolerance = 5.0f);

        /**
         * @brief Finds a waypoint in a wire at a given world position.
         * 二쇱뼱吏??붾뱶 ?꾩튂?먯꽌 ?꾩꽑???⑥씠?ъ씤?몃? 李얠뒿?덈떎.
         */
        int FindWayPointAtPosition(Wire& wire, ImVec2 worldPos, float radius = 8.0f);

        /**
         * @brief Checks if a connection between two ports is valid.
         * ???ы듃 媛꾩쓽 ?곌껐???좏슚?쒖? ?뺤씤?⑸땲??
         */
        bool IsValidWireConnection(const Port& fromPort, const Port& toPort);

        /**
         * @brief Finds a port at a given world position.
         * 二쇱뼱吏??붾뱶 ?꾩튂?먯꽌 ?ы듃瑜?李얠뒿?덈떎.
         */
        Port* FindPortAtPosition(ImVec2 worldPos, int& outComponentId);

        /**
         * @brief Applies all enabled snap settings to a position.
         * ?쒖꽦?붾맂 紐⑤뱺 ?ㅻ깄 ?ㅼ젙???꾩튂???곸슜?⑸땲??
         */
        ImVec2 ApplySnap(ImVec2 position, bool isWirePoint = false);

        /**
         * @brief Snaps a position to the grid.
         * ?꾩튂瑜?洹몃━?쒖뿉 ?ㅻ깄?⑸땲??
         */
        ImVec2 ApplyGridSnap(ImVec2 position);

        /**
         * @brief Snaps a position to the nearest component port.
         * ?꾩튂瑜?媛??媛源뚯슫 而댄룷?뚰듃 ?ы듃???ㅻ깄?⑸땲??
         */
        ImVec2 ApplyPortSnap(ImVec2 position);

        /**
         * @brief Snaps a position to align horizontally or vertically with a reference point.
         * 湲곗??먭낵 ?섑룊 ?먮뒗 ?섏쭅?쇰줈 ?뺣젹?섎룄濡??꾩튂瑜??ㅻ깄?⑸땲??
         */
        ImVec2 ApplyLineSnap(ImVec2 position, ImVec2 referencePoint);

        /**
         * @brief Renders visual guides for snapping.
         * ?ㅻ깄???꾪븳 ?쒓컖??媛?대뱶瑜??뚮뜑留곹빀?덈떎.
         */
        void RenderSnapGuides(ImDrawList* draw_list, ImVec2 worldSnapPos);

        // Pointer to the main GLFW window.
        // 硫붿씤 GLFW ?덈룄?곗뿉 ????ъ씤?곗엯?덈떎.
        GLFWwindow* window_;

        // Current operating mode (e.g., Wiring, Programming).
        // ?꾩옱 ?묐룞 紐⑤뱶(?? 諛곗꽑, ?꾨줈洹몃옒諛??낅땲??
        Mode current_mode_;

        // Currently selected tool (e.g., Select, Wire).
        // ?꾩옱 ?좏깮???꾧뎄(?? ?좏깮, ?꾩꽑)?낅땲??
        ToolType current_tool_;

        // Flag indicating if the main application loop is running.
        // 硫붿씤 ?좏뵆由ъ??댁뀡 猷⑦봽媛 ?ㅽ뻾 以묒씤吏 ?섑??대뒗 ?뚮옒洹몄엯?덈떎.
        bool running_;

        // Flag indicating if the PLC simulation is running.
        // PLC ?쒕??덉씠?섏씠 ?ㅽ뻾 以묒씤吏 ?섑??대뒗 ?뚮옒洹몄엯?덈떎.
        bool is_plc_running_;

        // Default window width and height constants.
        // 湲곕낯 ?덈룄???덈퉬 諛??믪씠 ?곸닔?낅땲??
        static constexpr int kWindowWidth = 1440;
        static constexpr int kWindowHeight = 1024;

        // Collection of all components placed in the workspace.
        // ?묒뾽 怨듦컙??諛곗튂??紐⑤뱺 而댄룷?뚰듃??而щ젆?섏엯?덈떎.
        std::vector<PlacedComponent> placed_components_;
        // ID of the currently selected component.
        // ?꾩옱 ?좏깮??而댄룷?뚰듃??ID?낅땲??
        int selected_component_id_;
        // Counter to generate unique instance IDs for new components.
        // ??而댄룷?뚰듃瑜??꾪븳 怨좎쑀 ?몄뒪?댁뒪 ID瑜??앹꽦?섎뒗 移댁슫?곗엯?덈떎.
        int next_instance_id_;

        // State variables for component drag-and-drop and movement.
        // 而댄룷?뚰듃 ?쒕옒洹????쒕∼ 諛??대룞???꾪븳 ?곹깭 蹂?섎뱾?낅땲??
        bool is_dragging_;
        int dragged_component_index_;
        bool is_moving_component_;
        int moving_component_id_;
        ImVec2 drag_start_offset_;

        // Collection of all wires in the workspace.
        // ?묒뾽 怨듦컙???덈뒗 紐⑤뱺 ?꾩꽑??而щ젆?섏엯?덈떎.
        std::vector<Wire> wires_;
        // Counter for generating unique wire IDs.
        // 怨좎쑀???꾩꽑 ID瑜??앹꽦?섍린 ?꾪븳 移댁슫?곗엯?덈떎.
        int next_wire_id_;
        // ID of the currently selected wire.
        // ?꾩옱 ?좏깮???꾩꽑??ID?낅땲??
        int selected_wire_id_;

        // State variables for the interactive wire creation process.
        // ?곹샇?묒슜?곸씤 ?꾩꽑 ?앹꽦 怨쇱젙???꾪븳 ?곹깭 蹂?섎뱾?낅땲??
        bool is_connecting_;
        int wire_start_component_id_;
        int wire_start_port_id_;
        ImVec2 wire_start_pos_;
        ImVec2 wire_current_pos_;
        std::vector<Position> current_way_points_;
        Port temp_port_buffer_;

        // State variables for editing an existing wire's waypoints.
        // 湲곗〈 ?꾩꽑???⑥씠?ъ씤?몃? ?몄쭛?섍린 ?꾪븳 ?곹깭 蹂?섎뱾?낅땲??
        WireEditMode wire_edit_mode_;
        int editing_wire_id_;
        int editing_point_index_;

        // State for the workspace camera's pan and zoom.
        // ?묒뾽 怨듦컙 移대찓?쇱쓽 ?대룞 諛??뺣?/異뺤냼瑜??꾪븳 ?곹깭?낅땲??
        ImVec2 camera_offset_;
        float camera_zoom_;
        ImVec2 canvas_top_left_;
        ImVec2 canvas_size_;

        // Configuration for snapping behavior.
        // ?ㅻ깄 ?숈옉?????援ъ꽦?낅땲??
        SnapSettings snap_settings_;

        // Maps storing simulation state data for each component port.
        // 媛?而댄룷?뚰듃 ?ы듃??????쒕??덉씠???곹깭 ?곗씠?곕? ??ν븯??留듭엯?덈떎.
        std::map<std::pair<int, int>, Position> port_positions_;
        std::map<std::pair<int, int>, float> port_voltages_;
        std::map<std::pair<int, int>, float> port_pressures_;

        // Data for the loaded ladder logic and the live state of PLC devices.
        // 遺덈윭???섎뜑 濡쒖쭅怨?PLC ?붾컮?댁뒪???ㅼ떆媛??곹깭??????곗씠?곗엯?덈떎.
        LadderProgram loaded_ladder_program_;
        std::map<std::string, bool> plc_device_states_;
        std::map<std::string, TimerState> plc_timer_states_;
        std::map<std::string, CounterState> plc_counter_states_;

        // Smart pointers to the major subsystems of the application.
        // ?좏뵆由ъ??댁뀡??二쇱슂 ?섏쐞 ?쒖뒪?쒖쓣 媛由ы궎???ㅻ쭏???ъ씤?곗엯?덈떎.
        std::unique_ptr<ProgrammingMode> programming_mode_;
        std::unique_ptr<PLCSimulatorCore> simulator_core_;
        PhysicsEngine* physics_engine_;
        std::unique_ptr<CompiledPLCExecutor> compiled_plc_executor_;
        std::unique_ptr<ProjectFileManager> project_file_manager_;

        // State variables for the real-time debugging and logging system.
        // ?ㅼ떆媛??붾쾭源?諛?濡쒓퉭 ?쒖뒪?쒖쓣 ?꾪븳 ?곹깭 蹂?섎뱾?낅땲??
        bool debug_mode_;
        bool enable_debug_logging_;
        bool debug_key_pressed_;
        int debug_update_counter_;
        std::string debug_log_buffer_;

        /**
         * @brief Toggles the comprehensive debug mode on or off.
         * 醫낇빀 ?붾쾭洹?紐⑤뱶瑜?耳쒓굅???뺣땲??
         */
        void ToggleDebugMode();

        /**
         * @brief Updates and logs debug information to the console if enabled.
         * ?쒖꽦?붾맂 寃쎌슦 肄섏넄???붾쾭洹??뺣낫瑜??낅뜲?댄듃?섍퀬 湲곕줉?⑸땲??
         */
        void UpdateDebugLogging();

        /**
         * @brief Logs the state of all placed components to the debug buffer.
         * 諛곗튂??紐⑤뱺 而댄룷?뚰듃???곹깭瑜??붾쾭洹?踰꾪띁??湲곕줉?⑸땲??
         */
        void LogComponentStates();

        /**
         * @brief Logs the overall state of the physics engine to the debug buffer.
         * 臾쇰━ ?붿쭊???꾨컲?곸씤 ?곹깭瑜??붾쾭洹?踰꾪띁??湲곕줉?⑸땲??
         */
        void LogPhysicsEngineStates();

        /**
         * @brief Logs the state of the electrical network to the debug buffer.
         * ?꾧린 ?ㅽ듃?뚰겕???곹깭瑜??붾쾭洹?踰꾪띁??湲곕줉?⑸땲??
         */
        void LogElectricalNetwork();

        /**
         * @brief Logs the state of the pneumatic network to the debug buffer.
         * 怨듭븬 ?ㅽ듃?뚰겕???곹깭瑜??붾쾭洹?踰꾪띁??湲곕줉?⑸땲??
         */
        void LogPneumaticNetwork();

        /**
         * @brief Logs the state of the mechanical systems (e.g., cylinders) to the debug buffer.
         * 湲곌퀎 ?쒖뒪???? ?ㅻ┛?????곹깭瑜??붾쾭洹?踰꾪띁??湲곕줉?⑸땲??
         */
        void LogMechanicalSystem();

        /**
         * @brief Prints a message to the system console.
         * ?쒖뒪??肄섏넄??硫붿떆吏瑜?異쒕젰?⑸땲??
         */
        void PrintDebugToConsole(const std::string& message);
    };

}  // namespace plc
#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_APPLICATION_H_