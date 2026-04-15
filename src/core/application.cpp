// application.cpp
// Implementation of main application class.

#include "plc_emulator/core/application.h"
#include "plc_emulator/components/state_keys.h"
#include "plc_emulator/core/component_transform.h"
#include "plc_emulator/core/win32_input.h"

#include "plc_emulator/components/component_registry.h"
#include "plc_emulator/lang/lang_manager.h"
#include "plc_emulator/core/ui_settings.h"



#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN

#ifndef NOMINMAX

#define NOMINMAX

#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif
#ifndef WINVER
#define WINVER 0x0602
#endif

#include <windows.h>

#include <commdlg.h>
#include <mmsystem.h>
#include <windowsx.h>

#endif



#include "imgui.h"

#include "imgui_impl_glfw.h"

#include "imgui_impl_opengl3.h"

#include "plc_emulator/programming/programming_mode.h"

#include "plc_emulator/project/ladder_to_ld_converter.h"

#include "plc_emulator/project/ld_to_ladder_converter.h"

#include "plc_emulator/project/openplc_compiler_integration.h"



#include <glad/glad.h>

#include <GLFW/glfw3.h>



#ifdef _WIN32

#define GLFW_EXPOSE_NATIVE_WIN32

#include <GLFW/glfw3native.h>

#endif



#include <algorithm>

#include <cmath>

#include <cstdio>

#include <cstring>

#include <fstream>

#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>



#include "nlohmann/json.hpp"



using json = nlohmann::json;

namespace plc {

namespace {
[[maybe_unused]] std::string GetLayoutSidecarPath(
    const std::string& project_path) {
  size_t last_slash = project_path.find_last_of("/\\");
  size_t last_dot = project_path.find_last_of('.');
  if (last_dot != std::string::npos &&
      (last_slash == std::string::npos || last_dot > last_slash)) {
    return project_path.substr(0, last_dot) + ".layout.json";
  }
  return project_path + ".layout.json";
}
}  // namespace





















































  

































































// =============================================================================


// =============================================================================









void Application::RenderPLCDebugPanel() {

  if (!programming_mode_)

    return;



  ImGuiIO& io = ImGui::GetIO();

  ImVec2 defaultPos(io.DisplaySize.x - 320.0f, 90.0f);

  ImGui::SetNextWindowPos(defaultPos, ImGuiCond_FirstUseEver);

  ImGui::SetNextWindowSize(ImVec2(300, 260), ImGuiCond_FirstUseEver);



  ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;

  if (ImGui::Begin("PLC Engine Status", nullptr, flags)) {
    {
      ImVec2 window_min = ImGui::GetWindowPos();
      ImVec2 window_size = ImGui::GetWindowSize();
      ImVec2 window_max(window_min.x + window_size.x, window_min.y + window_size.y);
      RegisterOverlayInputCaptureRect(
          window_min, window_max,
          ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) ||
              ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
    }

    const char* engineType = programming_mode_->GetEngineType();

    bool compiledLoaded = programming_mode_->HasCompiledCodeLoaded();

    bool needRecompile = programming_mode_->IsRecompileNeeded();
    char debug_buf[256] = {0};



    FormatString(debug_buf, sizeof(debug_buf),
                 "ui.debug.engine_fmt", "Engine: %s", engineType);
    ImGui::TextUnformatted(debug_buf);

    FormatString(debug_buf, sizeof(debug_buf),
                 "ui.debug.compiled_fmt", "Compiled Code: %s",
                 compiledLoaded ? TR("ui.debug.compiled_loaded", "Loaded")
                                : TR("ui.debug.compiled_not_loaded",
                                     "Not Loaded"));
    ImGui::TextUnformatted(debug_buf);

    FormatString(debug_buf, sizeof(debug_buf),
                 "ui.debug.recompile_fmt", "Recompile Needed: %s",
                 needRecompile ? TR("ui.common.yes", "Yes")
                               : TR("ui.common.no", "No"));
    ImGui::TextUnformatted(debug_buf);



    const std::string& lastCompileErr =

        programming_mode_->GetLastCompileError();

    if (!lastCompileErr.empty()) {

      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.2f, 0.2f, 1.0f));

      FormatString(debug_buf, sizeof(debug_buf),
                   "ui.debug.compile_error_fmt", "Compile Error: %s",
                   lastCompileErr.c_str());
      ImGui::TextWrapped("%s", debug_buf);

      ImGui::PopStyleColor();

    }



    ImGui::Separator();

    bool lastScanOk = programming_mode_->GetLastScanSuccess();

    int cycleUs = programming_mode_->GetLastCycleTimeUs();

    int instr = programming_mode_->GetLastInstructionCount();

    const std::string& scanErr = programming_mode_->GetLastScanError();



    FormatString(debug_buf, sizeof(debug_buf),
                 "ui.debug.last_scan_fmt", "Last Scan: %s",
                 lastScanOk ? TR("ui.common.ok", "OK")
                            : TR("ui.common.fail", "FAIL"));
    ImGui::TextUnformatted(debug_buf);
    FormatString(debug_buf, sizeof(debug_buf),
                 "ui.debug.cycle_time_fmt", "Cycle Time: %d us", cycleUs);
    ImGui::TextUnformatted(debug_buf);
    FormatString(debug_buf, sizeof(debug_buf),
                 "ui.debug.instructions_fmt", "Instructions: %d", instr);
    ImGui::TextUnformatted(debug_buf);

    if (!scanErr.empty()) {

      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.2f, 0.2f, 1.0f));

      FormatString(debug_buf, sizeof(debug_buf),
                   "ui.debug.scan_error_fmt", "Scan Error: %s",
                   scanErr.c_str());
      ImGui::TextWrapped("%s", debug_buf);

      ImGui::PopStyleColor();

    }



    ImGui::Separator();

    // GXWorks2 Normalization info

    bool gx2 = programming_mode_->IsGX2NormalizationEnabled();

    FormatString(debug_buf, sizeof(debug_buf),
                 "ui.debug.gx2_norm_fmt", "GX2 Normalize: %s",
                 gx2 ? TR("ui.common.enabled", "Enabled")
                     : TR("ui.common.disabled", "Disabled"));
    ImGui::TextUnformatted(debug_buf);

    if (gx2) {

      FormatString(debug_buf, sizeof(debug_buf),
                   "ui.debug.fix_count_fmt", "Fix Count: %d",
                   programming_mode_->GetNormalizationFixCount());
      ImGui::TextUnformatted(debug_buf);

      if (ImGui::CollapsingHeader("Normalization Summary",

                                  ImGuiTreeNodeFlags_DefaultOpen)) {

        const std::string& summary =

            programming_mode_->GetNormalizationSummary();

        if (summary.empty()) {

          ImGui::TextUnformatted(TR("ui.debug.no_fixes", "(no fixes)"));

        } else {

          ImGui::BeginChild("gx2_summary", ImVec2(0, 80), true);

          ImGui::TextUnformatted(summary.c_str());

          ImGui::EndChild();

        }

      }

    }

  }

  ImGui::End();

}

}  // namespace plc







