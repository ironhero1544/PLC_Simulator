// ladder_to_ld_converter.h
// Copyright 2024 PLC Emulator Project
//
// Converts ladder to LD format.

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_LADDER_TO_LD_CONVERTER_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_LADDER_TO_LD_CONVERTER_H_

#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace plc {

// 🔥 **FORWARD DECLARATIONS**: 순환 참조 방지
struct LadderProgram;
struct Rung;
struct LadderInstruction;
enum class LadderInstructionType;

// 🔥 **NEW**: LadderIR 전방 선언
class LadderIRProgram;
class IRToStackConverter;

/**
 * @brief GX Works2 스타일 래더 프로그램을 OpenPLC .ld 형식으로 변환하는 클래스
  * GX W또는ks2 스타일 래더 프로그램을 OpenPLC .ld 형식으로 변환하는 클래스
 *
 * 이 클래스는 기존 JSON 기반 래더 구조를 OpenPLC 컴파일러가 이해할 수 있는
 * .ld 파일 형식으로 변환합니다.
 *
 * 주요 변환 작업:
 * - verticalConnections → OR 블록 구조
 * - XIC, XIO, OTE → OpenPLC 명령어
 * - FX3U-32M I/O 매핑 (X0~X15, Y0~Y15)
 */
class LadderToLDConverter {
 public:
  LadderToLDConverter();
  ~LadderToLDConverter();

  /**
   * @brief 래더 프로그램을 .ld 파일로 변환하여 저장
   * @param program 변환할 래더 프로그램
   * @param outputPath 출력할 .ld 파일 경로
   * @return 성공 시 true, 실패 시 false
    * 성공 시 참, 실패 시 거짓
   */
  bool ConvertToLDFile(const LadderProgram& program,
                       const std::string& outputPath);

  /**
   * @brief 래더 프로그램을 .ld 형식 문자열로 변환
   * @param program 변환할 래더 프로그램
   * @return .ld 형식 문자열
   */
  std::string ConvertToLDString(const LadderProgram& program);

  /**
   * @brief 변환 과정에서 발생한 오류 메시지 반환
   * @return 오류 메시지 문자열
   */
  const std::string& GetLastError() const { return last_error_; }

  /**
   * @brief 디버그 모드 설정 (변환 과정 상세 로그 출력)
   * @param enable 디버그 모드 활성화 여부
   */
  void SetDebugMode(bool enable) { debug_mode_ = enable; }

  // 🔥 **NEW**: IR 기반 변환 함수들 (Phase 3)

  /**
   * @brief LadderIR을 이용한 고정밀도 .ld 변환
    * L추가erIR을 이용한 고정밀도 .ld 변환
   * 병렬(OR) 회로의 정확한 변환을 보장합니다.
   * @param program 변환할 래더 프로그램
   * @return .ld 형식 문자열
   */
  std::string ConvertToLDStringWithIR(const LadderProgram& program);

  /**
   * @brief IR 기반 스택 명령어 생성
   * MPS, ORB, MPP 등 고급 스택 명령어를 정확히 생성합니다.
   * @param irProgram LadderIR 프로그램
    * L추가erIR 프로그램
   * @return OpenPLC 스택 명령어 리스트
   */
  std::vector<std::string> GenerateStackInstructions(
      const LadderIRProgram& irProgram);

 private:
  std::string last_error_;
  bool debug_mode_ = false;

  struct DeviceSet {
    std::set<int> x_inputs;
    std::set<int> y_outputs;
    std::set<int> m_bits;
    std::set<int> t_timers;
    std::set<int> c_counters;
  };

  /**
   * @brief .ld 파일 헤더 생성 (I/O 변수 선언 등)
   * @return 헤더 문자열
   */
  std::string GenerateLDHeader(const DeviceSet& devices);

  /**
   * @brief 개별 rung을 .ld 형식으로 변환
   * @param rung 변환할 rung
   * @param rungIndex rung 인덱스
   * @param ldContent 출력할 .ld 내용
   */
  void ConvertSingleRung(const Rung& rung, int rungIndex,
                         std::string& ldContent);

  /**
   * @brief 래더 명령어를 OpenPLC .ld 형식으로 변환
   * @param instruction 래더 명령어
   * @return .ld 형식 명령어 문자열
   */
  std::string ConvertInstruction(const LadderInstruction& instruction);

  /**
   * @brief FX3U-32M I/O 주소를 OpenPLC 형식으로 변환
   * @param address FX3U 주소 (예: "X11", "Y5", "M100")
   * @return OpenPLC 주소 (예: "%IX0.11", "%QX0.5", "%MX100")
   */
  std::string ConvertAddress(const std::string& address) const;

  /**
   * @brief 래더 명령어 타입을 OpenPLC .ld 명령어로 변환
   * @param type 래더 명령어 타입
   * @return OpenPLC .ld 명령어 문자열
   */
  std::string GetLDInstructionName(LadderInstructionType type);

  /**
   * @brief 디버그 로그 출력
   * @param message 로그 메시지
   */
  void DebugLog(const std::string& message);
  DeviceSet CollectUsedDevices(const LadderProgram& program) const;
  void AppendOutputInstruction(const LadderInstruction& output,
                               std::string& ldContent) const;
};

}  // namespace plc
#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_LADDER_TO_LD_CONVERTER_H_
