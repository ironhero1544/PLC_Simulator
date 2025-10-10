// input_handler.h
// Input event handling and processing
// 입력 이벤트 처리

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_INPUT_HANDLER_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_INPUT_HANDLER_H_

struct GLFWwindow;

namespace plc {

// Handles keyboard and mouse input events
// 키보드 및 마우스 입력 이벤트 처리
class InputHandler {
 public:
  InputHandler();
  ~InputHandler();

  InputHandler(const InputHandler&) = delete;
  InputHandler& operator=(const InputHandler&) = delete;

  // Initialize input handling for given window
  // 주어진 윈도우에 대한 입력 처리 초기화
  void Initialize(GLFWwindow* window);

  // Process input events for current frame
  // 현재 프레임의 입력 이벤트 처리
  void ProcessInput();

  // Check if specific key is pressed
  // 특정 키가 눌렸는지 확인
  bool IsKeyPressed(int key) const;

  // Check if specific mouse button is pressed
  // 특정 마우스 버튼이 눌렸는지 확인
  bool IsMouseButtonPressed(int button) const;

  // Get current mouse position
  // 현재 마우스 위치 가져오기
  void GetMousePosition(double* x, double* y) const;

 private:
  GLFWwindow* window_;
};

}  // namespace plc

#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_INPUT_HANDLER_H_
