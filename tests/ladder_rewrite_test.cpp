#include "plc_emulator/programming/compiled_plc_executor.h"
#include "plc_emulator/programming/ladder_program.h"
#include "plc_emulator/project/gx_csv_codec.h"
#include "plc_emulator/project/instruction_codec.h"
#include "plc_emulator/project/ladder_to_ld_converter.h"
#include "plc_emulator/project/ld_to_ladder_converter.h"
#include "plc_emulator/project/structured_ladder_compiler.h"

#include <iostream>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
#include <stdexcept>

namespace {
namespace model = plc_emulator::programming;
using Type = plc::LadderInstructionType;

void Require(bool condition, const std::string& message) {
  if (!condition)
    throw std::runtime_error(message);
}

bool ExportTextGXCSV(const plc::LadderProgram& ladder, const std::string& name,
                     std::string* content, std::string* error) {
  return model::ExportGXCSV(ladder, name, content, error,
                            model::GXCSVEncoding::kUtf8);
}

plc::LadderProgram Program(Type output = Type::OTE,
                           const std::string& address = "Y0") {
  plc::LadderProgram ladder;
  ladder.rungs[0].cells = {{Type::XIC, "X0"}, {output, address}};
  return ladder;
}

model::ExecutionProgram Compile(const plc::LadderProgram& ladder) {
  model::ExecutionProgram program;
  std::string error;
  if (!model::CompileLadder(ladder, &program, &error)) {
    throw std::runtime_error(error);
  }
  return program;
}

void Devices() {
  Require(plc_emulator::programming::ParseDeviceAddress("X10")->index == 8,
          "Octal X10");
  Require(plc_emulator::programming::ParseDeviceAddress("Y17")->index == 15,
          "Octal Y17");
  Require(!plc_emulator::programming::ParseDeviceAddress("X8"),
          "Invalid octal");
  Require(!plc_emulator::programming::ParseDeviceAddress("X400"), "I/O range");
  Require(!plc_emulator::programming::ParseDeviceAddress("M-1"),
          "Negative device");
  Require(!plc_emulator::programming::ParseDeviceAddress("D0junk"),
          "Trailing characters");
  Require(plc_emulator::programming::ParseDeviceAddress("M8000")->kind ==
              model::DeviceKind::kSpecialM,
          "Special M separation");
  Require(plc_emulator::programming::ParseDeviceAddress("S100").has_value(),
          "S device");
  Require(model::ParseOperand("K-10")->immediate == -10, "Signed constant");
  Require(model::ParseOperand("HFFFF")->immediate == 65535, "Hex constant");
  Require(!model::ParseOperand("K2147483648"), "Overflow");
}

void SeriesAndTransactionalLoad() {
  plc::CompiledPLCExecutor executor;
  const auto program = Compile(Program());
  Require(executor.LoadProgram(program), "Load program");
  executor.SetInput(0, true);
  Require(executor.ExecuteScanCycle().success && executor.GetOutput(0),
          "NO on");
  model::ExecutionProgram invalid = program;
  invalid.instructions[0].condition.back().left = 999;
  Require(!executor.LoadProgram(invalid), "Reject malformed graph");
  executor.SetInput(0, false);
  Require(executor.ExecuteScanCycle().success && !executor.GetOutput(0),
          "Failed load preserves previous program");
  auto ladder = Program();
  ladder.rungs[0].cells[0].type = Type::XIO;
  Require(executor.LoadProgram(Compile(ladder)), "Load NC");
  Require(executor.ExecuteScanCycle().success && executor.GetOutput(0),
          "NC on");
}

void Parallel() {
  plc::LadderProgram ladder = Program();
  plc::Rung branch;
  branch.cells = {{Type::XIC, "X1"}};
  ladder.rungs.insert(ladder.rungs.begin() + 1, branch);
  ladder.verticalConnections.emplace_back(1, 0, 1);
  plc::CompiledPLCExecutor executor;
  Require(executor.LoadProgram(Compile(ladder)), "Load parallel");
  for (int bits = 0; bits < 4; ++bits) {
    executor.SetInput(0, bits & 1);
    executor.SetInput(1, bits & 2);
    Require(executor.ExecuteScanCycle().success &&
                executor.GetOutput(0) == (bits != 0),
            "Parallel truth table");
  }
}

void TimerCounterAndReset() {
  auto ladder = Program(Type::TON, "T0");
  ladder.rungs[0].cells[1].preset = "K2";
  plc::CompiledPLCExecutor executor;
  Require(executor.LoadProgram(Compile(ladder)), "Load timer");
  executor.SetContinuousExecution(true, 100);
  executor.SetInput(0, true);
  Require(executor.ExecuteScanCycle().success, "Timer starts");
  Require(executor.ExecuteScanCycle().success && !executor.GetDeviceState("T0"),
          "Timer counts elapsed time after coil activation");
  Require(executor.ExecuteScanCycle().success && executor.GetDeviceState("T0"),
          "Timer elapsed");
  executor.SetInput(0, false);
  Require(executor.ExecuteScanCycle().success && executor.GetTimerValue(0) == 0,
          "Timer resets when unpowered");
  ladder = Program(Type::CTU, "C0");
  ladder.rungs[0].cells[1].preset = "K2";
  Require(executor.LoadProgram(Compile(ladder)), "Load counter");
  executor.ResetMemory();
  executor.SetInput(0, true);
  Require(executor.ExecuteScanCycle().success, "Counter first edge");
  Require(
      executor.ExecuteScanCycle().success && executor.GetCounterValue(0) == 1,
      "Counter does not count sustained power");
  executor.SetInput(0, false);
  Require(executor.ExecuteScanCycle().success, "Counter off");
  executor.SetInput(0, true);
  Require(executor.ExecuteScanCycle().success && executor.GetDeviceState("C0"),
          "Counter second edge");
}

void InvalidTopology() {
  auto ladder = Program();
  ladder.rungs[0].cells.insert(ladder.rungs[0].cells.begin() + 1,
                               plc::LadderInstruction{});
  model::ExecutionProgram program;
  std::string error;
  Require(!model::CompileLadder(ladder, &program, &error),
          "Disconnected output");
  ladder = Program();
  ladder.verticalConnections.emplace_back(1, 0, 10);
  Require(!model::CompileLadder(ladder, &program, &error),
          "Invalid connection");
}

model::ExecutionProgram Statements(
    std::vector<model::InstructionStatement> statements) {
  model::ExecutionProgram program;
  std::string error;
  if (!model::CompileStatements(statements, &program, &error))
    throw std::runtime_error(error);
  return program;
}

void CSVAndNestedBranches() {
  const auto program = Statements({{"LD", {"X0"}},
                                   {"MPS", {}},
                                   {"AND", {"X1"}},
                                   {"LD", {"X2"}},
                                   {"AND", {"X3"}},
                                   {"ORB", {}},
                                   {"OUT", {"Y0"}},
                                   {"MRD", {}},
                                   {"AND", {"X4"}},
                                   {"OUT", {"Y1"}},
                                   {"MPP", {}},
                                   {"MOV", {"K10", "D0"}},
                                   {"END", {}}});
  plc::LadderProgram ladder;
  std::string error;
  Require(model::MaterializeLadder(program, &ladder, &error), error);
  std::string csv;
  const bool exported = ExportTextGXCSV(ladder, "round trip", &csv, &error);
  Require(exported, "Export failed");
  Require(csv.find("VCX=") == std::string::npos,
          "No private CSV topology markers");
  plc::LadderProgram imported;
  Require(model::ImportGXCSV(csv, &imported, &error), error);
  plc::CompiledPLCExecutor before;
  plc::CompiledPLCExecutor after;
  Require(before.LoadProgram(program), "Load original branch program");
  Require(after.LoadProgram(Compile(imported)), "Load CSV branch program");
  for (int bits = 0; bits < 32; ++bits) {
    for (int i = 0; i < 5; ++i) {
      before.SetInput(i, bits & (1 << i));
      after.SetInput(i, bits & (1 << i));
    }
    Require(
        before.ExecuteScanCycle().success && after.ExecuteScanCycle().success,
        "Branch execution");
    Require(before.GetOutput(0) ==
                (((bits & 1) && (bits & 2)) || ((bits & 4) && (bits & 8))),
            "Nested branch truth table");
    Require(before.GetOutput(0) == after.GetOutput(0) &&
                before.GetOutput(1) == after.GetOutput(1),
            "CSV semantic round trip");
    Require(after.GetWordValue(*plc_emulator::programming::ParseDeviceAddress(
                "D0")) == (bits == 0 ? 0 : 10),
            "MOV continuation operands retained");
  }
}

void ArithmeticAndPulse() {
  plc::CompiledPLCExecutor executor;
  const auto program = Statements({{"LD", {"X0"}},
                                   {"MOVP", {"K10", "D0"}},
                                   {"LD", {"X0"}},
                                   {"ADDP", {"D0", "K2", "D0"}},
                                   {"END", {}}});
  Require(executor.LoadProgram(program), "Load pulse arithmetic");
  executor.SetInput(0, true);
  Require(executor.ExecuteScanCycle().success, "Pulse first scan");
  Require(executor.GetWordValue(
              *plc_emulator::programming::ParseDeviceAddress("D0")) == 12,
          "MOV plus ADD");
  Require(executor.ExecuteScanCycle().success &&
              executor.GetWordValue(
                  *plc_emulator::programming::ParseDeviceAddress("D0")) == 12,
          "Pulse runs once");
  const auto comparison =
      Statements({{"LD=", {"D0", "K12"}}, {"OUT", {"Y0"}}, {"END", {}}});
  Require(executor.LoadProgram(comparison), "Load comparison");
  Require(executor.ExecuteScanCycle().success && executor.GetOutput(0),
          "Comparison contact");
  model::ExecutionProgram invalid;
  std::string error;
  Require(!model::CompileStatements(
              {{"LD", {"X0"}}, {"DMOV", {"K1", "D7999"}}, {"END", {}}},
              &invalid, &error),
          "Wide destination bound");
  Require(!model::CompileStatements(
              {{"LD", {"X0"}}, {"MPP", {}}, {"OUT", {"Y0"}}}, &invalid, &error),
          "Branch stack underflow");
}

void SavedBranchAndCacheInvalidation() {
  plc::LadderProgram shared_grid;
  shared_grid.rungs.clear();
  shared_grid.rungs.emplace_back();
  shared_grid.rungs.emplace_back();
  shared_grid.rungs[0].cells = {{Type::XIC, "M0"}, {Type::RST, "M0"}};
  shared_grid.rungs[1].cells = {{Type::EMPTY, ""}, {Type::OTE, "Y0"}};
  shared_grid.verticalConnections.emplace_back(1, 0, 1);
  std::string grid_csv;
  std::string grid_error;
  Require(ExportTextGXCSV(shared_grid, "grid branch", &grid_csv, &grid_error),
          grid_error);
  plc::LadderProgram shared_import;
  Require(model::ImportGXCSV(grid_csv, &shared_import, &grid_error),
          grid_error);
  plc::CompiledPLCExecutor grid_executor;
  Require(grid_executor.LoadProgram(Compile(shared_import)),
          "Grid branch load");
  grid_executor.SetMemory(0, true);
  Require(
      grid_executor.ExecuteScanCycle().success && grid_executor.GetOutput(0),
      "CSV preserves a shared grid condition after reset");
  shared_grid.rungs[0].cells = {
      {Type::XIC, "M0"}, {Type::XIC, "X0"}, {Type::RST, "M0"}};
  shared_grid.rungs[1].cells = {
      {Type::EMPTY, ""}, {Type::XIC, "X1"}, {Type::OTE, "Y0"}};
  Require(ExportTextGXCSV(shared_grid, "split branch", &grid_csv, &grid_error),
          grid_error);
  Require(model::ImportGXCSV(grid_csv, &shared_import, &grid_error),
          grid_error);
  Require(grid_executor.LoadProgram(Compile(shared_import)),
          "Split branch load");
  grid_executor.SetInput(0, true);
  grid_executor.SetInput(1, true);
  grid_executor.SetMemory(0, true);
  Require(grid_executor.ExecuteScanCycle().success &&
              grid_executor.GetOutput(0) && !grid_executor.GetMemory(0),
          "CSV saves a shared prefix across different output conditions");
  const auto program = Statements({{"LD", {"M0"}},
                                   {"MPS", {}},
                                   {"RST", {"M0"}},
                                   {"MPP", {}},
                                   {"OUT", {"Y0"}},
                                   {"END", {}}});
  plc::LadderProgram ladder;
  std::string error;
  Require(model::MaterializeLadder(program, &ladder, &error), error);
  std::string csv;
  Require(ExportTextGXCSV(ladder, "saved branch", &csv, &error), error);
  plc::LadderProgram imported;
  Require(model::ImportGXCSV(csv, &imported, &error), error);
  plc::CompiledPLCExecutor executor;
  Require(executor.LoadProgram(Compile(imported)), "Saved branch load");
  executor.SetMemory(0, true);
  Require(executor.ExecuteScanCycle().success && executor.GetOutput(0) &&
              !executor.GetMemory(0),
          "MPP preserves the condition before RST writes M0");
  imported.rungs[0].cells[0].address = "X8";
  model::ExecutionProgram rejected;
  Require(!model::CompileLadder(imported, &rejected, &error),
          "Editing imported grid invalidates its canonical snapshot");
}

void WordOperationsAndLD() {
  const auto program = Statements({{"LD", {"M8000"}},
                                   {"MOV", {"K7", "D0"}},
                                   {"DIV", {"D0", "K3", "D2"}},
                                   {"DMUL", {"K-70000", "K70000", "D4"}},
                                   {"FMOV", {"K9", "D10", "K3"}},
                                   {"BMOV", {"D10", "D11", "K3"}},
                                   {"CMP", {"D0", "K7", "M10"}},
                                   {"ZCP", {"K1", "K9", "D0", "M20"}},
                                   {"RST", {"D0"}},
                                   {"ZRST", {"D10", "D11"}},
                                   {"END", {}}});
  plc::LadderProgram ladder;
  std::string error;
  Require(model::MaterializeLadder(program, &ladder, &error), error);
  plc::LadderToLDConverter writer;
  const std::string ld = writer.ConvertToLDString(ladder);
  Require(!ld.empty(), writer.GetLastError());
  plc::LDToLadderConverter reader;
  plc::LadderProgram imported;
  Require(reader.ConvertFromLDString(ld, imported), reader.GetLastError());
  plc::CompiledPLCExecutor executor;
  Require(executor.LoadProgram(Compile(imported)), "LD word operations load");
  Require(executor.ExecuteScanCycle().success, "Word operations scan");
  const auto word = [&executor](const std::string& address) {
    return executor.GetWordValue(
        *plc_emulator::programming::ParseDeviceAddress(address));
  };
  Require(word("D2") == 2 && word("D3") == 1, "DIV quotient/remainder");
  Require(word("D4") == -4352 && word("D5") == -9233 && word("D6") == -2 &&
              word("D7") == -1,
          "DMUL writes four registers");
  Require(executor.GetMemory(11) && executor.GetMemory(21),
          "CMP/ZCP middle result bits");
  Require(word("D0") == 0 && word("D10") == 0 && word("D11") == 0 &&
              word("D12") == 9 && word("D13") == 9,
          "Reset and overlapping block copy");
}

void TierThreeDataInstructions() {
  const std::vector<model::InstructionStatement> statements{
      {"LD", {"X0"}},
      {"MOV", {"H8001", "D0"}},
      {"ROR", {"D0", "K1"}},
      {"BON", {"D0", "M0", "K14"}},
      {"ROL", {"D0", "K1"}},
      {"RCR", {"D0", "K1"}},
      {"RCL", {"D0", "K1"}},
      {"SUM", {"D0", "D1"}},
      {"DECO", {"D1", "M10", "K3"}},
      {"ENCO", {"M10", "D2", "K3"}},
      {"DECO", {"D2", "D3", "K4"}},
      {"ENCO", {"D3", "D4", "K4"}},
      {"DMOV", {"H80000001", "D20"}},
      {"DROR", {"D20", "K1"}},
      {"DROL", {"D20", "K1"}},
      {"DRCR", {"D20", "K1"}},
      {"DRCL", {"D20", "K1"}},
      {"DSUM", {"D20", "D22"}},
      {"DBON", {"D20", "M1", "K31"}},
      {"SFTR", {"M10", "M30", "K4", "K1"}},
      {"SFTL", {"M12", "M30", "K4", "K1"}},
      {"MOV", {"K7", "D30"}},
      {"MOV", {"K9", "D40"}},
      {"WSFR", {"D30", "D40", "K3", "K1"}},
      {"WSFL", {"D30", "D40", "K3", "K1"}},
      {"END", {}}};
  model::ExecutionProgram program;
  std::string error;
  Require(model::CompileStatements(statements, &program, &error), error);
  const auto verify = [](const model::ExecutionProgram& candidate) {
    plc::CompiledPLCExecutor executor;
    Require(executor.LoadProgram(candidate), "Load Tier 3 data operations");
    executor.SetInput(0, true);
    Require(executor.ExecuteScanCycle().success,
            "Execute Tier 3 data operations");
    const auto word = [&executor](uint32_t index, bool wide = false) {
      return executor.GetWordValue({model::DeviceKind::kD, index}, wide);
    };
    Require(word(0) == -32767 && word(1) == 2 && executor.GetMemory(0),
            "16-bit rotations, carry and population count");
    Require(word(2) == 2 && word(3) == 4 && word(4) == 2 &&
                executor.GetMemory(12) && !executor.GetMemory(11),
            "DECO one-hot and ENCO index for bit and word devices");
    Require(word(20, true) == INT32_MIN + 1 && word(22, true) == 2 &&
                executor.GetMemory(1),
            "32-bit rotations, DSUM and DBON high bit");
    Require(executor.GetMemory(30) && !executor.GetMemory(31) &&
                word(40) == 7 && word(41) == 0 && word(42) == 0,
            "Bit and word shift directions and inserted source values");
    Require(executor.GetDeviceState("M8022"), "Rotation carry result");
  };
  verify(program);
  plc::LadderProgram ladder;
  Require(model::MaterializeLadder(program, &ladder, &error), error);
  std::string csv;
  Require(ExportTextGXCSV(ladder, "Tier3", &csv, &error), error);
  plc::LadderProgram imported;
  Require(model::ImportGXCSV(csv, &imported, &error), error);
  verify(Compile(imported));

  Require(model::CompileStatements({{"LD", {"X1"}},
                                    {"MOV", {"K1", "D0"}},
                                    {"LD", {"X0"}},
                                    {"ROLP", {"D0", "K1"}},
                                    {"SUM", {"D0", "D2"}},
                                    {"END", {}}},
                                   &program, &error),
          error);
  plc::CompiledPLCExecutor executor;
  Require(executor.LoadProgram(program), "Load pulse rotate");
  executor.SetInput(0, true);
  Require(
      executor.ExecuteScanCycle().success && executor.GetDeviceState("M8020"),
      "SUM sets zero flag");
  executor.SetInput(0, false);
  executor.SetInput(1, true);
  Require(executor.ExecuteScanCycle().success, "Initialize rotation value");
  executor.SetInput(1, false);
  executor.SetInput(0, true);
  Require(executor.ExecuteScanCycle().success &&
              executor.GetWordValue({model::DeviceKind::kD, 0}) == 2 &&
              !executor.GetDeviceState("M8020"),
          "ROLP rising edge and zero flag");
  Require(executor.ExecuteScanCycle().success &&
              executor.GetWordValue({model::DeviceKind::kD, 0}) == 2,
          "ROLP does not rotate again while input remains ON");
  executor.SetInput(0, false);
  Require(executor.ExecuteScanCycle().success, "Release rotation pulse");
  executor.SetInput(0, true);
  Require(executor.ExecuteScanCycle().success &&
              executor.GetWordValue({model::DeviceKind::kD, 0}) == 4,
          "ROLP rotates again on the next rising edge");
  // DECO reads a bit field in octal X address order, not an individual bit.
  Require(model::CompileStatements({{"LD", {"X0"}},
                                    {"DECO", {"X1", "D0", "K3"}},
                                    {"ENCO", {"D0", "D1", "K4"}},
                                    {"END", {}}},
                                   &program, &error),
          error);
  Require(executor.LoadProgram(program), "Load input-bit decode");
  executor.SetInput(0, true);
  executor.SetInput(1, true);
  executor.SetInput(3, true);
  Require(executor.ExecuteScanCycle().success &&
              executor.GetWordValue({model::DeviceKind::kD, 0}) == 32 &&
              executor.GetWordValue({model::DeviceKind::kD, 1}) == 5,
          "DECO assembles its low-order source bits");
  Require(model::CompileStatements({{"LD", {"X0"}},
                                    {"MOV", {"H0089", "D0"}},
                                    {"ENCO", {"D0", "D1", "K4"}},
                                    {"END", {}}},
                                   &program, &error),
          error);
  Require(executor.LoadProgram(program) &&
              executor.ExecuteScanCycle().success &&
              executor.GetWordValue({model::DeviceKind::kD, 1}) == 7,
          "ENCO selects the highest active bit");
  Require(model::CompileStatements({{"LD", {"X0"}},
                                    {"DECO", {"K3", "M100", "K2"}},
                                    {"MOV", {"K1", "D40"}},
                                    {"MOV", {"K2", "D41"}},
                                    {"MOV", {"K3", "D42"}},
                                    {"MOV", {"K2", "D5"}},
                                    {"WSFL", {"D41", "D40", "K4", "D5"}},
                                    {"END", {}}},
                                   &program, &error),
          error);
  Require(
      executor.LoadProgram(program) && executor.ExecuteScanCycle().success &&
          executor.GetMemory(103) && !executor.GetMemory(100) &&
          executor.GetWordValue({model::DeviceKind::kD, 40}) == 2 &&
          executor.GetWordValue({model::DeviceKind::kD, 41}) == 3 &&
          executor.GetWordValue({model::DeviceKind::kD, 42}) == 1 &&
          executor.GetWordValue({model::DeviceKind::kD, 43}) == 2,
      "Immediate DECO and overlapping word shift capture source before writes");
  Require(model::CompileStatements({{"LD", {"X0"}},
                                    {"MOV", {"K5", "D5"}},
                                    {"SFTL", {"M0", "M10", "K4", "D5"}},
                                    {"END", {}}},
                                   &program, &error),
          error);
  Require(executor.LoadProgram(program) &&
              !executor.ExecuteScanCycle().success &&
              executor.GetLastExecutionResult().errorMessage.find(
                  "Invalid shift register") != std::string::npos,
          "Runtime shift count validation for D operands");
  for (const auto& definition : model::GetInstructionDefinitions()) {
    if (definition.opcode < model::Opcode::kRotateRight ||
        definition.opcode > model::Opcode::kBitTest)
      continue;
    model::InstructionStatement statement{
        std::string(definition.mnemonic) + "P", {}};
    for (size_t index = 0; index < definition.operand_count; ++index) {
      const auto rule = definition.operand_rules[index];
      statement.operands.push_back(
          rule == model::OperandRule::kImmediate ||
                  rule == model::OperandRule::kCount
              ? "K1"
          : rule == model::OperandRule::kBitRange ? "X0"
          : rule == model::OperandRule::kWritableBitRange
              ? "M10"
              : "D" + std::to_string(index * 10));
    }
    std::string input = statement.mnemonic;
    for (const auto& operand : statement.operands)
      input += " " + operand;
    plc::LadderInstruction cell;
    Require(model::ParseCellInput(input, Type::APPLICATION, &cell, &error) &&
                model::FormatCellInput(cell) == input,
            "Inline input and formatting for all new P instructions: " + input);
  }
  for (const auto& invalid : std::vector<model::InstructionStatement>{
           {"ROR", {"D0", "K17"}},
           {"DBON", {"D0", "M0", "K32"}},
           {"DECO", {"D0", "D1", "K5"}},
           {"ENCO", {"M7679", "D0", "K2"}},
           {"SFTL", {"M0", "M1", "K2", "K3"}},
           {"WSFR", {"D0", "D7999", "K3", "K1"}},
           {"DDECO", {"D0", "D1", "K4"}}}) {
    Require(!model::CompileStatements({{"LD", {"X0"}}, invalid, {"END", {}}},
                                      &program, &error),
            "Reject out-of-range or unsupported data instruction");
  }
}

void FxCpuInstructions() {
  const std::vector<model::InstructionStatement> statements{
      {"LD", {"X0"}},
      {"CML", {"H00FF", "D0"}},
      {"MOV", {"K1234", "D1"}},
      {"MOV", {"K9009", "D2"}},
      {"SMOV", {"D1", "K4", "K2", "D2", "K3"}},
      {"XCH", {"D1", "D2"}},
      {"BCD", {"D2", "D3"}},
      {"BIN", {"D3", "D4"}},
      {"NEG", {"D4"}},
      {"SWAP", {"D3"}},
      {"GRY", {"K12345", "D5"}},
      {"GBIN", {"D5", "D6"}},
      {"SQR", {"K101", "D7"}},
      {"MOV", {"K-4", "D10"}},
      {"MOV", {"K-3", "D11"}},
      {"MOV", {"K-1", "D12"}},
      {"MEAN", {"D10", "D13", "K3"}},
      {"LIMIT", {"K-10", "K20", "K30", "D14"}},
      {"BAND", {"K-10", "K20", "K-15", "D15"}},
      {"DCML", {"H12345678", "D20"}},
      {"DSWAP", {"D20"}},
      {"DBCD", {"K99999999", "D22"}},
      {"DBIN", {"D22", "D24"}},
      {"DNEG", {"D24"}},
      {"DGRY", {"K2147483647", "D26"}},
      {"DGBIN", {"D26", "D28"}},
      {"DMOV", {"K2000000000", "D30"}},
      {"DMOV", {"K2000000000", "D32"}},
      {"DMEAN", {"D30", "D34", "K2"}},
      {"DSQR", {"K2147483647", "D36"}},
      {"DLIMIT", {"K-100000", "K100000", "K200000", "D38"}},
      {"DBAND", {"K-100000", "K100000", "K-200000", "D40"}},
      {"DMOV", {"K7", "D42"}},
      {"DMOV", {"K9", "D44"}},
      {"DXCH", {"D42", "D44"}},
      {"MOV", {"H1234", "D50"}},
      {"MOV", {"HABCD", "D51"}},
      {"WTOB", {"D50", "D50", "K3"}},
      {"BTOW", {"D50", "D50", "K3"}},
      {"END", {}}};
  model::ExecutionProgram program;
  std::string error;
  Require(model::CompileStatements(statements, &program, &error), error);
  const auto verify = [](const model::ExecutionProgram& candidate) {
    plc::CompiledPLCExecutor executor;
    Require(executor.LoadProgram(candidate),
            "Load extended FX CPU instructions");
    executor.SetInput(0, true);
    Require(executor.ExecuteScanCycle().success,
            "Execute extended FX CPU instructions");
    const auto word = [&executor](uint32_t index, bool wide = false) {
      return executor.GetWordValue({model::DeviceKind::kD, index}, wide);
    };
    Require(word(0) == -256 && word(1) == 9129 && word(2) == 1234 &&
                word(3) == 0x3412 && word(4) == -1234,
            "CML, SMOV decimal positions, XCH, BCD/BIN, NEG and SWAP");
    Require(word(6) == 12345 && word(7) == 10 && word(13) == -2 &&
                word(14) == 20 && word(15) == -5,
            "Gray roundtrip, integer square root, signed mean, LIMIT and BAND");
    Require(static_cast<uint32_t>(word(20, true)) == 0xCBED87A9u &&
                static_cast<uint32_t>(word(22, true)) == 0x99999999u &&
                word(24, true) == -99999999 && word(28, true) == INT32_MAX,
            "32-bit complement/byte swap, 8-digit BCD and Gray code");
    Require(word(34, true) == 2000000000 && word(36, true) == 46340 &&
                word(38, true) == 100000 && word(40, true) == -100000 &&
                word(42, true) == 9 && word(44, true) == 7,
            "64-bit sum intermediate, 32-bit SQR/range/exchange results");
    Require(word(50) == 0x1234 && word(51) == 0x00CD &&
                executor.GetDeviceState("M8021"),
            "In-place odd byte conversion and square-root borrow flag");
  };
  verify(program);
  plc::LadderProgram ladder;
  Require(model::MaterializeLadder(program, &ladder, &error), error);
  std::string csv;
  Require(ExportTextGXCSV(ladder, "FXCPU", &csv, &error), error);
  plc::LadderProgram imported;
  Require(model::ImportGXCSV(csv, &imported, &error), error);
  verify(Compile(imported));
  plc::LadderInstruction cell;
  Require(model::ParseCellInput("SMOVP D1 K4 K2 D2 K3", Type::APPLICATION,
                                &cell, &error) &&
              cell.operands.size() == 5,
          "Inline five-operand instruction input");

  plc::CompiledPLCExecutor executor;
  Require(model::CompileStatements({{"LD", {"X0"}},
                                    {"SFWRP", {"K7", "D100", "K3"}},
                                    {"LD", {"X1"}},
                                    {"SFRDP", {"D100", "D110", "K3"}},
                                    {"END", {}}},
                                   &program, &error),
          error);
  Require(executor.LoadProgram(program), "Load FIFO instructions");
  const auto scan = [&executor](bool write, bool read) {
    executor.SetInput(0, write);
    executor.SetInput(1, read);
    Require(executor.ExecuteScanCycle().success, "FIFO scan");
  };
  scan(true, false);
  scan(false, false);
  scan(true, false);
  Require(executor.GetWordValue({model::DeviceKind::kD, 100}) == 2,
          "FIFO stores pulse writes and increments pointer");
  scan(false, false);
  scan(true, false);
  Require(executor.GetDeviceState("M8022") &&
              executor.GetWordValue({model::DeviceKind::kD, 100}) == 2,
          "Full FIFO raises carry without overwriting data");
  scan(false, true);
  Require(executor.GetWordValue({model::DeviceKind::kD, 110}) == 7 &&
              executor.GetWordValue({model::DeviceKind::kD, 100}) == 1,
          "FIFO reads oldest word and decrements pointer");
  scan(false, false);
  scan(false, true);
  Require(executor.GetDeviceState("M8020") &&
              executor.GetWordValue({model::DeviceKind::kD, 100}) == 0,
          "FIFO zero flag after consuming last word");
  scan(false, false);
  scan(false, true);
  Require(executor.GetWordValue({model::DeviceKind::kD, 110}) == 7,
          "Empty FIFO preserves destination");

  const std::vector<model::InstructionStatement> clock{{"LD", {"X0"}},
                                                       {"MOV", {"K24", "D200"}},
                                                       {"MOV", {"K2", "D201"}},
                                                       {"MOV", {"K29", "D202"}},
                                                       {"MOV", {"K12", "D203"}},
                                                       {"MOV", {"K34", "D204"}},
                                                       {"MOV", {"K56", "D205"}},
                                                       {"MOV", {"K0", "D206"}},
                                                       {"TWRP", {"D200"}},
                                                       {"LD", {"M8000"}},
                                                       {"TRD", {"D210"}},
                                                       {"LD", {"X1"}},
                                                       {"MOV", {"K30", "D202"}},
                                                       {"TWRP", {"D200"}},
                                                       {"END", {}}};
  Require(model::CompileStatements(clock, &program, &error), error);
  Require(executor.LoadProgram(program), "Load RTC instructions");
  executor.SetInput(0, true);
  executor.SetInput(1, false);
  Require(executor.ExecuteScanCycle().success &&
              executor.GetWordValue({model::DeviceKind::kD, 210}) == 24 &&
              executor.GetWordValue({model::DeviceKind::kD, 212}) == 29 &&
              executor.GetWordValue({model::DeviceKind::kD, 216}) == 4,
          "RTC writes leap date, reads seven words and computes weekday");
  executor.SetInput(0, false);
  executor.SetInput(1, true);
  Require(executor.ExecuteScanCycle().success &&
              executor.ExecuteScanCycle().success &&
              executor.GetWordValue({model::DeviceKind::kD, 212}) == 29,
          "Impossible RTC date keeps previous clock");
  executor.ResetMemory();
  executor.SetInput(0, false);
  executor.SetInput(1, false);
  Require(executor.ExecuteScanCycle().success &&
              executor.GetWordValue({model::DeviceKind::kD, 212}) == 29,
          "PLC memory reset preserves RTC");
  Require(model::MaterializeLadder(program, &ladder, &error), error);
  Require(ExportTextGXCSV(ladder, "RTC", &csv, &error), error);
  Require(model::ImportGXCSV(csv, &imported, &error), error);
  Require(executor.LoadProgram(Compile(imported)), "RTC CSV roundtrip load");
  executor.SetInput(0, true);
  Require(executor.ExecuteScanCycle().success &&
              executor.GetWordValue({model::DeviceKind::kD, 216}) == 4,
          "RTC instruction operands and scan order survive CSV roundtrip");
  for (const auto& definition : model::GetInstructionDefinitions()) {
    if (definition.opcode < model::Opcode::kComplement ||
        definition.opcode > model::Opcode::kWriteClock)
      continue;
    std::string arguments;
    for (size_t index = 0; index < definition.operand_count; ++index) {
      const auto rule = definition.operand_rules[index];
      arguments += " " + (rule == model::OperandRule::kImmediate ||
                                  rule == model::OperandRule::kCount
                              ? std::string("K2")
                          : rule == model::OperandRule::kValue
                              ? std::string("K1")
                              : "D" + std::to_string(index * 10));
    }
    const std::string base = std::string(definition.mnemonic) + "P" + arguments;
    Require(model::ParseCellInput(base, Type::APPLICATION, &cell, &error) &&
                model::FormatCellInput(cell) == base,
            "CPU P input/format: " + base);
    if (definition.supports_wide) {
      const std::string wide = "D" + base;
      Require(model::ParseCellInput(wide, Type::APPLICATION, &cell, &error) &&
                  model::FormatCellInput(cell) == wide,
              "CPU DP input/format: " + wide);
    }
  }
  for (const auto& invalid : std::vector<model::InstructionStatement>{
           {"SMOV", {"D0", "K1", "K2", "D1", "K3"}},
           {"MEAN", {"D0", "D1", "K-1"}},
           {"DMEAN", {"D7999", "D0", "K2"}},
           {"WTOB", {"D0", "D7999", "K3"}},
           {"SFWR", {"K1", "D7999", "K3"}},
           {"TRD", {"D7994"}},
           {"DSFRD", {"D0", "D10", "K4"}}})
    Require(!model::CompileStatements({{"LD", {"X0"}}, invalid, {"END", {}}},
                                      &program, &error),
            "Reject invalid CPU operand ranges or unsupported D variant");
  for (const auto& invalid : std::vector<model::InstructionStatement>{
           {"BCD", {"K10000", "D0"}},
           {"BIN", {"H12AF", "D0"}},
           {"SQR", {"K-1", "D0"}},
           {"LIMIT", {"K10", "K0", "K5", "D0"}}}) {
    Require(model::CompileStatements({{"LD", {"X0"}}, invalid, {"END", {}}},
                                     &program, &error),
            error);
    Require(executor.LoadProgram(program), "Load runtime value error");
    executor.SetInput(0, true);
    Require(!executor.ExecuteScanCycle().success,
            "Reject invalid source values");
  }
}

void ScanObservations() {
  plc::LadderProgram ladder;
  ladder.rungs[0].cells = {
      {Type::XIC, "X0"}, {Type::XIC, "X1"}, {Type::SET, "M0"}};
  plc::CompiledPLCExecutor executor;
  Require(executor.LoadProgram(Compile(ladder)), "Load monitored ladder");
  Require(!executor.GetCellPower(0, 2), "No observation before first scan");
  executor.SetInput(1, true);
  Require(executor.ExecuteScanCycle().success, "Blocked scan");
  Require(executor.GetCellPower(0, 1) == false &&
              executor.GetCellPower(0, 2) == false,
          "Downstream contact and SET require incoming power");
  executor.SetInput(0, true);
  Require(executor.ExecuteScanCycle().success && executor.GetMemory(0) &&
              executor.GetCellPower(0, 2) == true,
          "SET observed when powered");
  executor.SetInput(0, false);
  Require(
      executor.ExecuteScanCycle().success && executor.GetMemory(0) &&
          executor.GetCellPower(0, 2) == false,
      "Retained SET bit does not falsely highlight an inactive instruction");
  executor.ResetMemory();
  Require(!executor.GetCellPower(0, 2), "Reset invalidates scan observations");
  plc::LadderProgram materialized;
  std::string layout_error;
  Require(
      model::MaterializeLadder(Compile(ladder), &materialized, &layout_error),
      layout_error);
  Require(executor.LoadProgram(Compile(materialized)), "Import serial power");
  executor.SetInput(1, true);
  Require(executor.ExecuteScanCycle().success &&
              executor.GetCellPower(0, 1) == false,
          "Imported contact also accounts for upstream series conditions");

  model::ExecutionProgram program;
  std::string error;
  Require(model::CompileStatements(
              {{"LD", {"X0"}}, {"MOVP", {"K7", "D0"}}, {"END", {}}}, &program,
              &error),
          error);
  plc::LadderProgram imported;
  Require(model::MaterializeLadder(program, &imported, &error), error);
  Require(executor.LoadProgram(Compile(imported)),
          "Load imported observations");
  executor.SetInput(0, true);
  Require(executor.ExecuteScanCycle().success &&
              executor.GetCellPower(0, 11) == true,
          "Imported application instruction has its displayed coordinates");
  Require(executor.ExecuteScanCycle().success &&
              executor.GetCellPower(0, 11) == false,
          "Pulse instruction only highlights on the triggering scan");

  Require(model::CompileStatements(
              {{"LD", {"X0"}}, {"DIV", {"K1", "K0", "D0"}}, {"END", {}}},
              &program, &error),
          error);
  Require(executor.LoadProgram(program), "Load division failure program");
  const auto result = executor.ExecuteScanCycle();
  Require(
      !result.success &&
          result.errorMessage.find("Division by zero") != std::string::npos &&
          executor.GetLastExecutionResult().errorMessage == result.errorMessage,
      "Failed scan preserves the actual executor error");
  Require(!executor.GetCellPower(0, 11),
          "Failed scan hides stale observations");
}

void GXWrappingSymbols() {
  plc::LadderInstruction marker;
  std::string error;
  Require(model::ParseCellInput("K0", Type::HLINE, &marker, &error), error);
  Require(marker.type == Type::kWrappingSource && marker.address == "K0",
          "F9 K0 resolves as a wrapping symbol, not a PLC constant");
  plc::LadderProgram ladder;
  ladder.rungs.clear();
  ladder.rungs.emplace_back();
  ladder.rungs.emplace_back();
  ladder.rungs[0].cells = {{Type::XIC, "X0"}, marker};
  marker.type = Type::kWrappingDestination;
  ladder.rungs[1].cells = {marker, {Type::XIC, "X1"}, {Type::OTE, "Y0"}};
  const auto program = Compile(ladder);
  plc::LadderProgram compact;
  Require(model::MaterializeLadder(program, &compact, &error), error);
  Require(compact.rungs.size() == 2 && compact.rungs[0].cells.size() == 12,
          "F4 can compact an unnecessary wrap into one fixed-width row");
  plc::CompiledPLCExecutor executor;
  Require(executor.LoadProgram(program), "Load paired wrapping symbols");
  for (int bits = 0; bits < 4; ++bits) {
    executor.SetInput(0, (bits & 1) != 0);
    executor.SetInput(1, (bits & 2) != 0);
    Require(executor.ExecuteScanCycle().success &&
                executor.GetOutput(0) == (bits == 3),
            "Wrapping carries the source condition into the destination row");
  }
  std::string csv;
  Require(ExportTextGXCSV(ladder, "wrap", &csv, &error), error);
  plc::LadderProgram imported;
  Require(model::ImportGXCSV(csv, &imported, &error), error);
  Require(executor.LoadProgram(Compile(imported)), "Wrapped CSV load");
  executor.SetInput(0, false);
  Require(executor.ExecuteScanCycle().success && !executor.GetOutput(0),
          "CSV lowers display-only wrapping without losing source contact");
  ladder.rungs[1].cells[0].address = "K1";
  model::ExecutionProgram invalid;
  Require(!model::CompileLadder(ladder, &invalid, &error), "Unpaired wrapping");
  ladder.rungs[1].cells[0].address = "K0";
  ladder.rungs[0].cells[0] = {Type::kWrappingDestination, "K1"};
  ladder.rungs[1].cells[1] = {Type::kWrappingSource, "K1"};
  ladder.rungs[1].cells[2] = {Type::EMPTY, ""};
  ladder.rungs.emplace_back();
  ladder.rungs[2].cells = {{Type::EMPTY, ""}, {Type::OTE, "Y0"}};
  ladder.verticalConnections.emplace_back(1, 1, 2);
  Require(!model::CompileLadder(ladder, &invalid, &error), "Cyclic wrapping");
}
void FxPackedBitsAndWideContacts() {
  std::string error;
  model::ExecutionProgram program;
  Require(model::CompileStatements({{"LD", {"M8000"}},
                                    {"MOV", {"HABCD", "K4M100"}},
                                    {"MOV", {"K4M100", "D0"}},
                                    {"MOV", {"K2M100", "D1"}},
                                    {"INC", {"K1M100"}},
                                    {"MOV", {"K1M100", "D2"}},
                                    {"DMOV", {"H80000001", "K8M200"}},
                                    {"DMOV", {"K8M200", "D10"}},
                                    {"MOV", {"K4X360", "D20"}},
                                    {"DLD=", {"D10", "H80000001"}},
                                    {"DAND<", {"D10", "K0"}},
                                    {"OUT", {"Y377"}},
                                    {"END", {}}},
                                   &program, &error),
          error);
  plc::CompiledPLCExecutor executor;
  Require(executor.LoadProgram(program), "Load packed bit operands");
  executor.SetInput(240, true);
  executor.SetInput(255, true);
  Require(executor.ExecuteScanCycle().success &&
              static_cast<uint16_t>(executor.GetMemory().D[0]) == 0xABCD &&
              executor.GetMemory().D[1] == 205 &&
              executor.GetMemory().D[2] == 14 &&
              executor.GetWordValue({model::DeviceKind::kD, 10}, true) ==
                  INT32_MIN + 1 &&
              static_cast<uint16_t>(executor.GetMemory().D[20]) == 0x8001 &&
              executor.GetOutput(255),
          "Packed reads zero extend, writes preserve neighbors, 32-bit compare "
          "uses full data");
  plc::LadderProgram ladder;
  Require(model::MaterializeLadder(program, &ladder, &error), error);
  std::string csv;
  Require(ExportTextGXCSV(ladder, "packed", &csv, &error), error);
  Require(csv.find("\"LDD=\"") != std::string::npos &&
              csv.find("\"ANDD<\"") != std::string::npos &&
              csv.find("\"DLD=\"") == std::string::npos,
          "GX CSV uses native double-word comparison mnemonics");
  plc::LadderProgram imported;
  Require(model::ImportGXCSV(csv, &imported, &error), error);
  imported.canonical_program.reset();
  Require(executor.LoadProgram(Compile(imported)) &&
              executor.ExecuteScanCycle().success && executor.GetOutput(255),
          "Packed operands and wide comparisons survive CSV and grid editing");
  Require(!model::ParseOperand("K4M7670") && !model::ParseOperand("K4X370") &&
              !model::ParseOperand("K9M0"),
          "Packed ranges and digit count validation");
  Require(!model::CompileStatements(
              {{"LD", {"M8000"}}, {"MOV", {"K8M0", "D0"}}, {"END", {}}},
              &program, &error),
          "16-bit instruction rejects eight packed digits");
}

void FxBasicControl() {
  std::string error;
  model::ExecutionProgram program;
  Require(model::CompileStatements({{"LD", {"X0"}},
                                    {"MC", {"N0", "M100"}},
                                    {"LD", {"X1"}},
                                    {"OUT", {"Y0"}},
                                    {"PLS", {"M0"}},
                                    {"LD", {"X2"}},
                                    {"MC", {"N1", "M101"}},
                                    {"LD", {"M8000"}},
                                    {"SET", {"M10"}},
                                    {"OUT", {"Y1"}},
                                    {"MCR", {"N0"}},
                                    {"LD", {"X1"}},
                                    {"PLF", {"M1"}},
                                    {"LD", {"X0"}},
                                    {"AND", {"X1"}},
                                    {"MEP", {}},
                                    {"OUT", {"Y2"}},
                                    {"NOP", {}},
                                    {"END", {}}},
                                   &program, &error),
          error);
  plc::CompiledPLCExecutor executor;
  Require(executor.LoadProgram(program),
          "Load nested MC and pulse instructions");
  executor.SetInput(0, true);
  executor.SetInput(1, true);
  executor.SetInput(2, true);
  Require(executor.ExecuteScanCycle().success && executor.GetOutput(0) &&
              executor.GetOutput(1) && executor.GetOutput(2) &&
              executor.GetMemory(0) && executor.GetMemory(10),
          "MC powers nested outputs and PLS/MEP detect first rising edge");
  Require(executor.ExecuteScanCycle().success && !executor.GetOutput(2) &&
              !executor.GetMemory(0),
          "PLS and compound MEP clear while held");
  executor.SetInput(0, false);
  executor.SetInput(1, false);
  Require(
      executor.ExecuteScanCycle().success && !executor.GetOutput(0) &&
          !executor.GetOutput(1) && executor.GetMemory(10) &&
          executor.GetMemory(1),
      "Disabled MC clears ordinary coils, preserves SET and MCR restores bus");
  Require(executor.ExecuteScanCycle().success && !executor.GetMemory(1),
          "PLF clears after falling edge scan");
  plc::LadderProgram ladder;
  Require(model::MaterializeLadder(program, &ladder, &error), error);
  std::string csv;
  Require(ExportTextGXCSV(ladder, "basic", &csv, &error), error);
  plc::LadderProgram imported;
  Require(model::ImportGXCSV(csv, &imported, &error), error);
  Require(executor.LoadProgram(Compile(imported)),
          "Basic control CSV roundtrip");
  // Exercise the grid compiler rather than its unchanged canonical snapshot.
  imported.canonical_program.reset();
  Require(executor.LoadProgram(Compile(imported)),
          "Materialized compound MEP and MC compile from edited grid");
  executor.SetInput(0, true);
  executor.SetInput(1, true);
  executor.SetInput(2, true);
  Require(executor.ExecuteScanCycle().success && executor.GetOutput(2),
          "Edited grid preserves compound pulse semantics");
  Require(!model::CompileStatements(
              {{"LD", {"X0"}}, {"MC", {"N0", "M0"}}, {"END", {}}}, &program,
              &error),
          "Reject unterminated MC");
}

void FxTables() {
  std::string error;
  model::ExecutionProgram program;
  Require(model::CompileStatements({{"LD", {"M8000"}},
                                    {"MOV", {"K5", "D0"}},
                                    {"MOV", {"K-2", "D1"}},
                                    {"MOV", {"K5", "D2"}},
                                    {"MOV", {"K9", "D3"}},
                                    {"SER", {"D0", "K5", "D10", "K4"}},
                                    {"WSUM", {"D0", "D20", "K4"}},
                                    {"BK+", {"D0", "K2", "D30", "K4"}},
                                    {"BKCMP>=", {"K5", "D0", "M0", "K4"}},
                                    {"DIS", {"H1234", "D40", "K4"}},
                                    {"UNI", {"D40", "D50", "K4"}},
                                    {"MOV", {"H8001", "D60"}},
                                    {"SFR", {"D60", "K17"}},
                                    {"MOV", {"K2", "D70"}},
                                    {"MOV", {"K111", "D71"}},
                                    {"MOV", {"K222", "D72"}},
                                    {"FINS", {"K123", "D70", "K2"}},
                                    {"FDEL", {"D80", "D70", "K3"}},
                                    {"POP", {"D70", "D90", "K4"}},
                                    {"DMOV", {"K2147483647", "D100"}},
                                    {"DMOV", {"K2147483647", "D102"}},
                                    {"DWSUM", {"D100", "D110", "K2"}},
                                    {"END", {}}},
                                   &program, &error),
          error);
  plc::CompiledPLCExecutor executor;
  Require(executor.LoadProgram(program) && executor.ExecuteScanCycle().success,
          "Load table operations");
  Require(
      executor.GetMemory().D[10] == 2 && executor.GetMemory().D[11] == 0 &&
          executor.GetMemory().D[12] == 2 && executor.GetMemory().D[13] == 1 &&
          executor.GetMemory().D[14] == 3 &&
          executor.GetWordValue({model::DeviceKind::kD, 20}, true) == 17 &&
          executor.GetMemory().D[30] == 7 && executor.GetMemory().D[31] == 0 &&
          executor.GetMemory(0) && executor.GetMemory(1) &&
          executor.GetMemory(2) && !executor.GetMemory(3),
      "SER positions, signed WSUM, block arithmetic and comparison");
  Require(
      executor.GetMemory().D[50] == 0x1234 &&
          executor.GetMemory().D[60] == 0x4000 &&
          executor.GetDeviceState("M8022") && executor.GetMemory().D[70] == 1 &&
          executor.GetMemory().D[80] == 222 &&
          executor.GetMemory().D[90] == 123 &&
          executor.GetMemory().D[110] == -2 &&
          executor.GetMemory().D[111] == -1 &&
          executor.GetMemory().D[112] == 0 && executor.GetMemory().D[113] == 0,
      "Nibble conversion, shift carry, table insertion/deletion, POP and "
      "64-bit sum");
  plc::LadderProgram ladder;
  Require(model::MaterializeLadder(program, &ladder, &error), error);
  std::string csv;
  Require(ExportTextGXCSV(ladder, "tables", &csv, &error), error);
  plc::LadderProgram imported;
  Require(model::ImportGXCSV(csv, &imported, &error), error);
  Require(executor.LoadProgram(Compile(imported)) &&
              executor.ExecuteScanCycle().success &&
              executor.GetMemory().D[90] == 123,
          "Table instruction CSV roundtrip");
  Require(model::CompileStatements({{"LD", {"M8002"}},
                                    {"MOV", {"K1", "D0"}},
                                    {"MOV", {"K2", "D1"}},
                                    {"MOV", {"K3", "D2"}},
                                    {"MOV", {"K30", "D3"}},
                                    {"MOV", {"K10", "D4"}},
                                    {"MOV", {"K20", "D5"}},
                                    {"LD", {"X0"}},
                                    {"SORT", {"D0", "K3", "K2", "D10", "K2"}},
                                    {"END", {}}},
                                   &program, &error),
          error);
  Require(executor.LoadProgram(program), "Load column-major SORT");
  executor.SetInput(0, true);
  Require(
      executor.ExecuteScanCycle().success && !executor.GetDeviceState("M8029"),
      "SORT starts on first scan");
  Require(
      executor.ExecuteScanCycle().success && !executor.GetDeviceState("M8029"),
      "SORT remains busy for m1 scans");
  Require(
      executor.ExecuteScanCycle().success && executor.GetDeviceState("M8029") &&
          executor.GetMemory().D[10] == 2 && executor.GetMemory().D[11] == 3 &&
          executor.GetMemory().D[12] == 1 && executor.GetMemory().D[13] == 10 &&
          executor.GetMemory().D[14] == 20,
      "SORT preserves entire rows and completes after m1 scans");
  executor.SetInput(0, false);
  Require(
      executor.ExecuteScanCycle().success && !executor.GetDeviceState("M8029"),
      "SORT clears completion when off");
  for (const auto& invalid : std::vector<model::InstructionStatement>{
           {"SER", {"D0", "K0", "D7998", "K2"}},
           {"BK+", {"D0", "D10", "D1", "K3"}},
           {"DIS", {"K0", "D0", "K5"}},
           {"WSUM", {"D0", "D7999", "K1"}},
           {"SORT", {"D0", "K33", "K2", "D10", "K1"}}})
    Require(!model::CompileStatements({{"LD", {"M8000"}}, invalid, {"END", {}}},
                                      &program, &error),
            "Table constant limits, overlap and footprints rejected by F4");
}

void FxStrings() {
  std::string error;
  model::ExecutionProgram program;
  plc::CompiledPLCExecutor executor;
  const auto text = [&](uint32_t head) {
    std::string result;
    for (uint32_t index = head; index < 8000; ++index) {
      const uint16_t word = executor.GetMemory().D[index];
      for (int shift : {0, 8}) {
        const char character = static_cast<char>(word >> shift);
        if (!character)
          return result;
        result += character;
      }
    }
    throw std::runtime_error("Test string has no terminator");
  };
  Require(model::CompileStatements({{"LD", {"M8000"}},
                                    {"$MOV", {"\"Ab c\"", "D0"}},
                                    {"$+", {"D0", "\"De\"", "D10"}},
                                    {"LEN", {"D10", "D20"}},
                                    {"RIGHT", {"D10", "D30", "K2"}},
                                    {"LEFT", {"D10", "D40", "K3"}},
                                    {"MOV", {"K2", "D50"}},
                                    {"MOV", {"K3", "D51"}},
                                    {"MIDR", {"D10", "D60", "D50"}},
                                    {"MIDW", {"\"XYZ\"", "D10", "D50"}},
                                    {"INSTR", {"D10", "\"XYZ\"", "D70", "K1"}},
                                    {"MOV", {"K8", "D80"}},
                                    {"MOV", {"K2", "D81"}},
                                    {"STR", {"D80", "K12672", "D90"}},
                                    {"VAL", {"D90", "D100", "D110"}},
                                    {"DEMOV", {"E-1.23456", "D120"}},
                                    {"MOV", {"K0", "D130"}},
                                    {"MOV", {"K8", "D131"}},
                                    {"MOV", {"K3", "D132"}},
                                    {"DESTR", {"D120", "D130", "D140"}},
                                    {"DEVAL", {"D140", "D150"}},
                                    {"DEBCD", {"D120", "D160"}},
                                    {"DEBIN", {"D160", "D170"}},
                                    {"END", {}}},
                                   &program, &error),
          error);
  Require(executor.LoadProgram(program) && executor.ExecuteScanCycle().success,
          "Execute string and float string program");
  Require(text(0) == "Ab c" && text(10) == "AXYZDe" &&
              executor.GetMemory().D[20] == 6 && text(30) == "De" &&
              text(40) == "Ab " && text(60) == "b c" &&
              executor.GetMemory().D[70] == 2,
          "Byte strings preserve case, spaces, extraction and replacement");
  Require(text(90) == "  126.72" && executor.GetMemory().D[100] == 8 &&
              executor.GetMemory().D[101] == 2 &&
              executor.GetMemory().D[110] == 12672 && text(140) == "-  1.235" &&
              executor.GetMemory().D[160] == -1235 &&
              executor.GetMemory().D[161] == -3,
          "STR/VAL display specification and scientific conversion");
  plc::LadderProgram ladder;
  Require(model::MaterializeLadder(program, &ladder, &error), error);
  std::string csv;
  Require(ExportTextGXCSV(ladder, "strings", &csv, &error), error);
  plc::LadderProgram imported;
  Require(model::ImportGXCSV(csv, &imported, &error), error);
  Require(executor.LoadProgram(Compile(imported)) &&
              executor.ExecuteScanCycle().success && text(10) == "AXYZDe",
          "String literals survive GX quoting and CSV roundtrip");
  plc::LadderInstruction cell;
  Require(model::ParseCellInput("$MOV \"Ab c\" D0", Type::APPLICATION, &cell,
                                &error) &&
              model::FormatCellInput(cell) == "$MOV \"Ab c\" D0",
          "Editor preserves quoted literal spaces and case");
  Require(model::CompileStatements({{"LD", {"M8000"}},
                                    {"$MOV", {"\"ABCDE\"", "D0"}},
                                    {"$MOV", {"D0", "D1"}},
                                    {"END", {}}},
                                   &program, &error),
          error);
  Require(executor.LoadProgram(program) &&
              executor.ExecuteScanCycle().success && text(1) == "ABCDE",
          "Overlapping string move snapshots source");
  Require(model::CompileStatements(
              {{"LD", {"M8000"}}, {"$MOV", {"\"AB\"", "D7999"}}, {"END", {}}},
              &program, &error),
          error);
  Require(executor.LoadProgram(program) && !executor.ExecuteScanCycle().success,
          "String range failure preserves memory and reports error");
}

void FxFloatingPoint() {
  std::string error;
  model::ExecutionProgram program;
  Require(model::CompileStatements({{"LD", {"M8000"}},
                                    {"DEMOV", {"E1.5", "D0"}},
                                    {"DEMOV", {"E2.25", "D2"}},
                                    {"DEADD", {"D0", "D2", "D4"}},
                                    {"DEMUL", {"D4", "K2", "D6"}},
                                    {"INT", {"D6", "D8"}},
                                    {"DECMP", {"D0", "D2", "M10"}},
                                    {"DEZCP", {"K1", "K2", "D0", "M20"}},
                                    {"DRAD", {"K90", "D10"}},
                                    {"DSIN", {"D10", "D12"}},
                                    {"DINT", {"D12", "D14"}},
                                    {"DFLT", {"D14", "D16"}},
                                    {"DLOG10", {"K100", "D18"}},
                                    {"INT", {"D18", "D20"}},
                                    {"END", {}}},
                                   &program, &error),
          error);
  plc::CompiledPLCExecutor executor;
  Require(executor.LoadProgram(program), "Load float program");
  Require(
      executor.ExecuteScanCycle().success && executor.GetMemory().D[8] == 7 &&
          executor.GetMemory().D[14] == 1 && executor.GetMemory().D[20] == 2 &&
          executor.GetMemory(12) && executor.GetMemory(21),
      "IEEE float arithmetic, trigonometry, integer conversion and compares");
  const auto exponent = model::ParseOperand("E1.234+3");
  Require(exponent &&
              model::ParseOperand(model::FormatOperand(*exponent)) == exponent,
          "GX exponent notation roundtrip");
  plc::LadderProgram ladder;
  Require(model::MaterializeLadder(program, &ladder, &error), error);
  std::string csv;
  Require(ExportTextGXCSV(ladder, "float", &csv, &error), error);
  plc::LadderProgram imported;
  Require(model::ImportGXCSV(csv, &imported, &error), error);
  Require(executor.LoadProgram(Compile(imported)) &&
              executor.ExecuteScanCycle().success &&
              executor.GetMemory().D[8] == 7,
          "Float constants and D prefixes survive CSV roundtrip");
  for (const auto& invalid : std::vector<model::InstructionStatement>{
           {"EADD", {"D0", "D2", "D4"}},
           {"DEMOV", {"E1", "D7999"}},
           {"INT", {"D7999", "D0"}},
           {"DECMP", {"K1", "K2", "M7679"}}})
    Require(!model::CompileStatements({{"LD", {"M8000"}}, invalid, {"END", {}}},
                                      &program, &error),
            "Reject unsupported float width and memory ranges");
  Require(model::CompileStatements({{"LD", {"M8000"}},
                                    {"MOV", {"K123", "D0"}},
                                    {"DEMOV", {"E40000", "D2"}},
                                    {"INT", {"D2", "D0"}},
                                    {"END", {}}},
                                   &program, &error),
          error);
  Require(
      executor.LoadProgram(program) && executor.ExecuteScanCycle().success &&
          executor.GetMemory().D[0] == 123 && executor.GetDeviceState("M8022"),
      "INT overflow sets carry and preserves destination");
  Require(model::CompileStatements(
              {{"LD", {"M8000"}}, {"DEDIV", {"K1", "K0", "D0"}}, {"END", {}}},
              &program, &error),
          error);
  Require(executor.LoadProgram(program) && !executor.ExecuteScanCycle().success,
          "Float zero divisor reports execution error");
}

void FxProgramFlow() {
  std::string error;
  model::ExecutionProgram program;
  const auto compile =
      [&](std::vector<model::InstructionStatement> statements) {
        Require(model::CompileStatements(statements, &program, &error), error);
      };
  plc::CompiledPLCExecutor executor;
  compile({{"LD", {"M8000"}},
           {"MOV", {"K0", "D0"}},
           {"FOR", {"K3"}},
           {"FOR", {"K2"}},
           {"LD", {"M8000"}},
           {"INC", {"D0"}},
           {"NEXT", {}},
           {"NEXT", {}},
           {"END", {}}});
  Require(executor.LoadProgram(program), "Load nested FOR");
  Require(executor.ExecuteScanCycle().success && executor.GetMemory().D[0] == 6,
          "Nested FOR executes its body exactly six times");
  compile({{"LD", {"M8000"}},
           {"MOV", {"K0", "D0"}},
           {"P0", {}},
           {"LD", {"M8000"}},
           {"INC", {"D0"}},
           {"LD<", {"D0", "K4"}},
           {"CJ", {"P0"}},
           {"LD", {"M8000"}},
           {"CJ", {"P63"}},
           {"MOV", {"K99", "D0"}},
           {"END", {}}});
  Require(executor.LoadProgram(program), "Load backwards CJ");
  Require(executor.ExecuteScanCycle().success && executor.GetMemory().D[0] == 4,
          "CJ re-evaluates conditions and P63 jumps to END");
  compile({{"LD", {"X0"}},
           {"CALLP", {"P1"}},
           {"OUT", {"Y0"}},
           {"FEND", {}},
           {"P1", {}},
           {"LD", {"M8000"}},
           {"INC", {"D0"}},
           {"CALL", {"P2"}},
           {"SRET", {}},
           {"P2", {}},
           {"LD", {"M8000"}},
           {"INC", {"D1"}},
           {"SRET", {}},
           {"END", {}}});
  Require(executor.LoadProgram(program), "Load CALLP and nested CALL");
  executor.SetInput(0, true);
  Require(executor.ExecuteScanCycle().success && executor.GetOutput(0) &&
              executor.GetMemory().D[0] == 5 && executor.GetMemory().D[1] == 1,
          "SRET restores caller condition and continues after CALL");
  Require(executor.ExecuteScanCycle().success && executor.GetMemory().D[1] == 1,
          "CALLP does not repeat with held input");
  plc::LadderProgram ladder;
  Require(model::MaterializeLadder(program, &ladder, &error), error);
  std::string csv;
  Require(ExportTextGXCSV(ladder, "flow", &csv, &error), error);
  plc::LadderProgram imported;
  Require(model::ImportGXCSV(csv, &imported, &error), error);
  Require(model::SerializeInstructions(Compile(imported)).size() == 14,
          "CSV preserves pointers, independent instructions and CALLP");
  compile({{"EI", {}},
           {"LD", {"M8000"}},
           {"INC", {"D2"}},
           {"FEND", {}},
           {"I001", {}},
           {"LD", {"M8000"}},
           {"INC", {"D3"}},
           {"IRET", {}},
           {"END", {}}});
  Require(executor.LoadProgram(program), "Load interrupt");
  executor.SetInput(0, false);
  executor.SetInput(0, true);
  Require(executor.ExecuteScanCycle().success &&
              executor.GetMemory().D[2] == 1 && executor.GetMemory().D[3] == 1,
          "EI dispatches input interrupt and IRET resumes main");
  Require(executor.ExecuteScanCycle().success && executor.GetMemory().D[3] == 1,
          "Interrupt edge is consumed once");
  Require(
      !model::CompileStatements({{"NEXT", {}}, {"END", {}}}, &program, &error),
      "Reject unmatched NEXT");
  Require(
      !model::CompileStatements(
          {{"LD", {"M8000"}}, {"CJ", {"P9"}}, {"END", {}}}, &program, &error),
      "Reject undefined pointer");
  compile({{"P0", {}}, {"LD", {"M8000"}}, {"CJ", {"P0"}}, {"END", {}}});
  Require(executor.LoadProgram(program), "Load infinite jump for guard test");
  Require(!executor.ExecuteScanCycle().success,
          "Infinite jump produces a diagnostic instead of hanging");
  plc::LadderInstruction cell;
  Require(model::ParseCellInput("CJ P10", Type::APPLICATION, &cell, &error),
          "Cell editing accepts unresolved pointer until F4");
  Require(model::ParseCellInput("FEND", Type::APPLICATION, &cell, &error) &&
              model::FormatCellInput(cell) == "FEND",
          "Zero operand instruction editing");
}
void FxHandyInstructions() {
  plc::CompiledPLCExecutor executor;
  Require(executor.LoadProgram(Statements({{"LD", {"M8000"}},
                                           {"ASC", {"AbC", "D0"}},
                                           {"MOV", {"HAB00", "D10"}},
                                           {"SEGD", {"K7", "D10"}},
                                           {"MOV", {"H0ABC", "D100"}},
                                           {"MOV", {"H1234", "D101"}},
                                           {"ASCI", {"D100", "D200", "K5"}},
                                           {"HEX", {"D200", "D210", "K5"}},
                                           {"SET", {"M8"}},
                                           {"SET", {"M9"}},
                                           {"PRUN", {"K4X0", "K4M0"}},
                                           {"PRUN", {"K4M0", "K4Y0"}},
                                           {"DMOV", {"K-1234567", "C235"}},
                                           {"DHCMOV", {"C235", "D220"}},
                                           {"END", {}}})),
          "Load display and ASCII instructions");
  executor.SetInput(0, true);
  executor.SetInput(8, true);
  Require(executor.ExecuteScanCycle().success,
          "Execute display and ASCII instructions");
  const auto& memory = executor.GetMemory();
  Require(static_cast<uint16_t>(memory.D[0]) == 0x6241 &&
              static_cast<uint16_t>(memory.D[1]) == 0x2043 &&
              memory.D[2] == 0x2020 && memory.D[3] == 0x2020,
          "ASC case preservation and eight-character padding");
  Require(static_cast<uint16_t>(memory.D[10]) == 0xab27,
          "SEGD official digit seven pattern preserves high byte");
  Require(static_cast<uint16_t>(memory.D[200]) == 0x3034 &&
              static_cast<uint16_t>(memory.D[201]) == 0x4241 &&
              (memory.D[202] & 0xff) == 'C' && memory.D[210] == 0x0abc &&
              memory.D[211] == 4,
          "ASCI/HEX five-digit ordering across words");
  Require(memory.M[0] && memory.M[8] && memory.M[9] && memory.M[10] &&
              memory.Y[0] && memory.Y[8] && !memory.Y[9],
          "PRUN octal decimal groups preserve gap relays");
  Require(executor.GetWordValue({model::DeviceKind::kD, 220}, true) == -1234567,
          "HCMOV reads native 32-bit high-speed counter");
  executor.ResetMemory();
  Require(
      executor.LoadProgram(Statements({{"LD", {"M8000"}},
                                       {"SET", {"M8049"}},
                                       {"MOV", {"K100", "D20"}},
                                       {"MOV", {"K200", "D21"}},
                                       {"SET", {"M8026"}},
                                       {"RAMP", {"D20", "D21", "D30", "K2"}},
                                       {"LD", {"X0"}},
                                       {"ANS", {"T0", "K2", "S900"}},
                                       {"TTMR", {"D40", "K1"}},
                                       {"STMR", {"T10", "K2", "M100"}},
                                       {"LD", {"X1"}},
                                       {"ANRP", {}},
                                       {"END", {}}})),
      "Load handy timing instructions");
  executor.SetContinuousExecution(true, 1000);
  executor.SetInput(0, true);
  Require(executor.ExecuteScanCycle().success && memory.D[30] == 100 &&
              memory.D[40] == 10 && memory.M[100] && memory.M[102],
          "Ramp starts and teaching timer multiplies seconds");
  Require(executor.ExecuteScanCycle().success && memory.D[30] == 150 &&
              memory.S[900] && memory.special_m[48] && memory.D[8049] == 900,
          "Ramp midpoint and annunciator monitoring");
  Require(executor.ExecuteScanCycle().success && memory.D[30] == 200 &&
              memory.special_m[29],
          "Ramp reaches target");
  executor.SetInput(0, false);
  executor.SetInput(1, true);
  Require(executor.ExecuteScanCycle().success && memory.D[30] == 200 &&
              memory.D[40] == 30 && memory.D[41] == 0 && !memory.S[900] &&
              !memory.special_m[48] && memory.M[100] && memory.M[101] &&
              !memory.M[102] && memory.M[103],
          "Teaching time retained, ANR reset, STMR off-delay starts");
  Require(executor.ExecuteScanCycle().success && !memory.M[100] &&
              !memory.M[101] && !memory.M[103],
          "STMR off-delay expires");
  model::ExecutionProgram invalid;
  std::string error;
  Require(!model::CompileStatements(
              {{"LD", {"X0"}}, {"TTMR", {"D7999", "K3"}}, {"END", {}}},
              &invalid, &error),
          "F4 rejects teaching range and magnification");
}

void FxSort2() {
  plc::CompiledPLCExecutor executor;
  Require(executor.LoadProgram(
              Statements({{"LD", {"M8002"}},
                          {"DMOV", {"K30", "D0"}},
                          {"DMOV", {"K300", "D2"}},
                          {"DMOV", {"K10", "D4"}},
                          {"DMOV", {"K100", "D6"}},
                          {"DMOV", {"K20", "D8"}},
                          {"DMOV", {"K200", "D10"}},
                          {"SET", {"M8165"}},
                          {"LD", {"M8000"}},
                          {"DSORT2", {"D0", "K3", "K2", "D20", "K1"}},
                          {"END", {}}})),
          "Load DSORT2 row-major descending table");
  Require(executor.ExecuteScanCycle().success &&
              executor.ExecuteScanCycle().success &&
              executor.ExecuteScanCycle().success,
          "SORT2 completes after three scans");
  Require(executor.GetWordValue({model::DeviceKind::kD, 20}, true) == 30 &&
              executor.GetWordValue({model::DeviceKind::kD, 22}, true) == 300 &&
              executor.GetWordValue({model::DeviceKind::kD, 24}, true) == 20 &&
              executor.GetWordValue({model::DeviceKind::kD, 28}, true) == 10 &&
              executor.GetMemory().special_m[29],
          "DSORT2 sorts complete rows by key in descending order");
}

void FxVirtualModules() {
  plc::CompiledPLCExecutor executor;
  Require(executor.SetModuleBufferWord(0, 10, 1234) &&
              executor.SetModuleBufferWord(0, 11, -5678) &&
              executor.SetAnalogInput(1, 0, 3072) && executor.SetVolume(0, 255),
          "Configure virtual extension blocks");
  Require(executor.LoadProgram(
              Statements({{"LD", {"M8000"}},
                          {"FROM", {"K0", "K10", "D0", "K2"}},
                          {"TO", {"K0", "K20", "D0", "K2"}},
                          {"DTO", {"K0", "K30", "H89ABCDEF", "K2"}},
                          {"DFROM", {"K0", "K30", "D10", "K2"}},
                          {"VRRD", {"K0", "D20"}},
                          {"VRSC", {"K0", "D21"}},
                          {"RD3A", {"K1", "K21", "D22"}},
                          {"WR3A", {"K1", "K22", "D22"}},
                          {"END", {}}})) &&
              executor.ExecuteScanCycle().success,
          "Execute module transfers and analog instructions");
  const auto& memory = executor.GetMemory();
  Require(memory.D[0] == 1234 && memory.D[1] == -5678 &&
              executor.GetModuleBufferWord(0, 20) == 1234 &&
              executor.GetModuleBufferWord(0, 21) == -5678,
          "FROM/TO preserve signed words");
  Require(executor.GetWordValue({model::DeviceKind::kD, 10}, true) ==
                  std::bit_cast<int32_t>(uint32_t{0x89abcdef}) &&
              executor.GetWordValue({model::DeviceKind::kD, 12}, true) ==
                  std::bit_cast<int32_t>(uint32_t{0x89abcdef}),
          "DFROM/DTO paired BFM words and constant fill");
  Require(memory.D[20] == 255 && memory.D[21] == 10 && memory.D[22] == 3072 &&
              executor.GetAnalogOutput(1, 1) == 3072,
          "Volume and dedicated analog channels");
  executor.ResetMemory();
  for (int index = 0; index < 5; ++index)
    Require(executor.SetModuleBufferWord(0, 100 + index,
                                         static_cast<int16_t>(10 + index)),
            "Set divided transfer source");
  Require(executor.LoadProgram(
              Statements({{"LD", {"X0"}},
                          {"RBFM", {"K0", "K100", "D100", "K5", "K2"}},
                          {"WBFM", {"K0", "K200", "D100", "K5", "K2"}},
                          {"END", {}}})),
          "Load competing divided module transfers");
  executor.SetInput(0, true);
  Require(executor.ExecuteScanCycle().success && memory.D[101] == 11 &&
              memory.D[102] == 0 && memory.special_m[328] &&
              !memory.special_m[29],
          "Divided read first chunk and per-unit arbitration");
  Require(executor.ExecuteScanCycle().success && memory.D[103] == 13 &&
              memory.D[104] == 0,
          "Divided read second chunk");
  Require(executor.ExecuteScanCycle().success && memory.D[104] == 14 &&
              executor.GetModuleBufferWord(0, 201) == 11 &&
              executor.GetModuleBufferWord(0, 202) == 0,
          "Waiting write resumes after read releases unit");
  Require(executor.ExecuteScanCycle().success &&
              executor.ExecuteScanCycle().success && memory.special_m[29] &&
              executor.GetModuleBufferWord(0, 204) == 14,
          "Divided module transfer completion");
  executor.SetInput(0, false);
  Require(executor.ExecuteScanCycle().success && !memory.special_m[29],
          "Divided transfer command reset clears completion");
  Require(!executor.SetModuleBufferWord(8, 0, 0) &&
              !executor.GetModuleBufferWord(0, 32767),
          "Module API bounds");
}

void FxTimeAndConversion() {
  const auto program = Statements({{"LD", {"M8000"}},
                                   {"MOV", {"K23", "D0"}},
                                   {"MOV", {"K59", "D1"}},
                                   {"MOV", {"K59", "D2"}},
                                   {"MOV", {"K0", "D3"}},
                                   {"MOV", {"K0", "D4"}},
                                   {"MOV", {"K2", "D5"}},
                                   {"TADD", {"D0", "D3", "D10"}},
                                   {"TSUB", {"D3", "D0", "D13"}},
                                   {"TCMP", {"K23", "K59", "K59", "D0", "M0"}},
                                   {"TZCP", {"D3", "D0", "D0", "M3"}},
                                   {"DHTOS", {"D0", "D20"}},
                                   {"DSTOH", {"D20", "D22"}},
                                   {"MOV", {"H0301", "D30"}},
                                   {"MOV", {"H0203", "D31"}},
                                   {"MOV", {"H1400", "D32"}},
                                   {"CRC", {"D30", "D40", "K6"}},
                                   {"CCD", {"D30", "D41", "K3"}},
                                   {"BINDA", {"K-25108", "D50"}},
                                   {"DABIN", {"D50", "D54"}},
                                   {"DBINDA", {"K-2147483648", "D60"}},
                                   {"DDABIN", {"D60", "D66"}},
                                   {"ZONE", {"K-100", "K100", "K-20", "D70"}},
                                   {"RND", {"D71"}},
                                   {"END", {}}});
  plc::CompiledPLCExecutor executor;
  Require(executor.LoadProgram(program) && executor.ExecuteScanCycle().success,
          "Time and conversion execution");
  const auto& memory = executor.GetMemory();
  Require(memory.D[10] == 0 && memory.D[11] == 0 && memory.D[12] == 1 &&
              memory.D[13] == 0 && memory.D[14] == 0 && memory.D[15] == 3,
          "Time arithmetic wraps at midnight");
  Require(memory.M[1] && memory.M[4] && memory.D[22] == 23 &&
              memory.D[23] == 59 && memory.D[24] == 59,
          "Time comparisons and second conversion");
  Require(static_cast<uint16_t>(memory.D[40]) == 0x41e4 && memory.D[41] == 29 &&
              memory.D[42] == 23,
          "CRC official example and checksum");
  Require(
      memory.D[54] == -25108 && memory.D[70] == -120 && memory.D[71] == 16838,
      "Decimal ASCII, ZONE and FX random sequence");
  Require(executor.GetWordValue({model::DeviceKind::kD, 66}, true) == INT32_MIN,
          "Decimal ASCII signed 32-bit minimum");
  std::string csv;
  std::string error;
  plc::LadderProgram restored;
  plc::LadderProgram source;
  Require(model::MaterializeLadder(program, &source, &error) &&
              ExportTextGXCSV(source, "utilities", &csv, &error) &&
              model::ImportGXCSV(csv, &restored, &error),
          "Utility CSV round trip");
  Require(executor.LoadProgram(Statements({{"LD", {"M8000"}},
                                           {"MOV", {"K3", "D0"}},
                                           {"MOV", {"K0", "D1"}},
                                           {"MOV", {"K0", "D2"}},
                                           {"MOV", {"K10", "D3"}},
                                           {"MOV", {"K50", "D4"}},
                                           {"MOV", {"K30", "D5"}},
                                           {"MOV", {"K100", "D6"}},
                                           {"SCL", {"K7", "D0", "D10"}},
                                           {"MOV", {"K0", "D21"}},
                                           {"MOV", {"K10", "D22"}},
                                           {"MOV", {"K30", "D23"}},
                                           {"MOV", {"K0", "D24"}},
                                           {"MOV", {"K50", "D25"}},
                                           {"MOV", {"K100", "D26"}},
                                           {"MOV", {"K3", "D20"}},
                                           {"SCL2", {"K7", "D20", "D11"}},
                                           {"DEZCP", {"E1", "E3", "E0", "M10"}},
                                           {"DEZCP", {"E1", "E3", "E4", "M13"}},
                                           {"END", {}}})) &&
              executor.ExecuteScanCycle().success && memory.D[10] == 35 &&
              memory.D[11] == 35 && memory.M[10] && memory.M[15],
          "Scaling table layouts and float zone output order");
  executor.ResetMemory();
  Require(executor.LoadProgram(Statements({{"LD", {"X0"}},
                                           {"DUTY", {"K1", "K3", "M8330"}},
                                           {"HOUR", {"K1", "D100", "M20"}},
                                           {"END", {}}})),
          "Load duty and hour meter");
  Require(executor.ExecuteScanCycle().success && !memory.special_m[330],
          "DUTY waits for its command");
  executor.SetInput(0, true);
  executor.SetContinuousExecution(true, 1000);
  Require(executor.ExecuteScanCycle().success && memory.special_m[330] &&
              memory.D[101] == 1,
          "DUTY starts and HOUR accumulates seconds");
  executor.SetInput(0, false);
  for (int scan = 0; scan < 3; ++scan)
    Require(executor.ExecuteScanCycle().success && !memory.special_m[330],
            "DUTY keeps running through three off scans");
  Require(executor.ExecuteScanCycle().success && memory.special_m[330] &&
              memory.D[101] == 1,
          "DUTY repeats while HOUR retains inactive time");
}

void FxTimerAndCounterClasses() {
  plc::CompiledPLCExecutor executor;
  const auto program = Statements({{"LD", {"X0"}},
                                   {"OUT", {"T0", "K2"}},
                                   {"OUT", {"T200", "K2"}},
                                   {"OUT", {"T246", "K2"}},
                                   {"OUT", {"T511", "K2"}},
                                   {"OUT", {"C0", "K2"}},
                                   {"OUT", {"C200", "K-1"}},
                                   {"LD", {"M8000"}},
                                   {"MOV", {"T200", "D0"}},
                                   {"LD", {"X1"}},
                                   {"RST", {"T246"}},
                                   {"LD", {"X2"}},
                                   {"OUT", {"M8200"}},
                                   {"END", {}}});
  Require(executor.LoadProgram(program), "FX timer and counter classes");
  executor.SetContinuousExecution(true, 10);
  executor.SetInput(0, true);
  Require(executor.ExecuteScanCycle().success, "Timer activation scan");
  Require(
      executor.ExecuteScanCycle().success && !executor.GetDeviceState("T0") &&
          !executor.GetDeviceState("T200") && executor.GetDeviceState("T246") &&
          executor.GetDeviceState("T511") && executor.GetMemory().D[0] == 1,
      "100ms, 10ms, 1ms and present value in timer units");
  Require(
      executor.ExecuteScanCycle().success && executor.GetDeviceState("T200"),
      "10ms timer reaches two ticks");
  executor.SetInput(0, false);
  executor.SetInput(2, true);
  Require(executor.ExecuteScanCycle().success &&
              executor.GetDeviceState("T246") &&
              !executor.GetDeviceState("T511"),
          "Retentive and ordinary timer unpowered behavior");
  executor.SetInput(0, true);
  Require(executor.ExecuteScanCycle().success &&
              executor.GetCounterValue(0) == 2 &&
              executor.GetCounterValue(200) == 0,
          "32-bit counter direction relay counts down");
  executor.SetInput(0, false);
  executor.SetInput(1, true);
  Require(
      executor.ExecuteScanCycle().success && !executor.GetDeviceState("T246"),
      "RST clears retentive timer contact");
  executor.SetInput(0, true);
  Require(executor.ExecuteScanCycle().success &&
              executor.GetCounterValue(0) == 2 &&
              executor.GetCounterValue(200) == -1,
          "16-bit counter stops, 32-bit counter continues negative");
}

void FxRegisterBanksAndIndexing() {
  for (const std::string address : {"R32767", "V7", "Z7", "D0.F", "U7\\G32766",
                                    "D20Z0", "X0V7", "U0\\G0Z0"}) {
    const auto parsed = plc_emulator::programming::ParseDeviceAddress(address);
    Require(parsed && plc_emulator::programming::ParseDeviceAddress(
                          plc_emulator::programming::FormatDeviceAddress(
                              *parsed)) == parsed,
            "Register address round trip");
  }
  Require(!plc_emulator::programming::ParseDeviceAddress("Z0Z0") &&
              !plc_emulator::programming::ParseDeviceAddress("M8000Z0") &&
              !plc_emulator::programming::ParseDeviceAddress("D0.FZ0") &&
              !plc_emulator::programming::ParseDeviceAddress("U0Z0\\G0"),
          "Reject prohibited index forms");
  plc::CompiledPLCExecutor executor;
  const auto program = Statements({{"LD", {"M8000"}},
                                   {"DMOV", {"K5", "Z0"}},
                                   {"MOV", {"K-2", "V1"}},
                                   {"MOV", {"K123", "R20"}},
                                   {"MOV", {"K77", "R20Z0"}},
                                   {"BMOV", {"R20Z0", "U0\\G10Z0", "K1"}},
                                   {"MOV", {"U0\\G15", "D0"}},
                                   {"MOV", {"K30V1", "D1"}},
                                   {"DMOV", {"K70000", "R30"}},
                                   {"DMOV", {"R30", "Z2"}},
                                   {"DMOV", {"Z2", "D2"}},
                                   {"SET", {"D10.F"}},
                                   {"CMP", {"K1", "K2", "D11.F"}},
                                   {"LD", {"D10.F"}},
                                   {"OUT", {"Y0"}},
                                   {"END", {}}});
  Require(executor.LoadProgram(program) && executor.ExecuteScanCycle().success,
          "Execute register banks and indexed operands");
  const auto& memory = executor.GetMemory();
  Require(memory.R[20] == 123 && memory.R[25] == 77 && memory.D[0] == 77 &&
              memory.D[1] == 28 && memory.Z[2] == 4464 && memory.V[2] == 1 &&
              memory.D[2] == 4464 && memory.D[3] == 1 && memory.Y[0] &&
              static_cast<uint16_t>(memory.D[10]) == 0x8000 &&
              memory.D[12] == 2,
          "Bank isolation, Z/V pair and word-bit result crossing");
  auto jumps = Statements({{"LD", {"M8000"}},
                           {"MOV", {"K1", "Z0"}},
                           {"CJ", {"P0Z0"}},
                           {"MOV", {"K9", "D0"}},
                           {"P1", {}},
                           {"LD", {"M8000"}},
                           {"MOV", {"K8", "D0"}},
                           {"END", {}}});
  Require(executor.LoadProgram(jumps) && executor.ExecuteScanCycle().success &&
              executor.GetMemory().D[0] == 8,
          "Indexed CJ pointer");
  auto bad = Statements({{"LD", {"M8000"}},
                         {"MOV", {"K-1", "Z0"}},
                         {"MOV", {"D0Z0", "D1"}},
                         {"END", {}}});
  Require(executor.LoadProgram(bad) && !executor.ExecuteScanCycle().success,
          "Indexed address underflow fails safely");
}

void FxExtensionStorage() {
  plc::CompiledPLCExecutor executor;
  Require(executor.LoadProgram(Statements({{"LD", {"M8002"}},
                                           {"INITR", {"R0", "K1"}},
                                           {"MOV", {"K123", "R0"}},
                                           {"MOV", {"K456", "R1"}},
                                           {"DMOV", {"K70000", "Z0"}},
                                           {"ZPUSH", {"D100"}},
                                           {"DMOV", {"K-5", "Z0"}},
                                           {"ZPUSH", {"D100"}},
                                           {"DMOV", {"K0", "Z0"}},
                                           {"ZPOP", {"D100"}},
                                           {"DMOV", {"Z0", "D10"}},
                                           {"ZPOP", {"D100"}},
                                           {"DMOV", {"Z0", "D12"}},
                                           {"LD", {"X0"}},
                                           {"SAVER", {"R0", "K1024", "D20"}},
                                           {"END", {}}})),
          "Load index stack and extension save");
  executor.SetInput(0, true);
  Require(executor.ExecuteScanCycle().success &&
              executor.GetMemory().D[20] == 1024 &&
              !executor.GetMemory().special_m[29],
          "SAVER first half");
  Require(executor.ExecuteScanCycle().success &&
              executor.GetMemory().D[20] == 2048 &&
              executor.GetMemory().special_m[29],
          "SAVER second half complete");
  Require(
      executor.GetMemory().D[100] == 0 && executor.GetMemory().D[10] == -5 &&
          executor.GetMemory().D[11] == -1 &&
          executor.GetMemory().D[12] == 4464 && executor.GetMemory().D[13] == 1,
      "Nested index stack layout and restoration");
  Require(executor.LoadProgram(Statements({{"LD", {"M8000"}},
                                           {"FMOV", {"K0", "R0", "K2"}},
                                           {"LOADR", {"R0", "K2"}},
                                           {"INITER", {"R0", "K1"}},
                                           {"LOADR", {"R0", "K1"}},
                                           {"MOV", {"K99", "R0"}},
                                           {"RWER", {"R0", "K1"}},
                                           {"MOV", {"K0", "R0"}},
                                           {"LOADR", {"R0", "K1"}},
                                           {"END", {}}})) &&
              executor.ExecuteScanCycle().success &&
              executor.GetMemory().R[0] == 99 &&
              executor.GetMemory().R[1] == 456,
          "ER persists through program load, independent erase and rewrite");
  Require(
      executor.LoadProgram(
          Statements({{"LD", {"M8002"}},
                      {"INITR", {"R2048", "K1"}},
                      {"MOV", {"K10", "D0"}},
                      {"MOV", {"K20", "D1"}},
                      {"LD", {"M8000"}},
                      {"LOGR", {"D0", "K2", "R2048", "K1", "D2"}},
                      {"END", {}}})) &&
          executor.ExecuteScanCycle().success &&
          executor.ExecuteScanCycle().success &&
          executor.GetMemory().D[2] == 4 &&
          executor.GetMemory().R[2048] == 10 &&
          executor.GetMemory().R[2051] == 20 &&
          static_cast<uint16_t>(executor.GetMemory().R[2048 + 1926]) == 0xfff0,
      "LOGR data layout and flash position bitmap");
}

void FxDeviceComments() {
  plc::CompiledPLCExecutor executor;
  Require(executor.SetDeviceComment("D100", "Target Line A"),
          "Register PLC device comment");
  Require(executor.LoadProgram(Statements({{"LD", {"M8000"}},
                                           {"COMRD", {"D100", "R0"}},
                                           {"$MOV", {"R0", "D0"}},
                                           {"END", {}}})) &&
              executor.ExecuteScanCycle().success &&
              executor.GetMemory().D[0] == 0x6154 &&
              executor.GetMemory().D[7] == 0x2020 &&
              executor.GetMemory().D[8] == 0,
          "COMRD official ASCII layout and terminator");
}

void FxHighSpeedInputs() {
  plc::CompiledPLCExecutor executor;
  Require(
      executor.LoadProgram(Statements({{"LD", {"M8000"}},
                                       {"OUT", {"C235", "K100"}},
                                       {"DHSCS", {"K2", "C235", "Y0"}},
                                       {"DHSCR", {"K4", "C235", "Y0"}},
                                       {"DHSZ", {"K2", "K3", "C235", "M10"}},
                                       {"END", {}}})) &&
          executor.ExecuteScanCycle().success &&
          executor.GetCounterValue(235) == 0,
      "High-speed counter enabled without counting rung edges");
  executor.SetInput(0, true);
  Require(executor.GetCounterValue(235) == 1 && executor.GetMemory().M[10],
          "X0 high-speed count between scans");
  executor.SetInput(0, false);
  executor.SetInput(0, true);
  Require(executor.GetCounterValue(235) == 2 && executor.GetPhysicalOutput(0) &&
              executor.GetMemory().M[11],
          "Asynchronous high-speed set and zone");
  Require(executor.InjectCounterPulses(235, 2) &&
              executor.GetCounterValue(235) == 4 &&
              !executor.GetPhysicalOutput(0) && executor.GetMemory().M[12],
          "Asynchronous high-speed reset");
  executor.ResetMemory();
  Require(executor.LoadProgram(
              Statements({{"LD", {"M8002"}},
                          {"DMOV", {"K2", "D0"}},
                          {"MOV", {"H0005", "D2"}},
                          {"DMOV", {"K4", "D3"}},
                          {"MOV", {"H0002", "D5"}},
                          {"LD", {"M8000"}},
                          {"OUT", {"C235", "K100"}},
                          {"DHSCT", {"D0", "K2", "C235", "Y0", "K3"}},
                          {"END", {}}})) &&
              executor.ExecuteScanCycle().success &&
              executor.InjectCounterPulses(235, 2) &&
              executor.GetPhysicalOutput(0) && executor.GetPhysicalOutput(2) &&
              executor.GetMemory().D[8138] == 1,
          "HSCT first comparison pattern");
  Require(executor.InjectCounterPulses(235, 2) &&
              !executor.GetPhysicalOutput(0) && executor.GetPhysicalOutput(1) &&
              executor.GetMemory().D[8138] == 0 &&
              executor.GetMemory().special_m[138],
          "HSCT wraps table and completes");
  executor.ResetMemory();
  Require(
      executor.LoadProgram(Statements(
          {{"LD", {"M8000"}}, {"SPD", {"X2", "K20", "R0"}}, {"END", {}}})) &&
          executor.ExecuteScanCycle().success,
      "SPD sampling enabled");
  executor.SetContinuousExecution(true, 10);
  for (int pulse = 0; pulse < 3; ++pulse) {
    executor.SetInput(2, true);
    executor.SetInput(2, false);
  }
  Require(executor.ExecuteScanCycle().success &&
              executor.GetMemory().R[1] == 3 &&
              executor.GetMemory().R[2] == 10 &&
              executor.ExecuteScanCycle().success &&
              executor.GetMemory().R[0] == 3 && executor.GetMemory().R[1] == 0,
          "SPD current count, period and measured density");
  executor.ResetMemory();
  Require(executor.LoadProgram(Statements({{"LD", {"M8000"}},
                                           {"REFF", {"K0"}},
                                           {"LD", {"X4"}},
                                           {"OUT", {"Y0"}},
                                           {"END", {}}})) &&
              executor.SetPhysicalInput(4, true) &&
              executor.ExecuteScanCycle().success &&
              executor.GetPhysicalOutput(0),
          "REFF immediately refreshes pending physical input");
  executor.ResetMemory();
  executor.SetMatrixInput(8, 0, true);
  executor.SetMatrixInput(9, 3, true);
  Require(executor.LoadProgram(Statements({{"LD", {"M8000"}},
                                           {"MTR", {"X20", "Y10", "M30", "K2"}},
                                           {"END", {}}})) &&
              executor.ExecuteScanCycle().success,
          "MTR two-column start");
  executor.SetContinuousExecution(true, 20);
  Require(executor.ExecuteScanCycle().success && executor.GetMemory().M[30] &&
              executor.GetPhysicalOutput(9) &&
              executor.ExecuteScanCycle().success &&
              executor.GetMemory().M[43] && executor.GetMemory().special_m[29],
          "MTR time multiplexing and decimal result groups");
}

void FxOperatorPanels() {
  plc::CompiledPLCExecutor executor;
  Require(
      executor.LoadProgram(Statements(
          {{"LD", {"M8000"}}, {"TKY", {"M100", "D0", "M200"}}, {"END", {}}})),
      "TKY program");
  executor.SetMemory(103, true);
  Require(executor.ExecuteScanCycle().success &&
              executor.GetMemory().D[0] == 3 && executor.GetMemory().M[203] &&
              executor.GetMemory().M[210],
          "TKY key capture and sensing");
  Require(executor.ExecuteScanCycle().success && executor.GetMemory().D[0] == 3,
          "TKY held key does not repeat");
  executor.SetMemory(103, false);
  executor.ExecuteScanCycle();
  executor.SetMemory(107, true);
  Require(
      executor.ExecuteScanCycle().success && executor.GetMemory().D[0] == 37,
      "TKY decimal digit shift");
  executor.ResetMemory();
  Require(
      executor.LoadProgram(Statements({{"LD", {"M8000"}},
                                       {"SET", {"M8167"}},
                                       {"HKY", {"X20", "Y20", "D0", "M200"}},
                                       {"END", {}}})),
      "HKY program");
  executor.SetMatrixInput(18, 3, true);
  for (int scan = 0; scan < 8; ++scan)
    Require(executor.ExecuteScanCycle().success, "HKY multiplexing scan");
  Require(executor.GetMemory().D[0] == 11 && executor.GetMemory().M[201] &&
              executor.GetMemory().M[206] && executor.GetMemory().special_m[29],
          "HKY hex extension and function-key flags");
  executor.ResetMemory();
  Require(executor.LoadProgram(Statements({{"LD", {"M8000"}},
                                           {"DSW", {"X20", "Y20", "D0", "K2"}},
                                           {"END", {}}})),
          "DSW program");
  for (int column = 0; column < 4; ++column) {
    const uint8_t digits =
        static_cast<uint8_t>((column + 1) | ((8 - column) << 4));
    for (int bit = 0; bit < 8; ++bit)
      executor.SetMatrixInput(16 + column, bit, (digits >> bit) & 1);
  }
  Require(executor.ExecuteScanCycle().success, "DSW strobe starts");
  executor.SetContinuousExecution(true, 100);
  for (int scan = 0; scan < 4; ++scan)
    Require(executor.ExecuteScanCycle().success, "DSW sample column");
  Require(executor.GetMemory().D[0] == 4321 &&
              executor.GetMemory().D[1] == 5678 &&
              executor.GetMemory().special_m[29],
          "DSW two numeric switch sets");
  executor.ResetMemory();
  Require(
      executor.LoadProgram(Statements({{"LD", {"M8000"}},
                                       {"SEGL", {"K1234", "Y30", "K0"}},
                                       {"END", {}}})) &&
          executor.ExecuteScanCycle().success &&
          executor.GetPhysicalOutput(26) && !executor.GetPhysicalOutput(28) &&
          executor.ExecuteScanCycle().success && executor.GetPhysicalOutput(28),
      "SEGL BCD data and strobe phase");
  for (int scan = 0; scan < 10; ++scan)
    Require(executor.ExecuteScanCycle().success, "SEGL display cycle");
  Require(executor.GetMemory().special_m[29], "SEGL twelve-scan completion");
  executor.ResetMemory();
  Require(
      executor.LoadProgram(Statements({{"LD", {"M8000"}},
                                       {"ARWS", {"M100", "D0", "Y30", "K0"}},
                                       {"END", {}}})),
      "ARWS program");
  executor.SetMemory(100, true);
  Require(
      executor.ExecuteScanCycle().success && executor.GetMemory().D[0] == 1000,
      "ARWS increment thousands");
  executor.SetMemory(100, false);
  executor.SetMemory(102, true);
  executor.ExecuteScanCycle();
  executor.SetMemory(102, false);
  executor.SetMemory(100, true);
  Require(
      executor.ExecuteScanCycle().success && executor.GetMemory().D[0] == 1100,
      "ARWS lower digit and increment");
}

void FxDrumAndRotaryControl() {
  plc::CompiledPLCExecutor executor;
  Require(executor.LoadProgram(Statements({{"LD", {"M8002"}},
                                           {"MOV", {"K40", "D0"}},
                                           {"MOV", {"K140", "D1"}},
                                           {"MOV", {"K100", "D2"}},
                                           {"MOV", {"K200", "D3"}},
                                           {"MOV", {"K120", "C0"}},
                                           {"LD", {"X0"}},
                                           {"ABSD", {"D0", "C0", "M100", "K2"}},
                                           {"END", {}}})),
          "ABSD program");
  executor.SetInput(0, true);
  Require(executor.ExecuteScanCycle().success && executor.GetMemory().M[100] &&
              executor.GetMemory().M[101],
          "ABSD official overlapping drum intervals");
  executor.SetInput(0, false);
  Require(executor.ExecuteScanCycle().success && executor.GetMemory().M[100],
          "ABSD unpowered outputs retained");
  executor.ResetMemory();
  Require(executor.LoadProgram(Statements({{"LD", {"M8002"}},
                                           {"MOV", {"K2", "D0"}},
                                           {"MOV", {"K3", "D1"}},
                                           {"LD", {"X0"}},
                                           {"OUT", {"C0", "K9999"}},
                                           {"LD", {"M8000"}},
                                           {"INCD", {"D0", "C0", "M100", "K2"}},
                                           {"END", {}}})) &&
              executor.ExecuteScanCycle().success &&
              executor.GetMemory().M[100],
          "INCD first process output");
  for (int pulse = 0; pulse < 2; ++pulse) {
    executor.SetInput(0, true);
    Require(executor.ExecuteScanCycle().success, "INCD counter pulse");
    executor.SetInput(0, false);
    executor.ExecuteScanCycle();
  }
  Require(executor.GetMemory().M[101] && !executor.GetMemory().M[100] &&
              executor.GetCounterValue(0) == 0 &&
              executor.GetCounterValue(1) == 1,
          "INCD reset measured counter and advance process");
  for (int pulse = 0; pulse < 3; ++pulse) {
    executor.SetInput(0, true);
    Require(executor.ExecuteScanCycle().success, "INCD second process pulse");
    if (pulse == 2)
      Require(executor.GetMemory().special_m[29], "INCD cycle complete flag");
    executor.SetInput(0, false);
    executor.ExecuteScanCycle();
  }
  Require(executor.GetMemory().M[100] && executor.GetCounterValue(1) == 0 &&
              !executor.GetMemory().special_m[29],
          "INCD process wraps and completion pulses one scan");
  executor.ResetMemory();
  Require(
      executor.LoadProgram(Statements({{"LD", {"M8002"}},
                                       {"MOV", {"K0", "D0"}},
                                       {"MOV", {"K0", "D1"}},
                                       {"MOV", {"K4", "D2"}},
                                       {"LD", {"X0"}},
                                       {"ROTC", {"D0", "K10", "K2", "M100"}},
                                       {"END", {}}})),
      "ROTC program");
  executor.SetInput(0, true);
  Require(executor.ExecuteScanCycle().success && executor.GetMemory().M[103],
          "ROTC shortest forward high speed");
  for (int pulse = 0; pulse < 2; ++pulse) {
    executor.SetMemory(100, true);
    executor.ExecuteScanCycle();
    executor.SetMemory(100, false);
    executor.ExecuteScanCycle();
  }
  Require(executor.GetMemory().D[0] == 2 && executor.GetMemory().M[104] &&
              !executor.GetMemory().M[103],
          "ROTC approaching product enters low speed");
  executor.SetInput(0, false);
  Require(executor.ExecuteScanCycle().success && !executor.GetMemory().M[104],
          "ROTC unpowered drive outputs clear");
}

void FxPulseAndPositioning() {
  plc::CompiledPLCExecutor executor;
  executor.SetContinuousExecution(true, 10);
  Require(executor.LoadProgram(Statements(
              {{"LD", {"X7"}}, {"PLSY", {"K1000", "K25", "Y0"}}, {"END", {}}})),
          "PLSY program");
  executor.SetInput(7, true);
  Require(executor.ExecuteScanCycle().success &&
              executor.GetAxisState(0)->busy &&
              executor.GetMemory().D[8140] == 0,
          "PLSY excludes pre-activation elapsed time");
  for (int scan = 0; scan < 4; ++scan)
    Require(executor.ExecuteScanCycle().success, "PLSY pulse scan");
  Require(executor.GetAxisState(0)->generated_pulses == 25 &&
              executor.GetMemory().D[8140] == 25 &&
              executor.GetMemory().D[8340] == 0 &&
              executor.GetAxisState(0)->complete &&
              executor.GetMemory().special_m[29],
          "PLSY exact pulse counter differs from positioning register");
  executor.ExecuteScanCycle();
  Require(executor.GetMemory().D[8140] == 25, "PLSY held completion");
  executor.SetInput(7, false);
  executor.ExecuteScanCycle();
  executor.SetInput(7, true);
  executor.ExecuteScanCycle();
  for (int scan = 0; scan < 4; ++scan)
    executor.ExecuteScanCycle();
  Require(executor.GetMemory().D[8140] == 50, "PLSY accumulated pulse count");

  executor.ResetMemory();
  Require(executor.LoadProgram(Statements(
              {{"LD", {"X7"}}, {"PWM", {"K15", "K40", "Y2"}}, {"END", {}}})),
          "PWM program");
  executor.SetInput(7, true);
  Require(executor.ExecuteScanCycle().success && executor.GetPhysicalOutput(2),
          "PWM starts high");
  executor.ExecuteScanCycle();
  executor.ExecuteScanCycle();
  Require(!executor.GetPhysicalOutput(2), "PWM low interval");
  executor.SetInput(7, false);
  Require(
      executor.ExecuteScanCycle().success && !executor.GetAxisState(2)->busy,
      "PWM command off stops");

  executor.ResetMemory();
  Require(executor.LoadProgram(
              Statements({{"LD", {"X7"}},
                          {"PLSR", {"K1000", "K100", "K100", "Y1"}},
                          {"END", {}}})),
          "PLSR program");
  executor.SetInput(7, true);
  executor.ExecuteScanCycle();
  executor.ExecuteScanCycle();
  Require(executor.GetAxisState(1)->frequency > 0 &&
              executor.GetAxisState(1)->frequency < 1000,
          "PLSR acceleration");
  for (int scan = 0; scan < 50; ++scan)
    executor.ExecuteScanCycle();
  Require(executor.GetAxisState(1)->complete &&
              executor.GetAxisState(1)->generated_pulses == 100 &&
              executor.GetMemory().D[8142] == 100,
          "PLSR exact finite quantity");

  executor.ResetMemory();
  Require(executor.LoadProgram(
              Statements({{"LD", {"X7"}},
                          {"DDRVI", {"K-120", "K2000", "Y0", "Y4"}},
                          {"END", {}}})),
          "DRVI program");
  executor.SetInput(7, true);
  Require(executor.ExecuteScanCycle().success && !executor.GetOutput(4),
          "DRVI reverse direction");
  for (int scan = 0; scan < 30; ++scan)
    executor.ExecuteScanCycle();
  Require(executor.GetAxisState(0)->complete &&
              executor.GetAxisState(0)->position == -120 &&
              executor.GetMemory().D[8140] == 0,
          "DRVI signed relative position");
  Require(
      executor.LoadProgram(Statements({{"LD", {"X7"}},
                                       {"DDRVA", {"K40", "K2000", "Y0", "Y4"}},
                                       {"END", {}}})),
      "DRVA program");
  executor.ExecuteScanCycle();
  for (int scan = 0; scan < 30; ++scan)
    executor.ExecuteScanCycle();
  Require(executor.GetAxisState(0)->position == 40 &&
              executor.GetAxisState(0)->generated_pulses == 160 &&
              executor.GetOutput(4),
          "DRVA retained current position");

  executor.ResetMemory();
  Require(
      executor.LoadProgram(Statements(
          {{"LD", {"X7"}}, {"DPLSV", {"K-1000", "Y0", "Y4"}}, {"END", {}}})),
      "PLSV program");
  executor.SetInput(7, true);
  executor.ExecuteScanCycle();
  executor.ExecuteScanCycle();
  Require(executor.GetAxisState(0)->position <= -10,
          "PLSV signed continuous output");
  executor.SetDeviceState("M8349", true);
  Require(executor.ExecuteScanCycle().success &&
              executor.GetAxisState(0)->abnormal &&
              !executor.GetAxisState(0)->busy &&
              executor.GetDeviceState("M8329"),
          "Immediate positioning stop");

  executor.ResetMemory();
  Require(
      executor.LoadProgram(Statements({{"LD", {"X7"}},
                                       {"ZRN", {"K2000", "K100", "X2", "Y0"}},
                                       {"END", {}}})),
      "ZRN program");
  executor.SetInput(2, false);
  executor.SetInput(7, true);
  executor.ExecuteScanCycle();
  executor.ExecuteScanCycle();
  executor.SetPhysicalInput(2, true);
  executor.ExecuteScanCycle();
  executor.SetPhysicalInput(2, false);
  Require(executor.GetAxisState(0)->position == 0 &&
              executor.GetAxisState(0)->complete,
          "ZRN DOG rear edge origin");

  executor.ResetMemory();
  Require(
      executor.LoadProgram(Statements(
          {{"LD", {"X7"}}, {"DSZR", {"X2", "X3", "Y0", "Y4"}}, {"END", {}}})),
      "DSZR program");
  executor.SetInput(2, true);
  executor.SetInput(3, false);
  executor.SetInput(7, true);
  Require(executor.ExecuteScanCycle().success && executor.GetOutput(4),
          "DSZR escapes initial DOG opposite home direction");
  executor.SetPhysicalInput(2, false);
  Require(!executor.GetOutput(4), "DSZR reverses after DOG escape");
  executor.SetPhysicalInput(2, true);
  executor.SetPhysicalInput(2, false);
  Require(executor.GetAxisState(0)->busy, "DSZR waits for zero phase");
  executor.SetPhysicalInput(3, true);
  Require(executor.GetAxisState(0)->complete &&
              executor.GetAxisState(0)->position == 0,
          "DSZR zero-phase completion");

  executor.ResetMemory();
  Require(
      executor.LoadProgram(Statements({{"LD", {"M8002"}},
                                       {"MOV", {"H50", "D8336"}},
                                       {"SET", {"M8336"}},
                                       {"LD", {"X7"}},
                                       {"DDVIT", {"K25", "K1000", "Y1", "Y5"}},
                                       {"END", {}}})),
      "DVIT program");
  executor.SetInput(5, false);
  executor.SetInput(7, true);
  executor.ExecuteScanCycle();
  executor.ExecuteScanCycle();
  const int position = executor.GetAxisState(1)->position;
  executor.SetPhysicalInput(5, true);
  for (int scan = 0; scan < 20; ++scan)
    executor.ExecuteScanCycle();
  Require(
      executor.GetAxisState(1)->complete &&
          executor.GetAxisState(1)->position == position + 25,
      "DVIT axis nibble selects X5 and finishes exact post-interrupt distance");
  Require(!executor.GetAxisState(4).has_value(), "Reject invalid axis");
}

void FxPositionTablesAndAbsoluteEncoder() {
  namespace model = plc_emulator::programming;
  plc::CompiledPLCExecutor executor;
  executor.SetContinuousExecution(true, 10);
  const std::array<model::DeviceAddress, 4> directions = {
      {{model::DeviceKind::kY, 8},
       {model::DeviceKind::kY, 9},
       {model::DeviceKind::kY, 10},
       {model::DeviceKind::kY, 11}}};
  Require(executor.ConfigurePositioningTable({model::DeviceKind::kR, 0},
                                             directions) &&
              executor.SetPositioningEntry(
                  2, 3, model::Opcode::kRelativePosition, 125, 2000),
          "Position table PLC parameters");
  Require(
      executor.GetMemory().R[808] == 125 && executor.GetMemory().R[810] == 2000,
      "GX position table axis and entry word layout");
  Require(executor.LoadProgram(Statements(
              {{"LD", {"X7"}}, {"DTBL", {"Y2", "K3"}}, {"END", {}}})),
          "DTBL program");
  executor.SetInput(7, true);
  Require(executor.ExecuteScanCycle().success, "DTBL activation");
  for (int scan = 0; scan < 30; ++scan)
    Require(executor.ExecuteScanCycle().success, "DTBL positioning scan");
  Require(executor.GetAxisState(2)->position == 125 &&
              executor.GetAxisState(2)->complete && executor.GetOutput(10),
          "DTBL executes configured relative positioning");
  executor.SetInput(7, false);
  executor.ExecuteScanCycle();
  Require(executor.LoadProgram(Statements({{"LD", {"M8002"}},
                                           {"DMOV", {"K-50", "R808"}},
                                           {"LD", {"X7"}},
                                           {"DTBL", {"Y2", "K3"}},
                                           {"END", {}}})),
          "DTBL writable data");
  executor.ExecuteScanCycle();
  executor.SetInput(7, true);
  executor.ExecuteScanCycle();
  for (int scan = 0; scan < 30; ++scan)
    executor.ExecuteScanCycle();
  Require(
      executor.GetAxisState(2)->position == 75,
      "DTBL reads pulse data changed by ladder/HMI without editing parameters");
  executor.ResetMemory();
  Require(executor.LoadProgram(Statements(
              {{"LD", {"X7"}}, {"DABS", {"X0", "Y20", "D8340"}}, {"END", {}}})),
          "DABS program");
  Require(executor.SetAbsoluteEncoder(0, -10), "Configure absolute servo");
  executor.SetInput(7, true);
  Require(executor.ExecuteScanCycle().success && executor.GetOutput(16) &&
              executor.GetOutput(17) && !executor.GetOutput(18),
          "DABS starts SON and transfer mode before requesting data");
  for (int scan = 0; scan < 40; ++scan)
    Require(executor.ExecuteScanCycle().success, "DABS handshake scan");
  Require(
      executor.GetAxisState(0)->position == -10 &&
          executor.GetDeviceState("M8029") && executor.GetOutput(16) &&
          !executor.GetOutput(17) && !executor.GetOutput(18),
      "DABS reads signed 32-bit value plus six-bit checksum and retains SON");
  executor.SetInput(7, false);
  Require(executor.ExecuteScanCycle().success && !executor.GetOutput(16),
          "DABS command off clears servo ON");
}

void FxPidControl() {
  plc::CompiledPLCExecutor executor;
  executor.SetContinuousExecution(true, 100);
  Require(
      executor.LoadProgram(Statements({{"LD", {"M8002"}},
                                       {"MOV", {"K100", "D0"}},
                                       {"MOV", {"K100", "D100"}},
                                       {"MOV", {"K35", "D101"}},
                                       {"MOV", {"K0", "D102"}},
                                       {"MOV", {"K100", "D103"}},
                                       {"MOV", {"K10", "D104"}},
                                       {"MOV", {"K0", "D105"}},
                                       {"MOV", {"K0", "D106"}},
                                       {"MOV", {"K5", "D120"}},
                                       {"MOV", {"K5", "D121"}},
                                       {"MOV", {"K35", "D122"}},
                                       {"MOV", {"K-20", "D123"}},
                                       {"LD", {"M0"}},
                                       {"MOV", {"K20", "D1"}},
                                       {"LD", {"X7"}},
                                       {"PID", {"D0", "D1", "D100", "D150"}},
                                       {"END", {}}})),
      "PID program");
  executor.SetInput(7, true);
  Require(
      executor.ExecuteScanCycle().success && executor.GetMemory().D[150] == 0,
      "PID preserves initial MV and initializes prior samples");
  for (int scan = 0; scan < 2; ++scan)
    executor.ExecuteScanCycle();
  Require(executor.GetMemory().D[150] == 20,
          "PID reverse action integrates SV-PV using 100ms time units");
  for (int scan = 0; scan < 3; ++scan)
    executor.ExecuteScanCycle();
  Require(executor.GetMemory().D[150] == 35, "PID output upper limit");
  executor.SetMemory(0, true);
  Require(executor.ExecuteScanCycle().success &&
              executor.GetMemory().D[150] == 23 &&
              (executor.GetMemory().D[124] & 1),
          "PID incremental P plus I and input variation alarm");
  executor.SetInput(7, false);
  executor.ExecuteScanCycle();
  Require(executor.GetMemory().D[150] == 23, "PID off retains MV");

  executor.ResetMemory();
  executor.SetContinuousExecution(true, 1000);
  Require(
      executor.LoadProgram(Statements({{"LD", {"M8002"}},
                                       {"MOV", {"K300", "D0"}},
                                       {"MOV", {"K1000", "D100"}},
                                       {"MOV", {"K16", "D101"}},
                                       {"MOV", {"K100", "D150"}},
                                       {"LD", {"M0"}},
                                       {"MOV", {"K60", "D1"}},
                                       {"LD", {"M1"}},
                                       {"MOV", {"K120", "D1"}},
                                       {"LD", {"X7"}},
                                       {"PID", {"D0", "D1", "D100", "D150"}},
                                       {"END", {}}})),
      "PID step tuning program");
  executor.SetInput(7, true);
  executor.ExecuteScanCycle();
  executor.ExecuteScanCycle();
  executor.ExecuteScanCycle();
  executor.SetMemory(0, true);
  executor.ExecuteScanCycle();
  Require(
      executor.GetMemory().D[150] == 100 && (executor.GetMemory().D[101] & 16),
      "PID step tuning holds MV before one-third response");
  executor.SetMemory(1, true);
  executor.ExecuteScanCycle();
  Require(executor.GetMemory().D[103] == 100 &&
              executor.GetMemory().D[104] == 40 &&
              executor.GetMemory().D[106] == 100 &&
              executor.GetMemory().D[101] == 1,
          "PID step tuning tangent gain, Ti, Td and detected reverse action");

  executor.ResetMemory();
  executor.SetContinuousExecution(true, 100);
  Require(
      executor.LoadProgram(Statements({{"LD", {"M8002"}},
                                       {"MOV", {"K100", "D100"}},
                                       {"MOV", {"K81", "D101"}},
                                       {"MOV", {"K5", "D125"}},
                                       {"MOV", {"K100", "D126"}},
                                       {"MOV", {"K0", "D127"}},
                                       {"MOV", {"K-50", "D128"}},
                                       {"MOV", {"K-10", "D1"}},
                                       {"LD", {"M0"}},
                                       {"MOV", {"K10", "D1"}},
                                       {"LDI", {"M0"}},
                                       {"MOV", {"K-10", "D1"}},
                                       {"LD", {"X7"}},
                                       {"PID", {"D0", "D1", "D100", "D150"}},
                                       {"END", {}}})),
      "PID limit cycle program");
  executor.SetInput(7, true);
  executor.ExecuteScanCycle();
  Require(executor.GetMemory().D[150] == 100, "PID limit tuning high level");
  for (int cycle = 0; cycle < 4; ++cycle) {
    executor.SetMemory(0, true);
    executor.ExecuteScanCycle();
    executor.ExecuteScanCycle();
    executor.SetMemory(0, false);
    executor.ExecuteScanCycle();
    executor.ExecuteScanCycle();
  }
  Require(
      !(executor.GetMemory().D[101] & 80) && executor.GetMemory().D[103] > 0 &&
          executor.GetMemory().D[104] > 0 && executor.GetMemory().D[106] > 0,
      "PID limit-cycle hysteresis switches levels and completes tuning");
}

void FxPositioningValidationAndCsv() {
  namespace model = plc_emulator::programming;
  model::ExecutionProgram invalid;
  std::string error;
  Require(!model::CompileStatements(
              {{"LD", {"M8000"}}, {"PLSY", {"K100", "K10", "Y2"}}, {"END", {}}},
              &invalid, &error),
          "F4 rejects unsupported PLSY output");
  Require(!model::CompileStatements(
              {{"LD", {"M8000"}}, {"DTBL", {"Y0", "K101"}}, {"END", {}}},
              &invalid, &error),
          "F4 rejects positioning table beyond 100");
  const auto program =
      Statements({{"LD", {"M8000"}}, {"DTBL", {"Y0", "K1"}}, {"END", {}}});
  plc::LadderProgram ladder;
  std::string csv;
  Require(model::MaterializeLadder(program, &ladder, &error) &&
              ExportTextGXCSV(ladder, "positioning", &csv, &error),
          error);
  Require(csv.find("\"18\"\t\"\"\t\"END\"") != std::string::npos,
          "GX DTBL occupies 17 steps despite its two visible operands");
  plc::LadderProgram imported;
  Require(model::ImportGXCSV(csv, &imported, &error) &&
              Compile(imported).instructions.front().opcode ==
                  model::Opcode::kPositionTable &&
              model::SerializeInstructions(Compile(imported)).size() == 3,
          "GX positioning CSV instruction roundtrip");

  plc::CompiledPLCExecutor executor;
  executor.SetContinuousExecution(true, 10);
  Require(
      executor.LoadProgram(Statements({{"LD", {"M8002"}},
                                       {"SET", {"M8341"}},
                                       {"LD", {"X7"}},
                                       {"ZRN", {"K2000", "K100", "X2", "Y0"}},
                                       {"END", {}}})),
      "ZRN CLEAR program");
  executor.SetInput(7, true);
  executor.SetInput(2, false);
  executor.ExecuteScanCycle();
  executor.ExecuteScanCycle();
  executor.SetPhysicalInput(2, true);
  executor.SetPhysicalInput(2, false);
  Require(executor.GetPhysicalOutput(4), "ZRN raises default CLEAR signal");
  executor.ExecuteScanCycle();
  executor.ExecuteScanCycle();
  Require(executor.GetPhysicalOutput(4), "ZRN CLEAR adds one scan to 20ms");
  executor.ExecuteScanCycle();
  Require(!executor.GetPhysicalOutput(4),
          "ZRN CLEAR ends after 20ms plus one scan");
  executor.ResetMemory();
  Require(executor.LoadProgram(Statements({{"LD", {"M8000"}},
                                           {"MOV", {"K5", "V0"}},
                                           {"FLT", {"K1", "D0V0"}},
                                           {"DEMOV", {"D0", "D10"}},
                                           {"INT", {"D10", "D20"}},
                                           {"END", {}}})),
          "Float V index program");
  Require(
      executor.ExecuteScanCycle().success && executor.GetMemory().D[20] == 1,
      "32-bit float operands ignore a V modifier per FX rules");
  executor.ResetMemory();
  Require(executor.LoadProgram(Statements({{"LD", {"M8000"}},
                                           {"MOV", {"K1", "V0"}},
                                           {"MOV", {"K0", "Z0"}},
                                           {"FLT", {"K1", "D0Z0"}},
                                           {"END", {}}})),
          "Float Z pair program");
  Require(!executor.ExecuteScanCycle().success,
          "32-bit float Z modifier uses its V high word and rejects "
          "out-of-range index");
}

void FxParallelPrint() {
  plc::CompiledPLCExecutor executor;
  Require(executor.LoadProgram(Statements({{"LD", {"M8002"}},
                                           {"$MOV", {"\"AB\"", "D0"}},
                                           {"SET", {"M8027"}},
                                           {"LD", {"X7"}},
                                           {"PR", {"D0", "Y20"}},
                                           {"END", {}}})),
          "PR program");
  executor.SetInput(7, true);
  Require(executor.ExecuteScanCycle().success &&
              executor.GetPhysicalOutput(16) &&
              executor.GetPhysicalOutput(22) &&
              !executor.GetPhysicalOutput(24) && executor.GetPhysicalOutput(25),
          "PR first ASCII byte and execution flag");
  executor.ExecuteScanCycle();
  Require(executor.GetPhysicalOutput(24), "PR one-scan strobe");
  executor.ExecuteScanCycle();
  Require(!executor.GetPhysicalOutput(16) && executor.GetPhysicalOutput(17) &&
              executor.GetPhysicalOutput(22) && !executor.GetPhysicalOutput(24),
          "PR second ASCII byte after strobe");
  executor.ExecuteScanCycle();
  executor.ExecuteScanCycle();
  Require(executor.GetDeviceState("M8029") && !executor.GetPhysicalOutput(25),
          "PR null termination completes and clears execution flag");
  executor.ExecuteScanCycle();
  Require(!executor.GetDeviceState("M8029"), "PR null completion pulses once");
  executor.SetInput(7, false);
  executor.ExecuteScanCycle();
  for (int bit = 16; bit < 26; ++bit)
    Require(!executor.GetPhysicalOutput(bit), "PR off clears all ten outputs");
}

void FxVirtualSerialPorts() {
  plc::CompiledPLCExecutor executor;
  executor.SetContinuousExecution(true, 10);
  Require(executor.LoadProgram(Statements({{"LD", {"M8002"}},
                                           {"MOV", {"H381", "D8120"}},
                                           {"MOV", {"H02", "D8124"}},
                                           {"MOV", {"H03", "D8125"}},
                                           {"$MOV", {"\"AB\"", "D0"}},
                                           {"SET", {"M8122"}},
                                           {"LD", {"M8000"}},
                                           {"RS", {"D0", "K2", "D10", "K2"}},
                                           {"END", {}}})),
          "RS channel 1 program");
  Require(
      executor.ExecuteScanCycle().success && executor.GetDeviceState("M8122"),
      "RS send request armed");
  const std::array<uint8_t, 4> input = {2, 0x34, 0x56, 3};
  Require(executor.InjectSerialReceived(1, input), "RS inject complete frame");
  Require(executor.ExecuteScanCycle().success &&
              !executor.GetDeviceState("M8122") &&
              executor.GetDeviceState("M8123") &&
              executor.GetMemory().D[10] == 0x5634 &&
              executor.GetMemory().D[8123] == 2,
          "RS receives packed bytes and completes");
  std::array<uint8_t, 16> transmitted{};
  const size_t sent = executor.ReadSerialTransmitted(1, transmitted);
  Require(sent == 4 && transmitted[0] == 2 && transmitted[1] == 'A' &&
              transmitted[2] == 'B' && transmitted[3] == 3,
          "RS transmits configured header, payload and terminator");
  Require(executor.ReadSerialTransmitted(1, transmitted) == 0,
          "RS TX ring drains");
  Require(executor.InjectSerialReceived(1, input), "RS queue second frame");
  executor.ExecuteScanCycle();
  Require(executor.GetMemory().D[8123] == 2,
          "RS receive completion blocks next frame");
  executor.SetDeviceState("M8123", false);
  executor.ExecuteScanCycle();
  Require(executor.GetDeviceState("M8123"),
          "RS receives again after flag clears");

  executor.ResetMemory();
  Require(
      executor.LoadProgram(Statements({{"LD", {"M8002"}},
                                       {"MOV", {"H2381", "D8420"}},
                                       {"MOV", {"H02", "D8430"}},
                                       {"MOV", {"H03", "D8432"}},
                                       {"$MOV", {"\"AB\"", "D0"}},
                                       {"SET", {"M8422"}},
                                       {"LD", {"M8000"}},
                                       {"RS2", {"D0", "K2", "D20", "K2", "K2"}},
                                       {"END", {}}})),
      "RS2 channel 2 program");
  Require(executor.ExecuteScanCycle().success, "RS2 starts");
  const std::array<uint8_t, 6> checked = {2, 0x34, 0x56, 3, '8', 'D'};
  Require(executor.InjectSerialReceived(2, checked), "RS2 checksum input");
  Require(executor.ExecuteScanCycle().success &&
              executor.GetDeviceState("M8423") &&
              executor.GetMemory().D[20] == 0x5634 &&
              executor.GetMemory().D[8434] == 0x8D &&
              executor.GetMemory().D[8435] == 0x8D,
          "RS2 channel 2 validates ASCII sum after terminator");
  const size_t sent2 = executor.ReadSerialTransmitted(2, transmitted);
  Require(sent2 == 6 && transmitted[0] == 2 && transmitted[1] == 'A' &&
              transmitted[2] == 'B' && transmitted[3] == 3 &&
              transmitted[4] == '8' && transmitted[5] == '6',
          "RS2 send sum includes data and terminator");
}

void FxInitialState() {
  plc::CompiledPLCExecutor executor;
  Require(
      executor.LoadProgram(Statements(
          {{"LD", {"M8000"}}, {"IST", {"X20", "S20", "S40"}}, {"END", {}}})),
      "IST program");
  executor.SetInput(16, true);
  Require(
      executor.ExecuteScanCycle().success && executor.GetDeviceState("S0") &&
          executor.GetDeviceState("M8040") && executor.GetDeviceState("M8047"),
      "IST individual mode initializes S0 and disables transfer");
  executor.SetInput(16, false);
  executor.SetInput(18, true);
  executor.SetDeviceState("M8043", true);
  executor.SetInput(22, true);
  Require(
      executor.ExecuteScanCycle().success && executor.GetDeviceState("S2") &&
          executor.GetDeviceState("M8041") &&
          executor.GetDeviceState("M8042") && !executor.GetDeviceState("M8040"),
      "IST stepping start pulses and enables transfer");
  executor.ExecuteScanCycle();
  Require(!executor.GetDeviceState("M8042") && executor.GetDeviceState("M8040"),
          "IST start pulse lasts one scan");
}

void FxStepLadder() {
  plc::CompiledPLCExecutor executor;
  const auto program = Statements(
      {{"LD", {"M8002"}}, {"SET", {"S0"}},         {"SET", {"M8047"}},
       {"STL", {"S0"}},   {"OUT", {"Y0"}},         {"LD", {"X0"}},
       {"SET", {"S20"}},  {"LD", {"S0"}},          {"OUT", {"M10"}},
       {"STL", {"S20"}},  {"OUT", {"Y1"}},         {"OUT", {"Y2"}},
       {"LD", {"X1"}},    {"SET", {"S21"}},        {"STL", {"S21"}},
       {"OUT", {"Y2"}},   {"SET", {"Y3"}},         {"RET", {}},
       {"LD", {"M8000"}}, {"MOV", {"K7", "D100"}}, {"END", {}}});
  Require(executor.LoadProgram(program), "STL/RET program");
  Require(executor.ExecuteScanCycle().success && executor.GetOutput(0) &&
              !executor.GetOutput(1) && executor.GetMemory().D[100] == 7,
          "STL direct outputs and RET resumes ordinary ladder");
  executor.SetInput(0, true);
  Require(executor.ExecuteScanCycle().success && executor.GetOutput(0) &&
              executor.GetOutput(1) && !executor.GetMemory(10),
          "STL outgoing and incoming outputs overlap for one scan; source "
          "contact turns off immediately");
  Require(executor.ExecuteScanCycle().success && !executor.GetOutput(0) &&
              executor.GetOutput(1) && executor.GetOutput(2),
          "STL outgoing state receives one off execution");
  executor.SetDeviceState("M8040", true);
  executor.SetInput(1, true);
  executor.ExecuteScanCycle();
  Require(!executor.GetDeviceState("S21"), "M8040 blocks STL transition");
  executor.SetDeviceState("M8040", false);
  executor.ExecuteScanCycle();
  executor.ExecuteScanCycle();
  Require(
      !executor.GetOutput(1) && executor.GetOutput(2) && executor.GetOutput(3),
      "Different STL states OR the same OUT coil");
  executor.SetDeviceState("S21", false);
  executor.ExecuteScanCycle();
  Require(!executor.GetOutput(2) && executor.GetOutput(3),
          "STL OUT clears at exit and SET output remains latched");

  model::ExecutionProgram invalid;
  std::string error;
  Require(!model::CompileStatements({{"STL", {"S0"}},
                                     {"OUT", {"Y0"}},
                                     {"STL", {"S0"}},
                                     {"RET", {}},
                                     {"END", {}}},
                                    &invalid, &error),
          "Duplicate STL state is rejected");
  Require(
      !model::CompileStatements({{"STL", {"S0"}}, {"OUT", {"Y0"}}, {"END", {}}},
                                &invalid, &error),
      "Missing RET is rejected");
  Require(executor.LoadProgram(Statements({{"STL", {"S20"}},
                                           {"STL", {"S21"}},
                                           {"OUT", {"Y4"}},
                                           {"LD", {"X4"}},
                                           {"SET", {"S22"}},
                                           {"RET", {}},
                                           {"END", {}}})),
          "STL parallel recombination program");
  executor.SetDeviceState("S20", true);
  executor.SetDeviceState("S21", false);
  executor.ExecuteScanCycle();
  Require(!executor.GetOutput(4),
          "STL recombination waits for all source states");
  executor.SetDeviceState("S21", true);
  executor.SetInput(4, true);
  executor.ExecuteScanCycle();
  Require(executor.GetOutput(4) && executor.GetDeviceState("S22"),
          "STL recombination transfers once all states and condition are ON");
  executor.ExecuteScanCycle();
  Require(!executor.GetDeviceState("S20") && !executor.GetDeviceState("S21") &&
              !executor.GetOutput(4),
          "STL recombination resets both source states and their output");
}

void FxVirtualInverters() {
  plc::CompiledPLCExecutor executor;
  executor.SetContinuousExecution(true, 10);
  Require(executor.ConfigureInverter(1, 3), "Configure virtual inverter");
  Require(executor.SetInverterParameter(1, 3, 1201, 45),
          "Set virtual inverter extended parameter");
  const auto run = [&](model::InstructionStatement instruction, int scans = 2) {
    Require(executor.LoadProgram(
                Statements({{"LD", {"M8000"}}, instruction, {"END", {}}})),
            "Inverter command program");
    for (int scan = 0; scan < scans; ++scan)
      Require(executor.ExecuteScanCycle().success, "Inverter command scan");
  };
  run({"IVRD", {"K3", "K1201", "D0", "K1"}});
  Require(executor.GetMemory().D[0] == 45 && executor.GetDeviceState("M8029") &&
              !executor.GetDeviceState("M8151"),
          "IVRD reads virtual extended parameter and completes");
  run({"IVWR", {"K3", "K1201", "K55", "K1"}});
  Require(executor.GetInverterParameter(1, 3, 1201) == 55,
          "IVWR writes virtual parameter");
  run({"IVDR", {"K3", "HED", "K1500", "K1"}});
  run({"IVCK", {"K3", "H6D", "D1", "K1"}});
  Require(executor.GetMemory().D[1] == 1500,
          "IVDR frequency setting is visible through IVCK");
  Require(executor.LoadProgram(Statements({{"LD", {"M8002"}},
                                           {"MOV", {"K7", "D10"}},
                                           {"MOV", {"K81", "D11"}},
                                           {"MOV", {"K8", "D12"}},
                                           {"MOV", {"K82", "D13"}},
                                           {"LD", {"M8000"}},
                                           {"IVBWR", {"K3", "K2", "D10", "K1"}},
                                           {"END", {}}})),
          "IVBWR program");
  for (int scan = 0; scan < 3; ++scan)
    Require(executor.ExecuteScanCycle().success, "IVBWR scan");
  Require(executor.GetInverterParameter(1, 3, 7) == 81 &&
              executor.GetInverterParameter(1, 3, 8) == 82,
          "IVBWR writes every parameter-value table pair");
  Require(executor.SetInverterMonitor(1, 3, 0x6F, 1200),
          "Set virtual inverter output frequency");
  run({"IVMC", {"K3", "H10", "D10", "D20", "K1"}});
  Require(executor.GetMemory().D[20] == 7 && executor.GetMemory().D[21] == 1200,
          "IVMC applies two commands and reads two monitor values");
  executor.ResetMemory();
  Require(executor.ConfigureInverter(2, 4, false),
          "Configure disconnected inverter");
  run({"IVCK", {"K4", "H70", "D30", "K2"}}, 11);
  Require(executor.GetDeviceState("M8157") &&
              executor.GetMemory().D[8157] == 1 &&
              executor.GetDeviceState("M8438"),
          "Missing inverter response times out and latches channel 2 error");
}

void FxVirtualModbus() {
  using Space = plc::CompiledPLCExecutor::ModbusSpace;
  plc::CompiledPLCExecutor executor;
  executor.SetContinuousExecution(true, 10);
  Require(executor.ConfigureModbusSlave(1, 1) &&
              executor.ConfigureModbusSlave(1, 2),
          "Configure virtual MODBUS slaves");
  const auto run = [&](model::InstructionStatement command, int scans = 2) {
    Require(executor.LoadProgram(Statements({{"LD", {"M8411"}},
                                             {"MOV", {"H1", "D8401"}},
                                             {"MOV", {"K20", "D8409"}},
                                             {"MOV", {"K1", "D8412"}},
                                             {"LD", {"M8000"}},
                                             command,
                                             {"END", {}}})),
            "ADPRW program");
    for (int scan = 0; scan < scans; ++scan)
      Require(executor.ExecuteScanCycle().success, "ADPRW scan");
  };
  Require(executor.SetModbusValue(1, 1, Space::kHoldingRegister, 100, 234),
          "Set remote holding register");
  run({"ADPRW", {"K1", "H3", "K100", "K2", "D0"}});
  Require(executor.GetMemory().D[0] == 234 &&
              executor.GetDeviceState("M8029") &&
              !executor.GetDeviceState("M8401"),
          "ADPRW reads holding registers and completes");
  run({"ADPRW", {"K1", "H6", "K100", "K0", "H1234"}});
  run({"ADPRW", {"K1", "H16", "K100", "HFFF0", "H5"}});
  Require(executor.GetModbusValue(1, 1, Space::kHoldingRegister, 100) == 0x1235,
          "ADPRW single and mask register writes");
  run({"ADPRW", {"K0", "H10", "K110", "K2", "K99"}});
  Require(executor.GetModbusValue(1, 1, Space::kHoldingRegister, 110) == 99 &&
              executor.GetModbusValue(1, 2, Space::kHoldingRegister, 111) == 99,
          "ADPRW broadcast writes to each connected slave");
  run({"ADPRW", {"K1", "H5", "K0", "K0", "K1"}});
  run({"ADPRW", {"K1", "H1", "K0", "K17", "M100"}});
  Require(executor.GetMemory(100) && !executor.GetMemory(101),
          "ADPRW reads coils into individual PLC bit devices");
  run({"ADPRW", {"K1", "HF", "K20", "K17", "H5555"}});
  run({"ADPRW", {"K1", "H1", "K20", "K17", "D10"}});
  Require(
      executor.GetMemory().D[10] == 0x5555 && executor.GetMemory().D[11] == 1,
      "ADPRW multiple coils pack into words and repeat constant patterns");
  Require(executor.SetModbusValue(1, 1, Space::kDiscreteInput, 5, 1) &&
              executor.SetModbusValue(1, 1, Space::kInputRegister, 5, 678),
          "Set remote read-only input images");
  run({"ADPRW", {"K1", "H2", "K5", "K1", "D12"}});
  run({"ADPRW", {"K1", "H4", "K5", "K1", "D13"}});
  Require(executor.GetMemory().D[12] == 1 && executor.GetMemory().D[13] == 678,
          "ADPRW discrete and register input reads");
  Require(executor.LoadProgram(Statements({{"LD", {"M8000"}},
                                           {"MOV", {"K120", "D30"}},
                                           {"MOV", {"K120", "D31"}},
                                           {"MOV", {"K2", "D40"}},
                                           {"MOV", {"K2", "D41"}},
                                           {"MOV", {"K321", "D50"}},
                                           {"MOV", {"K654", "D51"}},
                                           {"END", {}}})),
          "Initialize read/write MODBUS table");
  executor.ExecuteScanCycle();
  run({"ADPRW", {"K1", "H17", "D30", "D40", "D50"}});
  Require(
      executor.GetMemory().D[52] == 321 && executor.GetMemory().D[53] == 654,
      "ADPRW combined register transaction writes before reading");
  run({"ADPRW", {"K1", "H8", "K0", "K345", "D60"}});
  Require(executor.GetMemory().D[60] == 345,
          "ADPRW diagnostic query echoes data");
  run({"ADPRW", {"K1", "HB", "K0", "K0", "D70"}});
  run({"ADPRW", {"K1", "HC", "K0", "K0", "D80"}});
  Require(executor.GetMemory().D[71] > 0 && executor.GetMemory().D[83] > 0,
          "ADPRW communication counters and event log");
  run({"ADPRW", {"K1", "H11", "K0", "K0", "D120"}});
  Require(
      executor.GetMemory().D[120] == 1 && executor.GetMemory().D[121] == 255,
      "ADPRW slave ID and running status");
  run({"ADPRW", {"K1", "H7", "K0", "K0", "D122"}});
  Require(executor.GetMemory().D[122] == 0, "ADPRW exception status");
  run({"ADPRW", {"K1", "H3", "K0", "K2", "D7999"}});
  Require(executor.GetMemory().D[8402] == 218,
          "ADPRW rejects PLC destination block overflow");
  Require(executor.ConfigureModbusSlave(1, 1, false),
          "Disconnect MODBUS slave");
  run({"ADPRW", {"K1", "H3", "K0", "K1", "D0"}}, 5);
  Require(
      executor.GetMemory().D[8402] == 211 && executor.GetDeviceState("M8408") &&
          executor.GetDeviceState("M8409") && executor.GetDeviceState("M8403"),
      "ADPRW retries and then latches response timeout");
}

void FxCfCards() {
  plc::CompiledPLCExecutor executor;
  Require(executor.ConfigureCfCard(1, 1048576, 4096),
          "Configure virtual CF card");
  const auto run = [&](std::vector<model::InstructionStatement> commands,
                       std::vector<model::InstructionStatement> setup = {},
                       int scans = 2) {
    std::vector<model::InstructionStatement> statements;
    if (!setup.empty())
      statements.push_back({"LD", {"M8002"}});
    statements.insert(statements.end(), setup.begin(), setup.end());
    statements.push_back({"LD", {"M8000"}});
    statements.insert(statements.end(), commands.begin(), commands.end());
    statements.push_back({"END", {}});
    Require(executor.LoadProgram(Statements(statements)), "CF command program");
    for (int scan = 0; scan < scans; ++scan)
      Require(executor.ExecuteScanCycle().success, "CF command scan");
  };
  const auto parameters = [](int head, std::initializer_list<int> values) {
    std::vector<model::InstructionStatement> setup;
    for (const int value : values)
      setup.push_back(
          {"MOV", {"K" + std::to_string(value), "D" + std::to_string(head++)}});
    return setup;
  };
  run({{"FLCRT", {"K1", "\"DATA\"", "D0", "K1"}}}, parameters(0, {0, 2, 2, 1}));
  Require(executor.GetDeviceState("M8029") && executor.GetDeviceState("M8405"),
          "FLCRT creates file on mounted CF card");
  auto setup = parameters(30, {2, -1, 1, 3, 0});
  const auto values = parameters(10, {123, 456, -7});
  setup.insert(setup.end(), values.begin(), values.end());
  run({{"FLWR", {"K1", "D10", "D30", "D20", "K1"}}}, setup);
  Require(executor.GetMemory().D[20] == 2 && executor.GetMemory().D[21] == 1 &&
              executor.ReadCfCsv(1, "DATA.CSV")
                      .value_or("")
                      .find("+00123,+00456,-00007") != std::string::npos,
          "FLWR writes fixed-width signed values and advances row/column");
  run({{"FLRD", {"K1", "D40", "D100", "D110", "K1"}}},
      parameters(40, {2, 1, 1, 5}));
  Require(executor.GetMemory().D[100] == 123 &&
              executor.GetMemory().D[102] == -7 &&
              executor.GetMemory().D[110] == 3,
          "FLRD reads available data and reports total row points");
  run({{"FLWR", {"K1", "D10", "D30", "D20", "K1"}}});
  Require(executor.GetMemory().D[20] == 1,
          "CF ring file returns to first row at maximum");
  run({{"FLSTRD", {"K1", "D200", "K1"}}});
  Require(executor.GetMemory().D[200] == 2 && executor.GetMemory().D[201] == 3,
          "FLSTRD reports last row and column");
  run({{"FLCRT", {"K2", "\"BUF\"", "D0", "K1"}}}, parameters(0, {0, 7, 4, 0}));
  setup = parameters(30, {7, -1, 1, 1, 1});
  setup.push_back({"$MOV", {"\"AB\"", "D50"}});
  run({{"FLWR", {"K2", "D50", "D30", "D20", "K1"}}}, setup);
  Require(executor.ReadCfCsv(1, "BUF").value_or("missing").empty(),
          "CF internal buffer is separate from persisted file");
  run({{"FLCMD", {"K2", "K1"}}});
  Require(
      executor.ReadCfCsv(1, "BUF").value_or("").find("AB") != std::string::npos,
      "FLCMD flushes buffered file data");
  run({{"FLWR", {"K2", "D50", "D30", "D20", "K1"}}});
  Require(executor.PowerCycleCfCard(1), "Power cycle virtual CF adapter");
  run({{"FLRD", {"K2", "D40", "D100", "D110", "K1"}}},
      parameters(40, {7, 2, 1, 1}));
  Require(
      executor.GetDeviceState("M8329") && executor.GetMemory().D[8418] == 806,
      "CF power interruption loses unflushed rows");
  run({{"FLDEL", {"K2", "K1", "K1"}}});
  Require(executor.ReadCfCsv(1, "BUF").has_value(),
          "FLDEL association-only mode preserves file");
  run({{"FLCRT", {"K3", "\"BUF\"", "D0", "K1"}}}, parameters(0, {0, 7, 4, 0}));
  run({{"FLSTRD", {"H100", "D200", "K1"}}});
  Require(executor.GetMemory().D[200] == 10,
          "FLCRT reassociates existing file; FLSTRD reports file ID bits");
  run({{"FLCMD", {"H200", "K1"}}});
  run({{"FLWR", {"K3", "D50", "D30", "D20", "K1"}}});
  Require(
      executor.GetMemory().D[8418] == 704 && executor.GetMemory().D[8414] == 1,
      "CF unmounted access fails and reports GX instruction step");
  run({{"FLCMD", {"H100", "K1"}}});
  run({{"FLSTRD", {"H500", "D210", "K1"}}});
  Require(executor.GetMemory().D[210] == 704,
          "CF status exposes recent error history");
  run({{"FLCMD", {"H500", "K1"}}});
  Require(!executor.GetDeviceState("M8418"), "FLCMD clears adapter errors");

  run({{"FLCRT", {"K4", "\"MIX\"", "D0", "K1"}}}, parameters(0, {0, 0, 4, 0}));
  setup = parameters(3000, {0, -1, 1, 3, 0});
  for (const auto& block :
       {parameters(3010, {1, -1, 1, 1, 0}), parameters(3020, {3, -1, 1, 1, 0}),
        parameters(3030, {7, -1, 1, 1, 0})})
    setup.insert(setup.end(), block.begin(), block.end());
  setup.push_back({"SET", {"M100"}});
  setup.push_back({"DMOV", {"K123456", "D50"}});
  setup.push_back({"$MOV", {"\"HELLO\"", "D60"}});
  run({{"FLWR", {"K4", "D50", "D3000", "D2000", "K1"}},
       {"FLWR", {"K4", "M100", "D3010", "D2000", "K1"}},
       {"FLWR", {"K4", "D50", "D3020", "D2000", "K1"}},
       {"FLWR", {"K4", "D60", "D3030", "D2000", "K1"}}},
      setup, 5);
  Require(executor.ReadCfCsv(1, "MIX").value_or("").find(
              "1,+0000123456,HELLO") != std::string::npos,
          "FLWR mixed sequence commits one row with three data types");
  setup = parameters(3000, {0, 1, 1, 3});
  for (const auto& block :
       {parameters(3010, {1, 1, 1, 1}), parameters(3020, {3, 1, 1, 1}),
        parameters(3030, {7, 1, 1, 1})})
    setup.insert(setup.end(), block.begin(), block.end());
  run({{"FLRD", {"K4", "D3000", "D5000", "D4000", "K1"}},
       {"FLRD", {"K4", "D3010", "M101", "D4000", "K1"}},
       {"FLRD", {"K4", "D3020", "D5000", "D4000", "K1"}},
       {"FLRD", {"K4", "D3030", "D5010", "D4000", "K1"}}},
      setup, 5);
  Require(executor.GetMemory(101) &&
              executor.GetWordValue(*model::ParseDeviceAddress("D5000"),
                                    true) == 123456 &&
              executor.GetMemory().D[5010] == 0x4548 &&
              executor.GetMemory().D[4000] == 3,
          "FLRD mixed sequence restores bit, double word and string");
  run({{"FLCRT", {"K0", "D50", "D0", "K1"}}}, parameters(0, {0, 2, 1, 90}));
  run({{"FLWR", {"K0", "D10", "D30", "D20", "K1"}}},
      parameters(30, {2, -1, 1, 1, 0}));
  Require(executor.ReadCfCsv(1, "FILE0000").has_value() &&
              executor.ReadCfCsv(1, "FILE0001").has_value() &&
              executor.GetMemory().D[20] == 1,
          "CF FIFO rotates to a new numbered file at maximum row");
  run({{"FLCRT", {"K5", "\"FINAL\"", "D0", "K1"}}},
      parameters(0, {0, 7, 2, 0}));
  setup = parameters(30, {7, -1, 1, 1, 1});
  setup.push_back({"$MOV", {"\"LONG\"", "D50"}});
  run({{"FLWR", {"K5", "D50", "D30", "D20", "K1"}}}, setup);
  Require(executor.ReadCfCsv(1, "FINAL").value_or("missing").empty(),
          "CF first row remains buffered");
  run({{"FLWR", {"K5", "D50", "D30", "D20", "K1"}}});
  Require(executor.ReadCfCsv(1, "FINAL").value_or("").find("2,,LONG") !=
              std::string::npos,
          "CF final row flushes all buffered rows of the file");
  setup = parameters(30, {7, 1, 1, 1, 1});
  setup.push_back({"$MOV", {"\"Z\"", "D50"}});
  run({{"FLWR", {"K5", "D50", "D30", "D20", "K1"}}}, setup);
  Require(executor.ReadCfCsv(1, "FINAL").value_or("").find("1,,Z   ") !=
              std::string::npos,
          "CF shorter overwrite preserves the original fixed field width");
  run({{"FLWR", {"K5", "D50", "D30", "D20", "K1"}}},
      parameters(30, {7, 1, 2, 1, 0}));
  Require(executor.GetMemory().D[8418] == 815,
          "CF overwrite cannot append fields to an existing row");
  for (const int type : {4, 5, 6}) {
    const std::string name = "TYPE" + std::to_string(type);
    run({{"FLCRT",
          {"K" + std::to_string(type + 2), "\"" + name + "\"", "D0", "K1"}}},
        parameters(0, {1, type, 2, 0}));
    setup = parameters(30, {type, -1, 1, 1, 0});
    setup.push_back(
        type == 6 ? model::InstructionStatement{"DEMOV", {"E1.25", "D50"}}
                  : model::InstructionStatement{"DMOV", {"H1234ABCD", "D50"}});
    run({{"FLWR", {"K" + std::to_string(type + 2), "D50", "D30", "D20", "K1"}}},
        setup);
    const std::string csv = executor.ReadCfCsv(1, name).value_or("");
    Require(csv.find(type == 4   ? "ABCD"
                     : type == 5 ? "1234ABCD"
                                 : "+1.2500000E+00") != std::string::npos &&
                csv.find('/') != std::string::npos,
            "CF hexadecimal/float fields and timestamp export");
    run({{"FLRD",
          {"K" + std::to_string(type + 2), "D40", "D100", "D110", "K1"}}},
        parameters(40, {type, 1, 1, 1}));
    Require(
        executor.GetWordValue(*model::ParseDeviceAddress("D100"), type != 4) ==
            executor.GetWordValue(*model::ParseDeviceAddress("D50"), type != 4),
        "CF numeric fields preserve their exact bit representation");
  }
  run({{"FLDEL", {"H200", "H100", "K1"}}});
  Require(!executor.ReadCfCsv(1, "DATA").has_value(),
          "FLDEL FAT16 format clears files");
}

void FxExternalInstructionsCsv() {
  const auto program = Statements({{"LD", {"M8000"}},
                                   {"IST", {"X20", "S20", "S40"}},
                                   {"RS", {"D0", "K1", "D10", "K1"}},
                                   {"RS2", {"D0", "K1", "D10", "K1", "K1"}},
                                   {"IVCK", {"K1", "H6D", "D0", "K1"}},
                                   {"IVDR", {"K1", "HED", "K100", "K1"}},
                                   {"IVRD", {"K1", "K7", "D0", "K1"}},
                                   {"IVWR", {"K1", "K7", "K100", "K1"}},
                                   {"IVBWR", {"K1", "K2", "D0", "K1"}},
                                   {"IVMC", {"K1", "K0", "D0", "D10", "K1"}},
                                   {"ADPRW", {"K1", "H03", "K0", "K1", "D0"}},
                                   {"FLCRT", {"K1", "\"DATA\"", "D0", "K1"}},
                                   {"FLDEL", {"K1", "K0", "K1"}},
                                   {"FLWR", {"K1", "D10", "D20", "D30", "K1"}},
                                   {"FLRD", {"K1", "D20", "D10", "D30", "K1"}},
                                   {"FLCMD", {"K1", "K1"}},
                                   {"FLSTRD", {"K1", "D0", "K1"}},
                                   {"STL", {"S0"}},
                                   {"OUT", {"Y0"}},
                                   {"RET", {}},
                                   {"END", {}}});
  plc::LadderProgram ladder;
  plc::LadderProgram imported;
  std::string csv;
  std::string error;
  Require(model::MaterializeLadder(program, &ladder, &error) &&
              ExportTextGXCSV(ladder, "FX external", &csv, &error) &&
              model::ImportGXCSV(csv, &imported, &error),
          error);
  const auto before = model::SerializeInstructions(program);
  const auto after = model::SerializeInstructions(Compile(imported));
  Require(before.size() == after.size(), "FX external CSV statement count");
  for (size_t index = 0; index < before.size(); ++index)
    Require(before[index].mnemonic == after[index].mnemonic &&
                before[index].operands == after[index].operands,
            "FX external CSV preserves instruction and operand sequence");
}

void GXNativeCsvEncoding() {
  for (const std::string literal : {"\"A\"", "\"AB\"", "\"ABC\"",
                                    "\"ABCDE\""}) {
    const int extra_steps = literal.size() <= 4 ? 0 :
                            literal.size() == 5 ? 2 : 4;
    Require(model::GXInstructionStepCount(
                {"$MOV", {literal, "D100"}}) == 5 + extra_steps &&
                model::GXInstructionStepCount(
                    {"FLCRT", {"K1", literal, "D200", "K1"}}) ==
                    9 + extra_steps,
            "GX native literal lengths use paired instruction steps");
  }
  Require(model::GXInstructionStepCount({"SET", {"S0"}}) == 2 &&
              model::GXInstructionStepCount({"SET", {"M0"}}) == 1,
          "GX native SET state relay consumes two steps");
  Require(model::GXInstructionStepCount(
              {"FLCRT", {"K1", "\"DATA\"", "D0", "K1"}}) == 11 &&
              model::GXInstructionStepCount(
                  {"FLCRT", {"K1", "D50", "D0", "K1"}}) == 9,
          "GX native FLCRT literal includes embedded string words");
  const plc::LadderProgram empty = Program();
  std::string csv;
  std::string error;
  Require(model::ExportGXCSV(empty, "Untitled Project", &csv, &error), error);
  // Header and encoding captured with GX Works2 on 2026-10-01.
  const std::string native_text =
      "\"(Untitled Project)\"\r\n"
      "\"PLC Information:\"\t\"FXCPU FX3U/FX3UC\"\r\n"
      "\"Step No.\"\t\"Line Statement\"\t\"Instruction\"\t"
      "\"I/O(Device)\"\t\"Blank\"\t\"PI Statement\"\t\"Note\"\r\n"
      "\"0\"\t\"\"\t\"LD\"\t\"X0\"\t\"\"\t\"\"\t\"\"\r\n"
      "\"1\"\t\"\"\t\"OUT\"\t\"Y0\"\t\"\"\t\"\"\t\"\"\r\n"
      "\"2\"\t\"\"\t\"END\"\t\"\"\t\"\"\t\"\"\t\"\"\r\n";
  std::string expected = "\xFF\xFE";
  for (const char character : native_text) {
    expected += character;
    expected += '\0';
  }
  Require(csv == expected,
          "Default CSV uses GX Works2 native encoding and framing");
  plc::LadderProgram imported;
  Require(model::ImportGXCSV(csv, &imported, &error), error);
  const std::string unicode = "\xED\x95\x9C\xEA\xB8\x80\xF0\x9F\x98\x80";
  const auto program = Statements({{"LD", {"M8000"}},
                                   {"$MOV", {"\"" + unicode + "\"", "D0"}},
                                   {"END", {}}});
  plc::LadderProgram ladder;
  Require(model::MaterializeLadder(program, &ladder, &error) &&
              model::ExportGXCSV(ladder, unicode, &csv, &error) &&
              model::ImportGXCSV(csv, &imported, &error),
          error);
  Require(model::SerializeInstructions(Compile(imported))[1].operands[0] ==
              "\"" + unicode + "\"",
          "UTF-16 preserves Korean and surrogate-pair string operands");
  Require(!model::ExportGXCSV(ladder, std::string("\xF0\x80\x80\x80", 4), &csv,
                              &error),
          "CSV rejects overlong UTF-8");
  Require(!model::ImportGXCSV(std::string("\xFF\xFE\x00\xD8", 4), &imported,
                              &error),
          "CSV rejects truncated UTF-16 surrogate pairs");
}

}  // namespace

