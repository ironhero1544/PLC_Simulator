#include "plc_emulator/project/gx_csv_codec.h"

#include "plc_emulator/project/instruction_codec.h"
#include "plc_emulator/project/structured_ladder_compiler.h"

#include <charconv>
#include <sstream>
#include <stdexcept>

namespace plc_emulator::programming {
namespace {
std::string Quote(const std::string& field) {
  std::string result = "\"";
  for (const char ch : field) {
    if (ch == '"')
      result += '"';
    result += ch;
  }
  return result + '"';
}

std::vector<std::string> Split(const std::string& line) {
  const char delimiter = line.find('\t') == std::string::npos ? ',' : '\t';
  std::vector<std::string> fields(1);
  bool quoted = false;
  for (size_t i = 0; i < line.size(); ++i) {
    const char ch = line[i];
    if (ch == '"') {
      if (quoted && i + 1 < line.size() && line[i + 1] == '"') {
        fields.back() += ch;
        ++i;
      } else
        quoted = !quoted;
    } else if (!quoted && ch == delimiter)
      fields.emplace_back();
    else if (ch != '\r')
      fields.back() += ch;
  }
  if (quoted)
    throw std::runtime_error("Unclosed CSV quote");
  return fields;
}

std::string EncodeUtf16LE(std::string_view text) {
  std::string result = "\xFF\xFE";
  const auto append = [&result](uint32_t unit) {
    result += static_cast<char>(unit & 255);
    result += static_cast<char>(unit >> 8);
  };
  for (size_t index = 0; index < text.size();) {
    const auto first = static_cast<unsigned char>(text[index++]);
    uint32_t code = first;
    int continuation = 0;
    uint32_t minimum = 0;
    if (first >= 0xC2 && first <= 0xDF) {
      code = first & 31;
      continuation = 1;
      minimum = 0x80;
    } else if (first >= 0xE0 && first <= 0xEF) {
      code = first & 15;
      continuation = 2;
      minimum = 0x800;
    } else if (first >= 0xF0 && first <= 0xF4) {
      code = first & 7;
      continuation = 3;
      minimum = 0x10000;
    } else if (first >= 0x80) {
      throw std::runtime_error("Invalid UTF-8 CSV text");
    }
    for (int byte = 0; byte < continuation; ++byte) {
      if (index == text.size())
        throw std::runtime_error("Truncated UTF-8 CSV text");
      const auto next = static_cast<unsigned char>(text[index++]);
      if ((next & 0xC0) != 0x80)
        throw std::runtime_error("Invalid UTF-8 CSV continuation");
      code = (code << 6) | (next & 63);
    }
    if (code < minimum || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF))
      throw std::runtime_error("Invalid Unicode CSV character");
    if (code > 0xFFFF) {
      code -= 0x10000;
      append(0xD800 | (code >> 10));
      append(0xDC00 | (code & 1023));
    } else {
      append(code);
    }
  }
  return result;
}

std::string Decode(const std::string& content) {
  if (content.size() >= 3 && content.substr(0, 3) == "\xEF\xBB\xBF") {
    return content.substr(3);
  }
  if (content.size() < 2 || content.substr(0, 2) != "\xFF\xFE")
    return content;
  if (content.size() % 2 != 0)
    throw std::runtime_error("Truncated UTF-16 CSV");
  std::string result;
  for (size_t i = 2; i + 1 < content.size(); i += 2) {
    uint32_t code =
        static_cast<unsigned char>(content[i]) |
        (static_cast<uint32_t>(static_cast<unsigned char>(content[i + 1]))
         << 8);
    if (code >= 0xD800 && code <= 0xDBFF) {
      if (i + 3 >= content.size())
        throw std::runtime_error("Truncated UTF-16 surrogate pair");
      const uint32_t low =
          static_cast<unsigned char>(content[i + 2]) |
          (static_cast<uint32_t>(static_cast<unsigned char>(content[i + 3]))
           << 8);
      if (low < 0xDC00 || low > 0xDFFF)
        throw std::runtime_error("Invalid UTF-16 surrogate pair");
      code = 0x10000 + ((code - 0xD800) << 10) + low - 0xDC00;
      i += 2;
    } else if (code >= 0xDC00 && code <= 0xDFFF) {
      throw std::runtime_error("Unpaired UTF-16 low surrogate");
    }
    if (code < 0x80)
      result += static_cast<char>(code);
    else if (code < 0x800) {
      result += static_cast<char>(0xC0 | code >> 6);
      result += static_cast<char>(0x80 | (code & 0x3F));
    } else if (code < 0x10000) {
      result += static_cast<char>(0xE0 | code >> 12);
      result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
      result += static_cast<char>(0x80 | (code & 0x3F));
    } else {
      result += static_cast<char>(0xF0 | code >> 18);
      result += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
      result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
      result += static_cast<char>(0x80 | (code & 0x3F));
    }
  }
  return result;
}

int StepCount(const InstructionStatement& statement) {
  if (statement.mnemonic == "MC")
    return 3;
  if (statement.mnemonic == "MCR")
    return 2;
  const auto pointer = ParseOperand(statement.mnemonic);
  if (pointer && (pointer->kind == OperandKind::kPointer ||
                  pointer->kind == OperandKind::kInterruptPointer))
    return 1;
  if (statement.mnemonic == "OUT" && statement.operands.size() > 1)
    return 3;
  if (statement.mnemonic == "SET" && !statement.operands.empty() &&
      statement.operands.front().starts_with("S"))
    return 2;
  std::string comparison = statement.mnemonic;
  const bool wide_comparison = comparison.rfind("DLD", 0) == 0 ||
                               comparison.rfind("DAND", 0) == 0 ||
                               comparison.rfind("DOR", 0) == 0;
  if (wide_comparison)
    comparison.erase(comparison.begin());
  if (statement.operands.size() == 2 &&
      (comparison.rfind("LD", 0) == 0 || comparison.rfind("AND", 0) == 0 ||
       comparison.rfind("OR", 0) == 0))
    return wide_comparison ? 9 : 5;
  std::string name = statement.mnemonic;
  const bool base_instruction =
      FindInstruction(name) || (name.size() > 1 && name.front() == 'D' &&
                                FindInstruction(name.substr(1)));
  if (!base_instruction && name.size() > 1 && name.back() == 'P')
    name.pop_back();
  const InstructionDef* definition = FindInstruction(name);
  bool wide = false;
  if (!definition && name.size() > 1 && name.front() == 'D') {
    wide = true;
    definition = FindInstruction(name.substr(1));
  }
  if (definition &&
      (definition->fnc_no != 0 || definition->opcode == Opcode::kJump)) {
    if (definition->opcode == Opcode::kPositionTable)
      return 17;
    const size_t literal_index = name == "FLCRT" ? 1 : 0;
    if ((name == "FLCRT" || name == "$MOV") &&
        statement.operands.size() > literal_index) {
      const std::string& literal = statement.operands[literal_index];
      if (literal.size() > 2 && literal.front() == '"' &&
          literal.back() == '"') {
        const int extra_steps =
            2 * static_cast<int>((literal.size() - 3) / 2);
        return 1 + 2 * definition->operand_count + extra_steps;
      }
    }
    return 1 + (wide ? 4 : 2) * definition->operand_count;
  }
  return 1;
}
}  // namespace

