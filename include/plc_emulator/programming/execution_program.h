#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace plc_emulator::programming {

enum class DeviceKind {
  kX,
  kY,
  kM,
  kS,
  kT,
  kC,
  kD,
  kSpecialM,
  kR,
  kV,
  kZ,
  kBufferMemory
};

struct DeviceAddress {
  DeviceKind kind = DeviceKind::kM;
  uint32_t index = 0;
  int8_t bit = -1;
  uint8_t unit = 0;
  char index_kind = 0;
  uint8_t index_register = 0;
  bool operator==(const DeviceAddress&) const = default;
};

enum class OperandKind {
  kBitDevice,
  kWordDevice,
  kImmediateDecimal,
  kImmediateHex,
  kPointer,
  kInterruptPointer,
  kImmediateFloat,
  kImmediateString,
  kNesting,
  kPackedBit
};

struct Operand {
  OperandKind kind = OperandKind::kBitDevice;
  DeviceAddress device;
  int32_t immediate = 0;
  std::string text{};
  bool operator==(const Operand&) const = default;
};

enum class Opcode {
  kOut,
  kSet,
  kReset,
  kTimerOn,
  kCounterUp,
  kMov,
  kAdd,
  kSub,
  kMul,
  kDiv,
  kInc,
  kDec,
  kWordAnd,
  kWordOr,
  kWordXor,
  kCompare,
  kZoneCompare,
  kZeroReset,
  kAlternate,
  kBlockMove,
  kFillMove,
  kRotateRight,
  kRotateLeft,
  kRotateCarryRight,
  kRotateCarryLeft,
  kBitShiftRight,
  kBitShiftLeft,
  kWordShiftRight,
  kWordShiftLeft,
  kDecode,
  kEncode,
  kSumBits,
  kBitTest,
  kComplement,
  kExchange,
  kToBcd,
  kFromBcd,
  kNegate,
  kMean,
  kSquareRoot,
  kByteSwap,
  kToGray,
  kFromGray,
  kWordToBytes,
  kBytesToWord,
  kLimit,
  kDeadBand,
  kShiftWrite,
  kShiftRead,
  kDigitMove,
  kReadClock,
  kWriteClock,
  kLabel,
  kJump,
  kCall,
  kReturn,
  kInterruptReturn,
  kEnableInterrupts,
  kDisableInterrupts,
  kFend,
  kWatchdog,
  kFor,
  kNext,
  kToFloat,
  kFromFloat,
  kFloatCompare,
  kFloatZoneCompare,
  kFloatMove,
  kFloatAdd,
  kFloatSub,
  kFloatMul,
  kFloatDiv,
  kFloatExp,
  kFloatLog,
  kFloatLog10,
  kFloatSqrt,
  kFloatNegate,
  kFloatSin,
  kFloatCos,
  kFloatTan,
  kFloatAsin,
  kFloatAcos,
  kFloatAtan,
  kFloatRadians,
  kFloatDegrees,
  kStringMove,
  kStringConcat,
  kStringLength,
  kStringRight,
  kStringLeft,
  kStringRead,
  kStringWrite,
  kStringSearch,
  kIntegerToString,
  kStringToInteger,
  kFloatToString,
  kStringToFloat,
  kFloatToScientific,
  kScientificToFloat,
  kTableSearch,
  kWordSum,
  kNibbleCombine,
  kNibbleSeparate,
  kTableDelete,
  kTableInsert,
  kStackPop,
  kShiftRightCarry,
  kShiftLeftCarry,
  kBlockAdd,
  kBlockSub,
  kBlockEqual,
  kBlockGreater,
  kBlockLess,
  kBlockNotEqual,
  kBlockLessEqual,
  kBlockGreaterEqual,
  kTableSort,
  kPulseRise,
  kPulseFall,
  kMasterControl,
  kMasterReset,
  kNop,
  kTimeCompare,
  kTimeZoneCompare,
  kTimeAdd,
  kTimeSub,
  kHoursToSeconds,
  kSecondsToHours,
  kHourMeter,
  kRandom,
  kDuty,
  kCrc,
  kCheckCode,
  kZone,
  kScale,
  kScale2,
  kDecimalAsciiToBinary,
  kBinaryToDecimalAscii,
  kReadModule,
  kWriteModule,
  kReadModuleDivided,
  kWriteModuleDivided,
  kVolumeRead,
  kVolumeScale,
  kReadAnalog,
  kWriteAnalog,
  kAnnunciatorSet,
  kAnnunciatorReset,
  kTeachingTimer,
  kSpecialTimer,
  kRamp,
  kSevenSegment,
  kAsciiConstant,
  kHexToAscii,
  kAsciiToHex,
  kParallelRun,
  kHighSpeedCounterMove,
  kTableSort2,
  kIndexPush,
  kIndexPop,
  kLoadExtension,
  kSaveExtension,
  kInitializeExtension,
  kLogExtension,
  kRewriteExtension,
  kInitializeExtensionFile,
  kReadComment,
  kRefreshIo,
  kRefreshFilter,
  kInputMatrix,
  kHighSpeedSet,
  kHighSpeedReset,
  kHighSpeedZone,
  kHighSpeedTable,
  kSpeedMeasure,
  kTenKey,
  kHexKey,
  kDigitalSwitch,
  kSevenSegmentLatch,
  kArrowSwitch,
  kAbsoluteDrum,
  kIncrementalDrum,
  kRotaryTable,
  kPulseOutput,
  kPulseWidth,
  kPulseRamp,
  kDogSearch,
  kInterruptPosition,
  kZeroReturn,
  kVariablePulse,
  kRelativePosition,
  kAbsolutePosition,
  kPositionTable,
  kAbsoluteRead,
  kPid,
  kPrint,
  kSerial,
  kSerial2,
  kInitialState,
  kStep,
  kStepEnd,
  kInverterCheck,
  kInverterDrive,
  kInverterRead,
  kInverterWrite,
  kInverterBlockWrite,
  kInverterMulti,
  kModbus,
  kCfCreate,
  kCfDelete,
  kCfWrite,
  kCfRead,
  kCfCommand,
  kCfStatus
};

