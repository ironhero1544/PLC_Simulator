// xml_serializer.cpp
//
// Implementation of XML serializer.

#include "plc_emulator/project/xml_serializer.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>

namespace plc {

XMLSerializer::XMLSerializer() : debug_mode_(false) {}

XMLSerializer::~XMLSerializer() {}

XMLSerializer::SerializationResult XMLSerializer::SerializeToXML(
    const LadderProgram& program) {
  SerializationResult result;
  last_error_.clear();

  LogDebug("🔄 Starting XML serialization...");

  try {
    std::stringstream xml;

    // XML 헤더
    xml << GenerateXMLHeader();
    xml << "<LadderProgram>\n";

    // 메타데이터
    xml << GenerateMetadata();

    // Rungs 직렬화
    xml << SerializeRungs(program.rungs);

    // VerticalConnections 직렬화
    xml << SerializeVerticalConnections(program.verticalConnections);

    xml << "</LadderProgram>\n";

    result.xmlContent = xml.str();
    result.bytesWritten = result.xmlContent.length();
    result.success = true;

    LogDebug("✅ XML serialization completed: " +
             std::to_string(result.bytesWritten) + " bytes");

  } catch (const std::exception& e) {
    result.success = false;
    result.errorMessage = "XML serialization failed: " + std::string(e.what());
    SetError(result.errorMessage);
  }

  return result;
}

XMLSerializer::DeserializationResult XMLSerializer::DeserializeFromXML(
    const std::string& xmlContent) {
  DeserializationResult result;
  last_error_.clear();

  LogDebug("🔄 Starting XML deserialization...");

  try {
    // XML 헤더 검증
    if (!ParseXMLHeader(xmlContent)) {
      result.success = false;
      result.errorMessage = "Invalid XML header: " + last_error_;
      return result;
    }

    // 메타데이터 파싱
    if (!ParseMetadata(xmlContent, result)) {
      result.success = false;
      result.errorMessage = "Failed to parse metadata: " + last_error_;
      return result;
    }

    // Rungs 파싱
    if (!ParseRungs(xmlContent, result.program)) {
      result.success = false;
      result.errorMessage = "Failed to parse rungs: " + last_error_;
      return result;
    }

    // VerticalConnections 파싱
    if (!ParseVerticalConnections(xmlContent, result.program)) {
      result.success = false;
      result.errorMessage =
          "Failed to parse vertical connections: " + last_error_;
      return result;
    }

    result.success = true;
    LogDebug("✅ XML deserialization completed: " +
             std::to_string(result.program.rungs.size()) + " rungs, " +
             std::to_string(result.program.verticalConnections.size()) +
             " connections");

  } catch (const std::exception& e) {
    result.success = false;
    result.errorMessage =
        "XML deserialization failed: " + std::string(e.what());
    SetError(result.errorMessage);
  }

  return result;
}

bool XMLSerializer::SaveToXMLFile(const LadderProgram& program,
                                  const std::string& filePath) {
  auto result = SerializeToXML(program);
  if (!result.success) {
    return false;
  }

  std::ofstream file(filePath);
  if (!file.is_open()) {
    SetError("Cannot open file for writing: " + filePath);
    return false;
  }

  file << result.xmlContent;
  file.close();

  LogDebug("💾 Saved XML to file: " + filePath + " (" +
           std::to_string(result.bytesWritten) + " bytes)");
  return true;
}

XMLSerializer::DeserializationResult XMLSerializer::LoadFromXMLFile(
    const std::string& filePath) {
  DeserializationResult result;

  std::ifstream file(filePath);
  if (!file.is_open()) {
    result.success = false;
    result.errorMessage = "Cannot open file for reading: " + filePath;
    SetError(result.errorMessage);
    return result;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  file.close();

  LogDebug("📖 Loaded XML from file: " + filePath + " (" +
           std::to_string(buffer.str().length()) + " bytes)");

  return DeserializeFromXML(buffer.str());
}

// =============================================================================
// XML 생성 관련 함수들
// =============================================================================

std::string XMLSerializer::GenerateXMLHeader() const {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
}

std::string XMLSerializer::GenerateMetadata() const {
  std::stringstream meta;
  meta << "  <Metadata>\n";
  meta << "    <Version>1.0</Version>\n";
  meta << "    <CreatedDate>" << GetCurrentDateTime() << "</CreatedDate>\n";
  meta << "    <Generator>PLC Ladder Simulator XML Serializer</Generator>\n";
  meta << "    <Description>Complete ladder program structure with UI layout "
          "preservation</Description>\n";
  meta << "  </Metadata>\n";
  return meta.str();
}

std::string XMLSerializer::SerializeRungs(
    const std::vector<Rung>& rungs) const {
  std::stringstream xml;
  xml << "  <Rungs count=\"" << rungs.size() << "\">\n";

  for (size_t i = 0; i < rungs.size(); ++i) {
    xml << SerializeRung(rungs[i], static_cast<int>(i));
  }

  xml << "  </Rungs>\n";
  return xml.str();
}

std::string XMLSerializer::SerializeRung(const Rung& rung, int index) const {
  std::stringstream xml;
  xml << "    <Rung index=\"" << index << "\" number=\"" << rung.number
      << "\" isEndRung=\"" << (rung.isEndRung ? "true" : "false") << "\">\n";
  xml << "      <Cells count=\"" << rung.cells.size() << "\">\n";

  for (size_t i = 0; i < rung.cells.size(); ++i) {
    xml << SerializeCell(rung.cells[i], index, static_cast<int>(i));
  }

  xml << "      </Cells>\n";
  xml << "    </Rung>\n";
  return xml.str();
}

std::string XMLSerializer::SerializeCell(const LadderInstruction& cell,
                                         int rungIndex, int cellIndex) const {
  std::stringstream xml;
  xml << "        <Cell index=\"" << cellIndex << "\" rung=\"" << rungIndex
      << "\">\n";
  xml << "          <Type>" << InstructionTypeToString(cell.type)
      << "</Type>\n";
  xml << "          <Address>" << EscapeXMLString(cell.address)
      << "</Address>\n";
  xml << "          <Preset>" << EscapeXMLString(cell.preset) << "</Preset>\n";
  xml << "          <IsActive>" << (cell.isActive ? "true" : "false")
      << "</IsActive>\n";
  xml << "        </Cell>\n";
  return xml.str();
}

std::string XMLSerializer::SerializeVerticalConnections(
    const std::vector<VerticalConnection>& connections) const {
  std::stringstream xml;
  xml << "  <VerticalConnections count=\"" << connections.size() << "\">\n";

  for (size_t i = 0; i < connections.size(); ++i) {
    xml << SerializeVerticalConnection(connections[i], static_cast<int>(i));
  }

  xml << "  </VerticalConnections>\n";
  return xml.str();
}

std::string XMLSerializer::SerializeVerticalConnection(
    const VerticalConnection& conn, int index) const {
  std::stringstream xml;
  xml << "    <Connection index=\"" << index << "\" x=\"" << conn.x << "\">\n";
  xml << "      <Rungs count=\"" << conn.rungs.size() << "\">";

  for (size_t i = 0; i < conn.rungs.size(); ++i) {
    if (i > 0)
      xml << ",";
    xml << conn.rungs[i];
  }

  xml << "</Rungs>\n";
  xml << "    </Connection>\n";
  return xml.str();
}

// =============================================================================
// XML 파싱 관련 함수들
// =============================================================================

bool XMLSerializer::ParseXMLHeader(const std::string& xmlContent) {
  if (xmlContent.find("<?xml") == std::string::npos) {
    SetError("Missing XML header");
    return false;
  }

  if (xmlContent.find("<LadderProgram>") == std::string::npos) {
    SetError("Missing LadderProgram root element");
    return false;
  }

  return true;
}

bool XMLSerializer::ParseMetadata(const std::string& xmlContent,
                                  DeserializationResult& result) {
  std::string metadata = ExtractXMLTag(xmlContent, "Metadata");
  if (metadata.empty()) {
    LogDebug("⚠️ No metadata found (backward compatibility)");
    return true;  // Metadata는 선택사항
  }

  result.sourceVersion = GetXMLContent(ExtractXMLTag(metadata, "Version"));
  result.createdDate = GetXMLContent(ExtractXMLTag(metadata, "CreatedDate"));

  LogDebug("📄 Metadata parsed - Version: " + result.sourceVersion +
           ", Date: " + result.createdDate);
  return true;
}

bool XMLSerializer::ParseRungs(const std::string& xmlContent,
                               LadderProgram& program) {
  std::string rungsSection = ExtractXMLTag(xmlContent, "Rungs");
  if (rungsSection.empty()) {
    SetError("Missing Rungs section");
    return false;
  }

  auto rungElements = ExtractXMLTags(rungsSection, "Rung");
  program.rungs.clear();
  program.rungs.reserve(rungElements.size());

  for (const auto& rungXML : rungElements) {
    Rung rung;
    if (!ParseRung(rungXML, rung)) {
      return false;
    }
    program.rungs.push_back(rung);
  }

  LogDebug("📋 Parsed " + std::to_string(program.rungs.size()) + " rungs");
  return true;
}

bool XMLSerializer::ParseRung(const std::string& rungXML, Rung& rung) {
  rung.number = std::stoi(GetXMLAttribute(rungXML, "number"));
  rung.isEndRung = (GetXMLAttribute(rungXML, "isEndRung") == "true");

  std::string cellsSection = ExtractXMLTag(rungXML, "Cells");
  if (cellsSection.empty()) {
    SetError("Missing Cells section in rung");
    return false;
  }

  auto cellElements = ExtractXMLTags(cellsSection, "Cell");
  rung.cells.clear();
  rung.cells.resize(12);  // 기본 12셀로 초기화

  for (const auto& cellXML : cellElements) {
    int index = std::stoi(GetXMLAttribute(cellXML, "index"));
    if (index >= 0 && index < static_cast<int>(rung.cells.size())) {
      if (!ParseCell(cellXML, rung.cells[index])) {
        return false;
      }
    }
  }

  return true;
}

bool XMLSerializer::ParseCell(const std::string& cellXML,
                              LadderInstruction& cell) {
  std::string typeStr = GetXMLContent(ExtractXMLTag(cellXML, "Type"));
  cell.type = StringToInstructionType(typeStr);
  cell.address =
      UnescapeXMLString(GetXMLContent(ExtractXMLTag(cellXML, "Address")));
  cell.preset =
      UnescapeXMLString(GetXMLContent(ExtractXMLTag(cellXML, "Preset")));
  cell.isActive = (GetXMLContent(ExtractXMLTag(cellXML, "IsActive")) == "true");

  return true;
}

bool XMLSerializer::ParseVerticalConnections(const std::string& xmlContent,
                                             LadderProgram& program) {
  std::string connectionsSection =
      ExtractXMLTag(xmlContent, "VerticalConnections");
  if (connectionsSection.empty()) {
    LogDebug("⚠️ No vertical connections found");
    return true;  // VerticalConnections는 선택사항
  }

  auto connectionElements = ExtractXMLTags(connectionsSection, "Connection");
  program.verticalConnections.clear();
  program.verticalConnections.reserve(connectionElements.size());

  for (const auto& connXML : connectionElements) {
    VerticalConnection conn;
    if (!ParseVerticalConnection(connXML, conn)) {
      return false;
    }
    program.verticalConnections.push_back(conn);
  }

  LogDebug("🔗 Parsed " + std::to_string(program.verticalConnections.size()) +
           " vertical connections");
  return true;
}

bool XMLSerializer::ParseVerticalConnection(const std::string& connXML,
                                            VerticalConnection& conn) {
  conn.x = std::stoi(GetXMLAttribute(connXML, "x"));

  std::string rungsStr = GetXMLContent(ExtractXMLTag(connXML, "Rungs"));
  if (!rungsStr.empty()) {
    std::stringstream ss(rungsStr);
    std::string item;
    conn.rungs.clear();

    while (std::getline(ss, item, ',')) {
      if (!item.empty()) {
        conn.rungs.push_back(std::stoi(item));
      }
    }
  }

  return true;
}

// =============================================================================
// 유틸리티 함수들
// =============================================================================

std::string XMLSerializer::EscapeXMLString(const std::string& str) const {
  std::string result = str;
  std::regex amp("&");
  result = std::regex_replace(result, amp, "&amp;");
  std::regex lt("<");
  result = std::regex_replace(result, lt, "&lt;");
  std::regex gt(">");
  result = std::regex_replace(result, gt, "&gt;");
  std::regex quot("\"");
  result = std::regex_replace(result, quot, "&quot;");
  std::regex apos("'");
  result = std::regex_replace(result, apos, "&apos;");
  return result;
}

std::string XMLSerializer::UnescapeXMLString(const std::string& str) const {
  std::string result = str;
  std::regex amp("&amp;");
  result = std::regex_replace(result, amp, "&");
  std::regex lt("&lt;");
  result = std::regex_replace(result, lt, "<");
  std::regex gt("&gt;");
  result = std::regex_replace(result, gt, ">");
  std::regex quot("&quot;");
  result = std::regex_replace(result, quot, "\"");
  std::regex apos("&apos;");
  result = std::regex_replace(result, apos, "'");
  return result;
}

std::string XMLSerializer::GetCurrentDateTime() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
  return ss.str();
}

std::string XMLSerializer::InstructionTypeToString(
    LadderInstructionType type) const {
  switch (type) {
    case LadderInstructionType::EMPTY:
      return "EMPTY";
    case LadderInstructionType::XIC:
      return "XIC";
    case LadderInstructionType::XIO:
      return "XIO";
    case LadderInstructionType::OTE:
      return "OTE";
    case LadderInstructionType::HLINE:
      return "HLINE";
    case LadderInstructionType::SET:
      return "SET";
    case LadderInstructionType::RST:
      return "RST";
    case LadderInstructionType::TON:
      return "TON";
    case LadderInstructionType::CTU:
      return "CTU";
    case LadderInstructionType::RST_TMR_CTR:
      return "RST_TMR_CTR";
    default:
      return "UNKNOWN";
  }
}

LadderInstructionType XMLSerializer::StringToInstructionType(
    const std::string& str) const {
  if (str == "EMPTY")
    return LadderInstructionType::EMPTY;
  if (str == "XIC")
    return LadderInstructionType::XIC;
  if (str == "XIO")
    return LadderInstructionType::XIO;
  if (str == "OTE")
    return LadderInstructionType::OTE;
  if (str == "HLINE")
    return LadderInstructionType::HLINE;
  if (str == "SET")
    return LadderInstructionType::SET;
  if (str == "RST")
    return LadderInstructionType::RST;
  if (str == "TON")
    return LadderInstructionType::TON;
  if (str == "CTU")
    return LadderInstructionType::CTU;
  if (str == "RST_TMR_CTR")
    return LadderInstructionType::RST_TMR_CTR;
  return LadderInstructionType::EMPTY;
}

std::string XMLSerializer::ExtractXMLTag(const std::string& xml,
                                         const std::string& tagName) const {
  std::string openTag = "<" + tagName;
  std::string closeTag = "</" + tagName + ">";

  size_t start = xml.find(openTag);
  if (start == std::string::npos)
    return "";

  size_t contentStart = xml.find(">", start) + 1;
  size_t end = xml.find(closeTag, contentStart);
  if (end == std::string::npos)
    return "";

  return xml.substr(start, end + closeTag.length() - start);
}

std::vector<std::string> XMLSerializer::ExtractXMLTags(
    const std::string& xml, const std::string& tagName) const {
  std::vector<std::string> result;
  std::string openTag = "<" + tagName;
  std::string closeTag = "</" + tagName + ">";

  size_t pos = 0;
  while ((pos = xml.find(openTag, pos)) != std::string::npos) {
    size_t end = xml.find(closeTag, pos);
    if (end != std::string::npos) {
      result.push_back(xml.substr(pos, end + closeTag.length() - pos));
      pos = end + closeTag.length();
    } else {
      break;
    }
  }

  return result;
}

std::string XMLSerializer::GetXMLAttribute(const std::string& xmlTag,
                                           const std::string& attrName) const {
  std::string pattern = attrName + "=\"";
  size_t start = xmlTag.find(pattern);
  if (start == std::string::npos)
    return "";

  start += pattern.length();
  size_t end = xmlTag.find("\"", start);
  if (end == std::string::npos)
    return "";

  return xmlTag.substr(start, end - start);
}

std::string XMLSerializer::GetXMLContent(const std::string& xmlTag) const {
  size_t start = xmlTag.find(">");
  if (start == std::string::npos)
    return "";
  start++;

  size_t end = xmlTag.rfind("</");
  if (end == std::string::npos || end <= start)
    return "";

  return xmlTag.substr(start, end - start);
}

void XMLSerializer::LogDebug(const std::string& message) const {
  if (debug_mode_) {
    std::cout << "[XMLSerializer] " << message << std::endl;
  }
}

void XMLSerializer::SetError(const std::string& error) {
  last_error_ = error;
  if (debug_mode_) {
    std::cout << "[XMLSerializer] ERROR: " << error << std::endl;
  }
}

}  // namespace plc