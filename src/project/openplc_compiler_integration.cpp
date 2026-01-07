// openplc_compiler_integration.cpp
//
// Implementation of compiler integration.

#include "plc_emulator/project/openplc_compiler_integration.h"

#include "plc_emulator/programming/programming_mode.h"  // ?뵦 **NEW**: LadderProgram ???
#include "plc_emulator/project/ladder_ir.h"  // ?뵦 **NEW**: LadderIR 吏??
#include "plc_emulator/project/ladder_to_ld_converter.h"  // ?뵦 **NEW**: IR 湲곕컲 蹂?섍린

#include <algorithm>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

namespace plc {

OpenPLCCompilerIntegration::OpenPLCCompilerIntegration() {
  debug_mode_ = false;
  optimization_level_ = 1;
  input_count_ = 16;
  output_count_ = 16;
  memory_count_ = 1000;
}

OpenPLCCompilerIntegration::~OpenPLCCompilerIntegration() {}

OpenPLCCompilerIntegration::CompilationResult
OpenPLCCompilerIntegration::CompileLDFile(const std::string& ldFilePath) {
  CompilationResult result;

  std::ifstream file(ldFilePath);
  if (!file.is_open()) {
    result.success = false;
    result.errorMessage = "Cannot open .ld file: " + ldFilePath;
    return result;
  }

  std::string ldContent((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

  return CompileLDString(ldContent);
}

OpenPLCCompilerIntegration::CompilationResult
OpenPLCCompilerIntegration::CompileLDString(const std::string& ldContent) {
  CompilationResult result;
  instructions_.clear();
  variables_.clear();
  last_error_.clear();

  DebugLog("Starting .ld compilation...");

  // 1. .ld ?뚯씪 ?댁슜 ?뚯떛
  if (!ParseLDContent(ldContent)) {
    result.success = false;
    result.errorMessage = last_error_;
    return result;
  }

  DebugLog("Parsed " + std::to_string(instructions_.size()) + " instructions");
  DebugLog("Found " + std::to_string(variables_.size()) + " variables");

  // 2. C++ 肄붾뱶 ?앹꽦
  result.generatedCode = GenerateCPPCode(instructions_);

  if (result.generatedCode.empty()) {
    result.success = false;
    result.errorMessage = "Failed to generate C++ code";
    return result;
  }

  // 3. 寃곌낵 ?ㅼ젙
  result.success = true;
  result.inputCount = input_count_;
  result.outputCount = output_count_;
  result.memoryCount = memory_count_;

  DebugLog("Compilation successful!");
  DebugLog("Generated " + std::to_string(result.generatedCode.length()) +
           " characters of C++ code");

  return result;
}

void OpenPLCCompilerIntegration::SetIOConfiguration(int inputs, int outputs) {
  input_count_ = inputs;
  output_count_ = outputs;
  DebugLog("I/O Configuration: " + std::to_string(inputs) + " inputs, " +
           std::to_string(outputs) + " outputs");
}

bool OpenPLCCompilerIntegration::SaveGeneratedCode(
    const CompilationResult& result, const std::string& outputPath) {
  if (!result.success) {
    SetError("Cannot save failed compilation result");
    return false;
  }

  std::ofstream file(outputPath);
  if (!file.is_open()) {
    SetError("Cannot open output file: " + outputPath);
    return false;
  }

  file << result.generatedCode;
  file.close();

  DebugLog("Saved generated C++ code to: " + outputPath);
  return true;
}

bool OpenPLCCompilerIntegration::ParseLDContent(const std::string& ldContent) {
  // .ld ?뚯씪 援ъ“:
  // 1. 二쇱꽍 (* ... *)
  // 2. PROGRAM PLC_PRG
  // 3. VAR ?뱀뀡
  // 4. END_VAR
  // 5. ?꾨줈洹몃옩 ?뱀뀡 (LD, AND, OR, ST 紐낅졊?대뱾)
  // 6. END_PROGRAM

  std::string content = ldContent;

  // 二쇱꽍 ?쒓굅
  std::regex commentRegex(R"(\(\*.*?\*\))");
  content = std::regex_replace(content, commentRegex, "");

  // VAR ?뱀뀡 異붿텧
  std::regex varRegex(R"(VAR\s+(.*?)\s+END_VAR)", std::regex_constants::icase);
  std::smatch varMatch;

  if (std::regex_search(content, varMatch, varRegex)) {
    if (!ParseVariableDeclarations(varMatch[1].str())) {
      return false;
    }
  }

  // ?꾨줈洹몃옩 ?뱀뀡 異붿텧 (VAR ?댄썑遺??END_PROGRAM源뚯?)
  size_t programStart = content.find("END_VAR");
  if (programStart != std::string::npos) {
    programStart += 7;  // "END_VAR" 湲몄씠

    size_t programEnd = content.find("END_PROGRAM");
    if (programEnd == std::string::npos) {
      programEnd = content.length();
    }

    std::string programSection =
        content.substr(programStart, programEnd - programStart);
    if (!ParseInstructions(programSection)) {
      return false;
    }
  } else {
    // VAR ?뱀뀡???녿뒗 寃쎌슦 ?꾩껜瑜??꾨줈洹몃옩 ?뱀뀡?쇰줈 泥섎━
    if (!ParseInstructions(content)) {
      return false;
    }
  }

  return true;
}

bool OpenPLCCompilerIntegration::ParseVariableDeclarations(
    const std::string& content) {
  std::istringstream stream(content);
  std::string line;

  while (std::getline(stream, line)) {
    line = Trim(line);
    if (line.empty() || line[0] == '(' ||
        line.find("(*") != std::string::npos) {
      continue;  // 二쇱꽍?대굹 鍮?以?嫄대꼫?곌린
    }

    // 蹂???좎뼵 ?뚯떛: X0 AT %IX0.0 : BOOL;
    std::regex varRegex(
        R"((\w+)\s+AT\s+%([IQMX])([XW]?)(\d+)\.?(\d*)\s*:\s*(\w+);?)");
    std::smatch match;

    if (std::regex_search(line, match, varRegex)) {
      Variable var;
      var.name = match[1].str();
      std::string location = match[2].str();
      std::string dataType = match[6].str();

      if (location == "I" || location == "IX") {
        var.type = Variable::BOOL_INPUT;
      } else if (location == "Q" || location == "QX") {
        var.type = Variable::BOOL_OUTPUT;
      } else if (location == "M" || location == "MX") {
        var.type = Variable::BOOL_MEMORY;
      }

      var.address = line;
      variables_.push_back(var);

      DebugLog("Found variable: " + var.name + " (" + location + ")");
    }
  }

  return true;
}

bool OpenPLCCompilerIntegration::ParseInstructions(const std::string& content) {
  std::istringstream stream(content);
  std::string line;
  int lineNumber = 0;

  while (std::getline(stream, line)) {
    lineNumber++;
    line = Trim(line);

    if (line.empty() || line[0] == '(' ||
        line.find("PROGRAM") != std::string::npos ||
        line.find("END_PROGRAM") != std::string::npos) {
      continue;
    }

    // ?쇱씤 ?좏겙??(?곗냽 怨듬갚 ?덉슜)
    std::istringstream lineStream(line);
    std::vector<std::string> tokens;
    std::string tok;
    while (lineStream >> tok)
      tokens.push_back(tok);
    if (tokens.empty())
      continue;

    auto upper = [](std::string s) {
      std::transform(s.begin(), s.end(), s.begin(), ::toupper);
      return s;
    };
    std::string instr = upper(tokens[0]);

    LDInstruction ldInst;
    ldInst.lineNumber = lineNumber;

    // LD/AND/OR + optional NOT 泥섎━
    if (instr == "LDN" || instr == "ANDN" || instr == "ORN") {
      if (tokens.size() < 2) {
        continue;
      }
      if (instr == "LDN") {
        ldInst.type = LDInstruction::LDN;
      } else if (instr == "ANDN") {
        ldInst.type = LDInstruction::ANDN;
      } else {
        ldInst.type = LDInstruction::ORN;
      }
      ldInst.operand = tokens[1];
    } else if (instr == "LD" || instr == "AND" || instr == "OR") {
      if (tokens.size() < 2) {
        // ?쇱뿰?곗옄 ?놁쓬: 臾댁떆
        continue;
      }
      bool hasNot = (upper(tokens[1]) == "NOT");
      if (instr == "LD") {
        ldInst.type = hasNot ? LDInstruction::LDN : LDInstruction::LD;
        if (hasNot) {
          if (tokens.size() < 3)
            continue;  // NOT ???쇱뿰?곗옄 ?놁쓬
          ldInst.operand = tokens[2];
        } else {
          ldInst.operand = tokens[1];
        }
      } else if (instr == "AND") {
        ldInst.type = hasNot ? LDInstruction::ANDN : LDInstruction::AND;
        if (hasNot) {
          if (tokens.size() < 3)
            continue;
          ldInst.operand = tokens[2];
        } else {
          ldInst.operand = tokens[1];
        }
      } else {  // OR
        ldInst.type = hasNot ? LDInstruction::ORN : LDInstruction::OR;
        if (hasNot) {
          if (tokens.size() < 3)
            continue;
          ldInst.operand = tokens[2];
        } else {
          ldInst.operand = tokens[1];
        }
      }
    } else if (instr == "ST" || instr == "S" || instr == "R") {
      if (tokens.size() < 2)
        continue;
      if (instr == "ST")
        ldInst.type = LDInstruction::ST;
      else if (instr == "S")
        ldInst.type = LDInstruction::S;
      else
        ldInst.type = LDInstruction::R;
      ldInst.operand = tokens[1];
    } else if (instr == "TON") {
      // TON <Tn> <Kxxx>
      if (tokens.size() < 2)
        continue;
      ldInst.type = LDInstruction::TON;
      ldInst.operand = tokens[1];
      if (tokens.size() >= 3)
        ldInst.preset = tokens[2];
    } else if (instr == "TOF" || instr == "CTU" || instr == "CTD") {
      // ?ν썑 ?뺤옣?? 湲곕낯 ?쇱뿰?곗옄留??섏슜
      ldInst.type = StringToInstructionType(instr);
      if (tokens.size() >= 2)
        ldInst.operand = tokens[1];
      if (instr == "CTU" && tokens.size() >= 3)
        ldInst.preset = tokens[2];
    } else {
      // ?????녿뒗 紐낅졊: 嫄대꼫?
      continue;
    }

    instructions_.push_back(ldInst);
    DebugLog("Line " + std::to_string(lineNumber) + ": " + instr +
             (ldInst.operand.empty() ? "" : " ") + ldInst.operand);
  }

  return true;
}

std::string OpenPLCCompilerIntegration::GenerateCPPCode(
    const std::vector<LDInstruction>& instructions) {
  std::stringstream code;

  // 1. ?ㅻ뜑 ?앹꽦
  code << GenerateFunctionHeader();
  code << "\n";

  // 2. 蹂???좎뼵
  code << GenerateVariableDeclarations();
  code << "\n";

  // 3. I/O 留ㅽ븨
  code << GenerateIOMapping();
  code << "\n";

  // 4. ?섎뜑 濡쒖쭅 ?ㅽ뻾 肄붾뱶
  code << GenerateExecutionCode(instructions);
  code << "\n";

  // 5. ?⑥닔 醫낅즺
  code << "}\n";

  return code.str();
}

std::string OpenPLCCompilerIntegration::GenerateFunctionHeader() {
  std::stringstream header;

  header
      << "// Generated by FX3U PLC Simulator - OpenPLC Compiler Integration\n";
  header << "// Target: FX3U-32M (16 inputs, 16 outputs)\n";
  header << "#include <iostream>\n";
  header << "#include <chrono>\n\n";

  header << "// PLC Logic Execution Function\n";
  header << "extern \"C\" void ExecutePLCLogic(bool* X, bool* Y, bool* M, int* "
            "T, int* C) {\n";

  return header.str();
}

std::string OpenPLCCompilerIntegration::GenerateVariableDeclarations() {
  std::stringstream vars;

  vars << "    // PLC Scan Cycle Variables\n";
  vars << "    static bool accumulator = false;\n";
  vars << "    static bool accumulator_stack[16]; // OR/AND 釉붾줉 ?ㅽ깮\n";
  vars << "    static int stack_pointer = 0;\n";
  vars << "    \n";
  vars << "    // Timer variables\n";
  vars
      << "    static auto last_scan_time = std::chrono::steady_clock::now();\n";
  vars << "    auto current_time = std::chrono::steady_clock::now();\n";
  vars << "    auto elapsed_ms = "
          "std::chrono::duration_cast<std::chrono::milliseconds>(current_time "
          "- last_scan_time).count();\n";
  vars << "    \n";

  return vars.str();
}

std::string OpenPLCCompilerIntegration::GenerateIOMapping() {
  std::stringstream mapping;

  mapping << "    // I/O Safety checks\n";
  mapping << "    if (!X || !Y || !M || !T || !C) return;\n";
  mapping << "    \n";
  mapping << "    // FX3U-32M I/O Configuration\n";
  mapping << "    // Inputs: X0-X" << (input_count_ - 1) << "\n";
  mapping << "    // Outputs: Y0-Y" << (output_count_ - 1) << "\n";
  mapping << "    // Memory: M0-M" << (memory_count_ - 1) << "\n";
  mapping << "    \n";

  return mapping.str();
}

std::string OpenPLCCompilerIntegration::GenerateExecutionCode(
    const std::vector<LDInstruction>& instructions) {
  std::stringstream code;

  code << "    // === PLC Ladder Logic Execution ===\n";
  code << "    \n";

  for (const auto& instruction : instructions) {
    std::string line = TranslateInstruction(instruction);
    if (!line.empty()) {
      code << "    " << line << " // Line " << instruction.lineNumber << "\n";
    }
  }

  code << "    \n";
  code << "    // Update scan time\n";
  code << "    last_scan_time = current_time;\n";

  return code.str();
}

std::string OpenPLCCompilerIntegration::TranslateInstruction(
    const LDInstruction& instruction) {
  auto preset_to_int = [](std::string preset, int scale) -> int {
    if (preset.empty()) {
      return 0;
    }
    if (preset[0] == 'K' || preset[0] == 'k') {
      preset = preset.substr(1);
    }
    int value = 0;
    try {
      value = std::stoi(preset);
    } catch (...) {
      return 0;
    }
    if (value < 0) {
      value = 0;
    }
    if (scale > 1) {
      value *= scale;
    }
    return value;
  };

  switch (instruction.type) {
    case LDInstruction::LD:
      return "accumulator = " + instruction.operand + ";";

    case LDInstruction::LDN:
      return "accumulator = !" + instruction.operand + ";";

    case LDInstruction::AND:
      return "accumulator = accumulator && " + instruction.operand + ";";

    case LDInstruction::ANDN:
      return "accumulator = accumulator && !" + instruction.operand + ";";

    case LDInstruction::OR:
      return "accumulator = accumulator || " + instruction.operand + ";";

    case LDInstruction::ORN:
      return "accumulator = accumulator || !" + instruction.operand + ";";

    case LDInstruction::ST:
      return instruction.operand + " = accumulator;";

    case LDInstruction::S:
      return "if (accumulator) " + instruction.operand + " = true;";

    case LDInstruction::R:
      if (!instruction.operand.empty() &&
          (instruction.operand[0] == 'T' || instruction.operand[0] == 'C')) {
        return "PLC_RST " + instruction.operand + ";";
      }
      return "if (accumulator) " + instruction.operand + " = false;";

    case LDInstruction::TON: {
      int preset_ms = preset_to_int(instruction.preset, 100);
      return "PLC_TON " + instruction.operand + " " +
             std::to_string(preset_ms) + ";";
    }

    case LDInstruction::CTU: {
      int preset = preset_to_int(instruction.preset, 1);
      return "PLC_CTU " + instruction.operand + " " +
             std::to_string(preset) + ";";
    }

    default:
      return "// Unknown instruction: " +
             std::to_string(static_cast<int>(instruction.type));
  }
}

OpenPLCCompilerIntegration::LDInstruction::Type
OpenPLCCompilerIntegration::StringToInstructionType(
    const std::string& typeStr) {
  std::string upper = typeStr;
  std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

  if (upper == "LD")
    return LDInstruction::LD;
  if (upper == "LDN" || upper == "LD NOT")
    return LDInstruction::LDN;
  if (upper == "AND")
    return LDInstruction::AND;
  if (upper == "ANDN" || upper == "AND NOT")
    return LDInstruction::ANDN;
  if (upper == "OR")
    return LDInstruction::OR;
  if (upper == "ORN" || upper == "OR NOT")
    return LDInstruction::ORN;
  if (upper == "ST")
    return LDInstruction::ST;
  if (upper == "S")
    return LDInstruction::S;
  if (upper == "R")
    return LDInstruction::R;
  if (upper == "TON")
    return LDInstruction::TON;
  if (upper == "TOF")
    return LDInstruction::TOF;
  if (upper == "CTU")
    return LDInstruction::CTU;
  if (upper == "CTD")
    return LDInstruction::CTD;

  return LDInstruction::LD;  // 湲곕낯媛?
}

void OpenPLCCompilerIntegration::SetError(const std::string& error) {
  last_error_ = error;
  if (debug_mode_) {
    std::cerr << "[OpenPLCCompilerIntegration] ERROR: " << error << std::endl;
  }
}

void OpenPLCCompilerIntegration::DebugLog(const std::string& message) {
  if (debug_mode_) {
    std::cout << "[OpenPLCCompilerIntegration] " << message << std::endl;
  }
}

std::string OpenPLCCompilerIntegration::Trim(const std::string& str) {
  size_t start = str.find_first_not_of(" \t\r\n");
  if (start == std::string::npos)
    return "";

  size_t end = str.find_last_not_of(" \t\r\n");
  return str.substr(start, end - start + 1);
}

// ============================================================================
// ?뵦 **NEW**: IR 湲곕컲 怨좎젙諛??而댄뙆???⑥닔??(Phase 4)
// ============================================================================

OpenPLCCompilerIntegration::CompilationResult
OpenPLCCompilerIntegration::CompileLadderProgramWithIR(
    const LadderProgram& ladderProgram) {
  DebugLog("?뵦 Starting IR-based compilation...");

  CompilationResult result;

  try {
    // 1. LadderProgram ??LadderIR 蹂??
    DebugLog("Step 1: Converting to IR...");
    LadderIRProgram irProgram = LadderToIRConverter::ConvertToIR(ladderProgram);

    // 2. LadderIR ??而댄뙆??
    DebugLog("Step 2: Compiling IR program...");
    result = CompileIRProgram(irProgram);

    DebugLog("??IR-based compilation completed!");

  } catch (const std::exception& e) {
    result.success = false;
    result.errorMessage = "IR compilation failed: " + std::string(e.what());
    SetError(result.errorMessage);
  }

  return result;
}

OpenPLCCompilerIntegration::CompilationResult
OpenPLCCompilerIntegration::CompileIRProgram(const LadderIRProgram& irProgram) {
  DebugLog("Compiling LadderIR program with advanced optimization...");

  CompilationResult result;

  try {
    // 1. LadderIR ???ㅽ깮 紐낅졊??蹂??
    std::vector<std::string> stackInstructions =
        IRToStackConverter::ConvertToStackInstructions(irProgram);

    DebugLog("Generated " + std::to_string(stackInstructions.size()) +
             " stack instructions");

    // 2. ?ㅽ깮 紐낅졊????.ld 臾몄옄??蹂??
    std::stringstream ldContent;

    // .ld ?ㅻ뜑 ?앹꽦
    ldContent << GenerateOpenPLCHeader();
    ldContent << "\n\nPROGRAM main\nVAR\nEND_VAR\n\n";

    // ?ㅽ깮 紐낅졊?대뱾??.ld ?뺤떇?쇰줈 蹂??
    for (const auto& instruction : stackInstructions) {
      if (!instruction.empty()) {
        ldContent << "    " << instruction << ";\n";
      }
    }

    ldContent << "\nEND_PROGRAM\n";

    // 3. .ld 臾몄옄??而댄뙆??
    std::string ldString = ldContent.str();
    result = CompileLDString(ldString);

    // 4. IR 理쒖쟻???뺣낫 異붽?
    if (result.success) {
      result.intermediateCode = "// IR-based compilation\n";
      result.intermediateCode +=
          "// Total IR rungs: " + std::to_string(irProgram.GetRungCount()) +
          "\n";
      result.intermediateCode +=
          "// Stack instructions: " + std::to_string(stackInstructions.size()) +
          "\n";
      result.intermediateCode += ldString;
    }

  } catch (const std::exception& e) {
    result.success = false;
    result.errorMessage = "IR compilation failed: " + std::string(e.what());
    SetError(result.errorMessage);
  }

  return result;
}

std::string OpenPLCCompilerIntegration::GenerateOpenPLCHeader() {
  std::stringstream header;

  header << "// Auto-generated by PLC Simulator IR Engine\n";
  header << "// Optimization Level: " << optimization_level_ << "\n";
  header << "// I/O Configuration: " << input_count_ << " inputs, "
         << output_count_ << " outputs\n";
  header << "\n";

  // I/O 蹂???좎뼵
  header << "VAR_GLOBAL\n";

  // ?낅젰 蹂?섎뱾 (X0~X15)
  for (int i = 0; i < input_count_; ++i) {
    header << "  X" << i << " : BOOL;\n";
  }

  // 異쒕젰 蹂?섎뱾 (Y0~Y15)
  for (int i = 0; i < output_count_; ++i) {
    header << "  Y" << i << " : BOOL;\n";
  }

  // ??대㉧/移댁슫??蹂?섎뱾
  header << "  // Timer variables\n";
  for (int i = 0; i < 100; ++i) {
    header << "  T" << i << " : TON;\n";
  }

  header << "  // Counter variables\n";
  for (int i = 0; i < 100; ++i) {
    header << "  C" << i << " : CTU;\n";
  }

  header << "END_VAR\n";

  return header.str();
}

}  // namespace plc
