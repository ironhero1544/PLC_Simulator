// render_manager.h
// Rendering coordination and ImGui management
// 렌더링 조정 및 ImGui 관리

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_RENDER_MANAGER_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_RENDER_MANAGER_H_

struct GLFWwindow;

namespace plc {

// Manages rendering initialization and ImGui context
// 렌더링 초기화 및 ImGui 컨텍스트 관리
class RenderManager {
 public:
  RenderManager();
  ~RenderManager();

  RenderManager(const RenderManager&) = delete;
  RenderManager& operator=(const RenderManager&) = delete;

  // Initialize ImGui context and backends
  // ImGui 컨텍스트 및 백엔드 초기화
  bool Initialize(GLFWwindow* window);

  // Shutdown and cleanup ImGui resources
  // ImGui 리소스 정리 및 종료
  void Shutdown();

  // Start new ImGui frame
  // 새 ImGui 프레임 시작
  void BeginFrame();

  // End frame and render ImGui draw data
  // 프레임 종료 및 ImGui 드로우 데이터 렌더링
  void EndFrame();

  // Apply custom ImGui style configuration
  // 커스텀 ImGui 스타일 적용
  void SetupCustomStyle();

 private:
  bool initialized_;
};

}  // namespace plc

#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_RENDER_MANAGER_H_