namespace {

void RuntimeProcessScenario() {
  plc::CompiledPLCExecutor executor;
  Require(executor.LoadProgram(Statements(
              {{"LD", {"M8002"}}, {"MOV", {"K0", "D0"}},
               {"LDP", {"X0"}}, {"RST", {"C0"}},
               {"LD", {"X0"}}, {"OR", {"M0"}}, {"ANI", {"X2"}},
               {"OUT", {"M0"}}, {"LD", {"M0"}},
               {"OUT", {"T0", "K2"}}, {"LD", {"M0"}},
               {"AND", {"T0"}}, {"AND", {"X1"}},
               {"OUT", {"C0", "K3"}}, {"INCP", {"D0"}},
               {"LD", {"C0"}}, {"OR", {"X2"}}, {"RST", {"M0"}},
               {"LD", {"M0"}}, {"AND", {"T0"}}, {"OUT", {"Y0"}},
               {"LD", {"C0"}}, {"OUT", {"Y1"}}, {"END", {}}})),
          "Load production start-delay-count-stop sequence");
  executor.SetContinuousExecution(true, 100);
  int scans = 0;
  const auto scan = [&executor, &scans]() {
    const auto result = executor.ExecuteScanCycle();
    Require(result.success, "Process scan: " + result.errorMessage);
    ++scans;
  };
  scan();
  Require(!executor.GetOutput(0) && executor.GetMemory().D[0] == 0,
          "Initial output is off and cumulative count is zero");
  for (int batch = 0; batch < 50; ++batch) {
    executor.SetInput(0, true);
    scan();
    Require(!executor.GetOutput(0), "Start waits for timer");
    executor.SetInput(0, false);
    scan();
    Require(!executor.GetOutput(0), "Delay does not complete early");
    scan();
    Require(executor.GetOutput(0), "Self-hold survives start release");
    for (int product = 0; product < 3; ++product) {
      executor.SetInput(1, true);
      scan();
      for (int held = 0; held < 4; ++held)
        scan();
      Require(executor.GetCounterValue(0) == product + 1 &&
                  executor.GetMemory().D[0] == batch * 3 + product + 1,
              "Held sensor counts only its rising edge");
      executor.SetInput(1, false);
      scan();
    }
    Require(!executor.GetOutput(0) && executor.GetOutput(1),
            "Third product stops motor in the same scan");
    scan();
    Require(executor.GetTimerValue(0) == 0,
            "Stopped process clears non-retentive timer");
  }
  executor.SetInput(0, true);
  scan();
  executor.SetInput(0, false);
  scan();
  scan();
  Require(executor.GetOutput(0), "Restart after completed batches");
  executor.SetInput(2, true);
  scan();
  Require(!executor.GetOutput(0) && !executor.GetMemory(0),
          "Stop input clears self-hold and motor immediately");
  executor.SetInput(2, false);
  for (int idle = 0; idle < 10; ++idle)
    scan();
  Require(!executor.GetOutput(0) && executor.GetMemory().D[0] == 150,
          "Releasing stop does not restart or change cumulative count");
  std::cout << "PASS production sequence: " << scans
            << " scans, 50 batches, 150 products, stop/restart\n";
}

void RuntimeArithmeticScenario() {
  plc::CompiledPLCExecutor executor;
  Require(executor.LoadProgram(Statements(
              {{"LD", {"M8002"}}, {"MOV", {"K0", "D0"}},
               {"DMOV", {"K100000", "D2"}}, {"LD", {"X0"}},
               {"ADDP", {"D0", "K1", "D0"}},
               {"DADDP", {"D2", "K200000", "D2"}},
               {"LD", {"M8000"}}, {"DEMOV", {"E1.5", "D10"}},
               {"DEMOV", {"E2.25", "D12"}},
               {"DEADD", {"D10", "D12", "D14"}},
               {"INT", {"D14", "D16"}},
               {"DECMP", {"D10", "D12", "M10"}},
               {"$MOV", {"\"AB\"", "D100"}}, {"END", {}}})),
          "Load arithmetic/float/string execution sequence");
  Require(executor.ExecuteScanCycle().success, "Initialize arithmetic");
  for (int pulse = 0; pulse < 20; ++pulse) {
    executor.SetInput(0, true);
    for (int held = 0; held < 5; ++held)
      Require(executor.ExecuteScanCycle().success, "Arithmetic held scan");
    Require(executor.GetMemory().D[0] == pulse + 1 &&
                executor.GetWordValue({model::DeviceKind::kD, 2}, true) ==
                    100000 + (pulse + 1) * 200000,
            "16/32-bit pulse arithmetic executes once per edge");
    Require(executor.GetMemory().D[16] == 3 && !executor.GetMemory(10) &&
                !executor.GetMemory(11) && executor.GetMemory(12) &&
                executor.GetMemory().D[100] == 0x4241 &&
                executor.GetMemory().D[101] == 0,
            "Float addition/conversion/comparison and string memory result");
    executor.SetInput(0, false);
    Require(executor.ExecuteScanCycle().success, "Arithmetic pulse release");
  }
  std::cout << "PASS arithmetic: 121 scans, 20 edges, D0=20, D2=4100000, "
               "float INT=3, string AB\n";
}

}  // namespace

