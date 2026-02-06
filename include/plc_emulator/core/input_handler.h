/*
 * input_handler.h
 *
 * 입력 이벤트 처리 인터페이스 선언.
 * Declarations for input event handling.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_INPUT_HANDLER_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_INPUT_HANDLER_H_

struct GLFWwindow;

namespace plc {

/*
 * 키보드/마우스 입력 상태를 수집합니다.
 * Collects keyboard and mouse input state.
 */
class InputHandler {
 public:
  InputHandler();
  ~InputHandler();

  InputHandler(const InputHandler&) = delete;
  InputHandler& operator=(const InputHandler&) = delete;

  void Initialize(GLFWwindow* window);
  void ProcessInput();
  bool IsKeyPressed(int key) const;
  bool IsMouseButtonPressed(int button) const;
  void GetMousePosition(double* x, double* y) const;

 private:
  GLFWwindow* window_;
};

}  /* namespace plc */

#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_INPUT_HANDLER_H_ */
