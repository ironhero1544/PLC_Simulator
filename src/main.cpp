// main.cpp
//
// Application entry point.



// C++ 프로그램의 진입점인 main 함수를 구현합니다.
#include "plc_emulator/core/application.h"

#include <cstdio>
#include <string>

int main(int argc, char* argv[]) {
  printf("=====================================\n");
  printf("FX3U PLC Simulator Starting...\n");
  printf("Current: Wiring Mode UI Complete\n");
  printf("Next: Drag & Drop Implementation\n");
  printf("=====================================\n");

  // try-catch 구문을 사용하여 예외 처리를 수행합니다.
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

    //  if-else 문을 사용하여 초기화 성공 여부를 확인합니다.
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
