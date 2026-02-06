// main.cpp
//
// Application entry point.

#include "plc_emulator/core/application.h"

#include <cstdio>
#include <string>

int main(int argc, char* argv[]) {
  printf("=====================================\n");
  printf("FX3U PLC Simulator Starting...\n");
  printf("Current: Wiring Mode UI Complete\n");
  printf("Next: Drag & Drop Implementation\n");
  printf("=====================================\n");

  // Report unexpected failures and exit cleanly.
  try {
    bool enable_debug = false;
    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "--Debug" || arg == "--debug") {
        enable_debug = true;
        break;
      }
    }

    plc::Application app(enable_debug);

    if (!app.Initialize()) {
      printf("Failed to initialize application!\n");
      return -1;
    }

    printf("Application initialized successfully!\n");
    printf("Press ESC to exit...\n");

    app.Run();
    app.Shutdown();

    printf("Application terminated normally.\n");
    return 0;
  } catch (const std::exception& e) {
    printf("Exception caught: %s\n", e.what());
    return -1;
  }
}