enum class OperandRule {
  kBit,
  kWritableBit,
  kWritableDevice,
  kTimer,
  kCounter,
  kValue,
  kWord,
  kDataDevice,
  kDataValue,
  kWritableDataDevice,
  kBitRange,
  kWritableBitRange,
  kImmediate,
  kCount,
  kPointer,
  kLabel,
  kFloatValue,
  kStringValue,
  kNesting,
  kWritableValue,
  kHighSpeedTarget,
  kDevice
};
inline constexpr size_t kMaxOperands = 5;

struct InstructionDef {
  Opcode opcode;
  std::string_view mnemonic;
  uint16_t fnc_no;
  uint8_t operand_count;
  std::array<OperandRule, kMaxOperands> operand_rules;
  bool supports_wide = true;
  bool supports_pulse = true;
  bool requires_wide = false;
};

bool IsRegisterDevice(DeviceAddress device);
uint32_t WordDeviceLimit(DeviceAddress device);
DeviceAddress OffsetBitAddress(DeviceAddress device, uint32_t offset);
std::optional<DeviceAddress> ParseDeviceAddress(std::string_view text);
std::string FormatDeviceAddress(DeviceAddress address);
const std::string& FormatIOAddress(char kind, int canonical_index);
std::optional<Operand> ParseOperand(std::string_view text);
std::string FormatOperand(const Operand& operand);
std::span<const InstructionDef> GetInstructionDefinitions();
const InstructionDef* FindInstruction(std::string_view mnemonic);
bool ValidateOperands(const InstructionDef& definition,
                      std::span<const Operand> operands, std::string* error);

enum class GateKind {
  kConstant,
  kContact,
  kNot,
  kAnd,
  kOr,
  kRising,
  kFalling,
  kEqual,
  kNotEqual,
  kGreater,
  kLess,
  kGreaterEqual,
  kLessEqual
};

// Topologically ordered logic data, evaluated by the existing OpenPLC executor.
// Gate references always refer to earlier entries; there is no bytecode/VM.
struct LogicGate {
  GateKind kind = GateKind::kConstant;
  uint32_t left = 0;
  uint32_t right = 0;
  bool constant = false;
  Operand first{};
  Operand second{};
  bool wide = false;
  bool operator==(const LogicGate&) const = default;
};

struct CellObservation {
  int rung = -1;
  int cell = -1;
  uint32_t gate = 0;
  std::vector<uint32_t> incoming_gates;
};

struct OpenPLCInstruction {
  Opcode opcode = Opcode::kOut;
  std::array<Operand, kMaxOperands> operands{};
  uint8_t operand_count = 0;
  bool pulse = false;
  bool wide = false;
  std::vector<LogicGate> condition;
  uint32_t condition_root = UINT32_MAX;
  uint32_t condition_begin = 0;
  int evaluation_group = -1;
  int rung = -1;
  int cell = -1;
  // Editor observations do not participate in instruction semantics.
  std::vector<CellObservation> observations;
};

enum class SequenceKind {
  kLoad,
  kAnd,
  kOr,
  kAndBlock,
  kOrBlock,
  kInvert,
  kRising,
  kFalling,
  kPush,
  kRead,
  kPop,
  kAction,
  kEnd
};

// Canonical import/export AST metadata; the executor evaluates only the DAG.
// Keeping saved branch nodes here makes CSV round trips preserve scan order.
struct CanonicalStatement {
  SequenceKind kind = SequenceKind::kEnd;
  LogicGate contact{};
  bool inverted = false;
  bool rising = false;
  bool falling = false;
  OpenPLCInstruction action{};
};

struct ExecutionProgram {
  std::vector<OpenPLCInstruction> instructions;
  std::vector<CanonicalStatement> source_topology;
};

bool ValidateProgram(const ExecutionProgram& program, std::string* error,
                     bool validate_flow = true);
bool IsIndependentInstruction(Opcode opcode);
// Resolved destinations and loop partners, prepared once when loading a
// program.
bool BuildFlowTargets(const ExecutionProgram& program,
                      std::vector<size_t>* targets, std::string* error);
size_t TableCountOperand(Opcode opcode);
bool ValidateTableOperation(const OpenPLCInstruction& instruction,
                            std::optional<int32_t> count, std::string* error);
bool ValidateDataOperation(const OpenPLCInstruction& instruction, int32_t count,
                           int32_t shift, std::string* error);
bool ValidateCpuOperation(const OpenPLCInstruction& instruction,
                          std::optional<int32_t> count, std::string* error);

}  // namespace plc_emulator::programming