int GXInstructionStepCount(const InstructionStatement& statement) {
  return StepCount(statement);
}

bool ImportGXCSV(const std::string& content, plc::LadderProgram* ladder,
                 std::string* error) {
  if (!ladder)
    return false;
  try {
    std::istringstream lines(Decode(content));
    std::vector<InstructionStatement> statements;
    bool data = false;
    int previous_step = -1;
    std::string line;
    while (std::getline(lines, line)) {
      const auto fields = Split(line);
      if (!data) {
        if (fields.size() >= 4 && fields[2] == "Instruction")
          data = true;
        continue;
      }
      if (fields.size() == 1 && fields[0].empty())
        continue;
      if (fields.size() < 4)
        throw std::runtime_error("Incomplete GX CSV row");
      if (fields[2].empty()) {
        if (fields[3].empty())
          continue;
        if (statements.empty() || statements.back().mnemonic == "END") {
          throw std::runtime_error("Orphan operand continuation");
        }
        statements.back().operands.push_back(fields[3]);
      } else {
        int step = 0;
        const auto [end, status] = std::from_chars(
            fields[0].data(), fields[0].data() + fields[0].size(), step);
        if (status != std::errc{} ||
            end != fields[0].data() + fields[0].size() || step <= previous_step)
          throw std::runtime_error("Invalid GX step numbering");
        previous_step = step;
        std::string mnemonic = fields[2];
        for (const std::string prefix : {"LD", "AND", "OR"}) {
          if (mnemonic.starts_with(prefix + "D")) {
            mnemonic = "D" + prefix + mnemonic.substr(prefix.size() + 1);
            break;
          }
        }
        InstructionStatement statement{mnemonic, {}};
        statement.operands = TokenizeInstructionText(fields[3]);
        statements.push_back(std::move(statement));
      }
    }
    if (!data || statements.empty() || statements.back().mnemonic != "END") {
      throw std::runtime_error("Missing GX CSV header or END");
    }
    ExecutionProgram program;
    return CompileStatements(statements, &program, error) &&
           MaterializeLadder(program, ladder, error);
  } catch (const std::exception& exception) {
    if (error)
      *error = exception.what();
    return false;
  }
}

bool ExportGXCSV(const plc::LadderProgram& ladder, const std::string& name,
                 std::string* content, std::string* error,
                 GXCSVEncoding encoding) {
  if (!content)
    return false;
  ExecutionProgram program;
  if (!CompileLadder(ladder, &program, error))
    return false;
  try {
    std::ostringstream output;
    output << Quote("(" + name + ")") << "\r\n";
    output << Quote("PLC Information:") << '\t' << Quote("FXCPU FX3U/FX3UC")
           << "\r\n";
    output << "\"Step No.\"\t\"Line Statement\"\t\"Instruction\"\t"
              "\"I/O(Device)\"\t\"Blank\"\t\"PI Statement\"\t\"Note\"\r\n";
    int step = 0;
    for (const auto& statement : SerializeInstructions(program)) {
      std::string mnemonic = statement.mnemonic;
      for (const std::string prefix : {"LD", "AND", "OR"}) {
        if (mnemonic.starts_with("D" + prefix)) {
          mnemonic = prefix + "D" + mnemonic.substr(prefix.size() + 1);
          break;
        }
      }
      output << Quote(std::to_string(step)) << "\t\"\"\t"
             << Quote(mnemonic) << '\t'
             << Quote(statement.operands.empty() ? "" : statement.operands[0])
             << "\t\"\"\t\"\"\t\"\"\r\n";
      for (size_t i = 1; i < statement.operands.size(); ++i) {
        output << "\"\"\t\"\"\t\"\"\t" << Quote(statement.operands[i])
               << "\t\"\"\t\"\"\t\"\"\r\n";
      }
      step += StepCount(statement);
    }
    *content = encoding == GXCSVEncoding::kUtf16LE ? EncodeUtf16LE(output.str())
                                                   : output.str();
    return true;
  } catch (const std::exception& exception) {
    if (error)
      *error = exception.what();
    return false;
  }
}
}  // namespace plc_emulator::programming
