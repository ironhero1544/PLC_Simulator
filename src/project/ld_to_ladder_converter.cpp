// ld_to_ladder_converter.cpp
//
// Implementation of ladder converter.

#include "plc_emulator/project/ld_to_ladder_converter.h"

#include "plc_emulator/core/data_types.h"
#include "plc_emulator/programming/programming_mode.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

namespace plc {

LDToLadderConverter::LDToLadderConverter() : debug_mode_(false) {}

LDToLadderConverter::~LDToLadderConverter() {}

bool LDToLadderConverter::ConvertFromLDFile(const std::string& ldFilePath,
                                            LadderProgram& ladderProgram) {
  std::ifstream file(ldFilePath);
  if (!file.is_open()) {
    SetError("Failed to open .ld file: " + ldFilePath);
    return false;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string ldContent = buffer.str();
  file.close();

  LogDebug("📖 Reading .ld file: " + ldFilePath + " (" +
           std::to_string(ldContent.length()) + " bytes)");

  return ConvertFromLDString(ldContent, ladderProgram);
}

bool LDToLadderConverter::ConvertFromLDString(const std::string& ldContent,
                                              LadderProgram& ladderProgram) {
  stats_ = ConversionStats();
  parsed_networks_.clear();

  if (!ParseLDFile(ldContent)) {
    return false;
  }

  if (!ConvertNetworksToLadder(ladderProgram)) {
    return false;
  }

  if (!ValidateConvertedLadder(ladderProgram)) {
    return false;
  }

  LogDebug("✅ Conversion completed successfully");
  LogDebug("📊 Stats: " + std::to_string(stats_.networksCount) +
           " networks, " + std::to_string(stats_.contactsCount) +
           " contacts, " + std::to_string(stats_.coilsCount) + " coils");

  return true;
}

bool LDToLadderConverter::ParseLDFile(const std::string& ldContent) {
  LogDebug("🔍 Parsing .ld file content...");

  std::vector<std::string> networkSections =
      ExtractXMLTags(ldContent, "network");

  if (networkSections.empty()) {
    SetError("No networks found in .ld file");
    return false;
  }

  LogDebug("📋 Found " + std::to_string(networkSections.size()) + " networks");

  for (size_t i = 0; i < networkSections.size(); ++i) {
    LDNetwork network;
    network.networkNumber = static_cast<int>(i);

    if (!ParseNetwork(networkSections[i], network)) {
      SetError("Failed to parse network " + std::to_string(i));
      return false;
    }

    parsed_networks_.push_back(network);
    stats_.networksCount++;
  }

  return true;
}

bool LDToLadderConverter::ParseNetwork(const std::string& networkXML,
                                       LDNetwork& network) {
  LogDebug("🔍 Parsing network " + std::to_string(network.networkNumber));

  std::vector<std::string> contacts = ExtractXMLTags(networkXML, "contact");
  std::vector<std::string> coils = ExtractXMLTags(networkXML, "coil");
  std::vector<std::string> blocks = ExtractXMLTags(networkXML, "block");

  for (const auto& contactXML : contacts) {
    LDElement element;
    if (ParseElement(contactXML, element)) {
      element.type = "contact";
      network.elements.push_back(element);
      stats_.contactsCount++;
    }
  }

  for (const auto& coilXML : coils) {
    LDElement element;
    if (ParseElement(coilXML, element)) {
      element.type = "coil";
      network.elements.push_back(element);
      stats_.coilsCount++;
    }
  }

  for (const auto& blockXML : blocks) {
    LDElement element;
    if (ParseElement(blockXML, element)) {
      element.type = "block";
      network.elements.push_back(element);
      stats_.blocksCount++;
    }
  }

  std::vector<std::string> connections =
      ExtractXMLTags(networkXML, "connection");
  for (const auto& conn : connections) {
    network.connections.push_back(conn);
    stats_.connectionsCount++;
  }

  LogDebug("📊 Network " + std::to_string(network.networkNumber) + ": " +
           std::to_string(network.elements.size()) + " elements, " +
           std::to_string(network.connections.size()) + " connections");

  return true;
}

bool LDToLadderConverter::ParseElement(const std::string& elementXML,
                                       LDElement& element) {
  element.name = GetXMLAttribute(elementXML, "name");
  element.operation = GetXMLAttribute(elementXML, "operation");

  std::string xStr = GetXMLAttribute(elementXML, "x");
  std::string yStr = GetXMLAttribute(elementXML, "y");

  if (!xStr.empty())
    element.x = std::stoi(xStr);
  if (!yStr.empty())
    element.y = std::stoi(yStr);

  element.attributes["negated"] = GetXMLAttribute(elementXML, "negated");
  element.attributes["edge"] = GetXMLAttribute(elementXML, "edge");
  element.attributes["storage"] = GetXMLAttribute(elementXML, "storage");

  LogDebug("🔧 Parsed element: " + element.name + " (" + element.operation +
           ") at (" + std::to_string(element.x) + "," +
           std::to_string(element.y) + ")");

  return !element.name.empty();
}

bool LDToLadderConverter::ConvertNetworksToLadder(
    LadderProgram& ladderProgram) {
  LogDebug("🔄 Converting networks to ladder program...");

  ladderProgram.rungs.clear();
  ladderProgram.verticalConnections.clear();

  for (const auto& network : parsed_networks_) {
    Rung rung;
    rung.number = network.networkNumber;

    if (!ConvertNetworkToRung(network, rung)) {
      SetError("Failed to convert network " +
               std::to_string(network.networkNumber) + " to rung");
      return false;
    }

    ladderProgram.rungs.push_back(rung);
  }

  Rung endRung;
  endRung.isEndRung = true;
  ladderProgram.rungs.push_back(endRung);

  GenerateVerticalConnections(ladderProgram);

  LogDebug("✅ Converted to " + std::to_string(ladderProgram.rungs.size()) +
           " rungs with " +
           std::to_string(ladderProgram.verticalConnections.size()) +
           " vertical connections");

  return true;
}

bool LDToLadderConverter::ConvertNetworkToRung(const LDNetwork& network,
                                               Rung& rung) {
  LogDebug("🔄 Converting network " + std::to_string(network.networkNumber) +
           " to rung");

  rung.cells.resize(12);
  for (auto& cell : rung.cells) {
    cell.type = LadderInstructionType::EMPTY;
  }

  std::vector<LDElement> sortedElements = network.elements;
  std::sort(sortedElements.begin(), sortedElements.end(),
            [](const LDElement& a, const LDElement& b) { return a.x < b.x; });

  int cellIndex = 0;
  for (const auto& element : sortedElements) {
    if (cellIndex >= 12) {
      LogDebug("⚠️ Warning: Network " + std::to_string(network.networkNumber) +
               " has more than 12 elements, truncating");
      break;
    }

    LadderInstruction& instruction = rung.cells[cellIndex];

    instruction.type =
        ConvertLDTypeToInstruction(element.type, element.operation);
    instruction.address = element.name;

    if (element.attributes.count("negated") &&
        element.attributes.at("negated") == "true") {
      if (instruction.type == LadderInstructionType::XIC) {
        instruction.type = LadderInstructionType::XIO;
      }
    }

    LogDebug("📍 Cell " + std::to_string(cellIndex) + ": " +
             (instruction.address.empty() ? "EMPTY" : instruction.address) +
             " (" + std::to_string(static_cast<int>(instruction.type)) + ")");

    cellIndex++;
  }

  OptimizeRungLayout(rung);

  return true;
}

LadderInstructionType LDToLadderConverter::ConvertLDTypeToInstruction(
    const std::string& ldType, const std::string& operation) {
  if (ldType == "contact") {
    if (operation == "LD" || operation == "AND") {
      return LadderInstructionType::XIC;
    } else if (operation == "LDN" || operation == "ANDN") {
      return LadderInstructionType::XIO;
    } else if (operation == "OR") {
      return LadderInstructionType::XIC;
    } else if (operation == "ORN") {
      return LadderInstructionType::XIO;
    }
  } else if (ldType == "coil") {
    if (operation == "ST") {
      return LadderInstructionType::OTE;
    } else if (operation == "S") {
      return LadderInstructionType::SET;
    } else if (operation == "R") {
      return LadderInstructionType::RST;
    }
  } else if (ldType == "block") {
    if (operation == "TON") {
      return LadderInstructionType::TON;
    } else if (operation == "CTU") {
      return LadderInstructionType::CTU;
    } else if (operation == "RES") {
      return LadderInstructionType::RST_TMR_CTR;
    }
  }

  LogDebug("⚠️ Unknown LD type/operation: " + ldType + "/" + operation +
           " -> EMPTY");
  return LadderInstructionType::EMPTY;
}

void LDToLadderConverter::OptimizeRungLayout(Rung& rung) {
  std::vector<LadderInstruction> nonEmptyInstructions;

  for (const auto& cell : rung.cells) {
    if (cell.type != LadderInstructionType::EMPTY) {
      nonEmptyInstructions.push_back(cell);
    }
  }

  for (auto& cell : rung.cells) {
    cell.type = LadderInstructionType::EMPTY;
    cell.address.clear();
    cell.preset.clear();
    cell.isActive = false;
  }

  for (size_t i = 0; i < nonEmptyInstructions.size() && i < rung.cells.size();
       ++i) {
    rung.cells[i] = nonEmptyInstructions[i];
  }
}

void LDToLadderConverter::GenerateVerticalConnections(
    LadderProgram& ladderProgram) {
  (void)ladderProgram;
  LogDebug("🔗 Generating vertical connections (simplified implementation)");

}


std::string LDToLadderConverter::ExtractXMLTag(const std::string& xml,
                                               const std::string& tagName) {
  std::string openTag = "<" + tagName;
  std::string closeTag = "</" + tagName + ">";

  size_t startPos = xml.find(openTag);
  if (startPos == std::string::npos) {
    return "";
  }

  size_t endPos = xml.find(closeTag, startPos);
  if (endPos == std::string::npos) {
    return "";
  }

  startPos = xml.find('>', startPos);
  if (startPos == std::string::npos) {
    return "";
  }

  return xml.substr(startPos + 1, endPos - startPos - 1);
}

std::vector<std::string> LDToLadderConverter::ExtractXMLTags(
    const std::string& xml, const std::string& tagName) {
  std::vector<std::string> results;
  std::string openTag = "<" + tagName;
  std::string closeTag = "</" + tagName + ">";

  size_t pos = 0;
  while (true) {
    size_t startPos = xml.find(openTag, pos);
    if (startPos == std::string::npos) {
      break;
    }

    size_t endPos = xml.find(closeTag, startPos);
    if (endPos == std::string::npos) {
      break;
    }

    endPos += closeTag.length();
    results.push_back(xml.substr(startPos, endPos - startPos));
    pos = endPos;
  }

  return results;
}

std::string LDToLadderConverter::GetXMLAttribute(const std::string& xmlTag,
                                                 const std::string& attrName) {
  std::string attrPattern = attrName + "=\"";
  size_t startPos = xmlTag.find(attrPattern);

  if (startPos == std::string::npos) {
    return "";
  }

  startPos += attrPattern.length();
  size_t endPos = xmlTag.find('"', startPos);

  if (endPos == std::string::npos) {
    return "";
  }

  return xmlTag.substr(startPos, endPos - startPos);
}

void LDToLadderConverter::LogDebug(const std::string& message) {
  if (debug_mode_) {
    std::cout << "[LDToLadder] " << message << std::endl;
  }
}

void LDToLadderConverter::SetError(const std::string& error) {
  last_error_ = error;
  LogDebug("❌ Error: " + error);
}

bool LDToLadderConverter::ValidateConvertedLadder(
    const LadderProgram& ladderProgram) {
  LogDebug("🔍 Validating converted ladder program...");

  if (ladderProgram.rungs.empty()) {
    SetError("No rungs in converted ladder program");
    return false;
  }

  for (const auto& rung : ladderProgram.rungs) {
    if (rung.isEndRung)
      continue;

    for (const auto& cell : rung.cells) {
      if (!CheckInstructionValidity(cell)) {
        SetError("Invalid instruction in rung " + std::to_string(rung.number));
        return false;
      }
    }
  }

  LogDebug("✅ Ladder program validation passed");
  return true;
}

bool LDToLadderConverter::CheckInstructionValidity(
    const LadderInstruction& instruction) {
  if (instruction.type == LadderInstructionType::EMPTY) {
    return true;
  }

  if (instruction.type == LadderInstructionType::XIC ||
      instruction.type == LadderInstructionType::XIO ||
      instruction.type == LadderInstructionType::OTE ||
      instruction.type == LadderInstructionType::SET ||
      instruction.type == LadderInstructionType::RST ||
      instruction.type == LadderInstructionType::TON ||
      instruction.type == LadderInstructionType::CTU ||
      instruction.type == LadderInstructionType::RST_TMR_CTR ||
      instruction.type == LadderInstructionType::BKRST) {
    if (instruction.address.empty()) {
      LogDebug("⚠️ Instruction without address: type " +
               std::to_string(static_cast<int>(instruction.type)));
      return false;
    }
  }

  return true;
}

}  // namespace plc
