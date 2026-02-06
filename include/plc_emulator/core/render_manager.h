/*
 * render_manager.h
 *
 * 렌더링 초기화와 ImGui 컨텍스트 관리를 담당합니다.
 * Manages rendering initialization and the ImGui context.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_RENDER_MANAGER_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_RENDER_MANAGER_H_

struct GLFWwindow;

namespace plc {

/*
 * 렌더링 프레임 시작/종료와 스타일 설정을 담당합니다.
 * Owns frame setup/teardown and style configuration.
 */
class RenderManager {
 public:
  RenderManager();
  ~RenderManager();

  RenderManager(const RenderManager&) = delete;
  RenderManager& operator=(const RenderManager&) = delete;

  bool Initialize(GLFWwindow* window);
  void Shutdown();
  void BeginFrame();
  void EndFrame();
  void SetupCustomStyle();

 private:
  bool initialized_;
};

}  /* namespace plc */

#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_RENDER_MANAGER_H_ */