void RunPhysicalRuntimeTests();

int main(int argc, char** argv) {
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
  try {
    if (argc == 2 && std::string(argv[1]) == "--physical-only") {
      RunPhysicalRuntimeTests();
      return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--runtime-only") {
      RuntimeProcessScenario();
      RuntimeArithmeticScenario();
      TimerCounterAndReset();
      std::cout << "PASS timer/counter activation, hold, reset\n";
      FxPulseAndPositioning();
      std::cout << "PASS virtual pulse/axis movement and completion\n";
      FxVirtualInverters();
      FxVirtualModbus();
      FxCfCards();
      std::cout << "PASS virtual inverter/MODBUS/CF execution\n";
      return 0;
    }
    std::cout << "Devices\n" << std::flush;
    Devices();
    std::cout << "SeriesAndTransactionalLoad\n" << std::flush;
    SeriesAndTransactionalLoad();
    std::cout << "Parallel\n" << std::flush;
    Parallel();
    std::cout << "TimerCounterAndReset\n" << std::flush;
    TimerCounterAndReset();
    std::cout << "InvalidTopology\n" << std::flush;
    InvalidTopology();
    std::cout << "CSVAndNestedBranches\n" << std::flush;
    CSVAndNestedBranches();
    std::cout << "ArithmeticAndPulse\n" << std::flush;
    ArithmeticAndPulse();
    SavedBranchAndCacheInvalidation();
    WordOperationsAndLD();
    GXWrappingSymbols();
    ScanObservations();
    TierThreeDataInstructions();
    FxCpuInstructions();
    FxProgramFlow();
    FxFloatingPoint();
    FxStrings();
    FxTables();
    FxBasicControl();
    FxPackedBitsAndWideContacts();
    FxTimeAndConversion();
    FxTimerAndCounterClasses();
    FxVirtualModules();
    FxHandyInstructions();
    FxSort2();
    FxRegisterBanksAndIndexing();
    FxExtensionStorage();
    FxDeviceComments();
    FxHighSpeedInputs();
    FxOperatorPanels();
    FxDrumAndRotaryControl();
    FxPulseAndPositioning();
    FxPositionTablesAndAbsoluteEncoder();
    FxPidControl();
    FxPositioningValidationAndCsv();
    FxParallelPrint();
    FxVirtualSerialPorts();
    FxInitialState();
    FxStepLadder();
    FxVirtualInverters();
    FxVirtualModbus();
    FxCfCards();
    FxExternalInstructionsCsv();
    GXNativeCsvEncoding();
    std::cout << "Ladder rewrite tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
