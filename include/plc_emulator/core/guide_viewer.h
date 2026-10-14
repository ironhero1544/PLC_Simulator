#pragma once

#include <string>

namespace plc_emulator {

// Opens a modeless native reader, without pausing the simulator.
void OpenGuideViewer(const std::string& title, void* owner_window);
std::string GetGuideViewerError();

}  // namespace plc_emulator
