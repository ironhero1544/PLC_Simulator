#include "plc_emulator/programming/execution_program.h"

#include <algorithm>
#include <bit>
#include <cctype>
#include <charconv>
#include <cmath>
#include <limits>
#include <map>

namespace plc_emulator::programming {
namespace {
using Rule = OperandRule;
constexpr InstructionDef kDefinitions[] = {
    {Opcode::kCfCreate,
     "FLCRT",
     300,
     4,
     {Rule::kValue, Rule::kStringValue, Rule::kWord, Rule::kCount},
     false,
     false},
    {Opcode::kCfDelete,
     "FLDEL",
     301,
     3,
     {Rule::kValue, Rule::kValue, Rule::kCount},
     false,
     false},
    {Opcode::kCfWrite,
     "FLWR",
     302,
     5,
     {Rule::kValue, Rule::kDataDevice, Rule::kWord, Rule::kWord, Rule::kCount},
     false,
     false},
    {Opcode::kCfRead,
     "FLRD",
     303,
     5,
     {Rule::kValue, Rule::kWord, Rule::kWritableDataDevice, Rule::kWord,
      Rule::kCount},
     false,
     false},
    {Opcode::kCfCommand,
     "FLCMD",
     304,
     2,
     {Rule::kValue, Rule::kCount},
     false,
     false},
    {Opcode::kCfStatus,
     "FLSTRD",
     305,
     3,
     {Rule::kValue, Rule::kWord, Rule::kCount},
     false,
     false},
    {Opcode::kModbus,
     "ADPRW",
     276,
     5,
     {Rule::kValue, Rule::kValue, Rule::kValue, Rule::kValue, Rule::kDataValue},
     false,
     false},
    {Opcode::kInverterCheck,
     "IVCK",
     270,
     4,
     {Rule::kValue, Rule::kValue, Rule::kWritableDataDevice, Rule::kCount},
     false,
     false},
    {Opcode::kInverterDrive,
     "IVDR",
     271,
     4,
     {Rule::kValue, Rule::kValue, Rule::kValue, Rule::kCount},
     false,
     false},
    {Opcode::kInverterRead,
     "IVRD",
     272,
     4,
     {Rule::kValue, Rule::kValue, Rule::kWritableDataDevice, Rule::kCount},
     false,
     false},
    {Opcode::kInverterWrite,
     "IVWR",
     273,
     4,
     {Rule::kValue, Rule::kValue, Rule::kValue, Rule::kCount},
     false,
     false},
    {Opcode::kInverterBlockWrite,
     "IVBWR",
     274,
     4,
     {Rule::kValue, Rule::kValue, Rule::kWord, Rule::kCount},
     false,
     false},
    {Opcode::kInverterMulti,
     "IVMC",
     275,
     5,
     {Rule::kValue, Rule::kValue, Rule::kWord, Rule::kWord, Rule::kCount},
     false,
     false},
    {Opcode::kStep, "STL", 0, 1, {Rule::kBit}, false, false},
    {Opcode::kStepEnd, "RET", 0, 0, {}, false, false},
    {Opcode::kInitialState,
     "IST",
     60,
     3,
     {Rule::kBit, Rule::kBit, Rule::kBit},
     false,
     false},
    {Opcode::kSerial,
     "RS",
     80,
     4,
     {Rule::kStringValue, Rule::kCount, Rule::kWord, Rule::kCount},
     false,
     false},
    {Opcode::kSerial2,
     "RS2",
     87,
     5,
     {Rule::kStringValue, Rule::kCount, Rule::kWord, Rule::kCount,
      Rule::kCount},
     false,
     false},
    {Opcode::kPrint,
     "PR",
     77,
     2,
     {Rule::kDataDevice, Rule::kWritableBit},
     false,
     false},
    {Opcode::kPid,
     "PID",
     88,
     4,
     {Rule::kWord, Rule::kWord, Rule::kWord, Rule::kWord},
     false,
     false},
    {Opcode::kPositionTable,
     "TBL",
     152,
     2,
     {Rule::kWritableBit, Rule::kCount},
     true,
     false,
     true},
    {Opcode::kAbsoluteRead,
     "ABS",
     155,
     3,
     {Rule::kBit, Rule::kWritableBit, Rule::kWord},
     true,
     false,
     true},
    {Opcode::kPulseOutput,
     "PLSY",
     57,
     3,
     {Rule::kValue, Rule::kValue, Rule::kWritableBit},
     true,
     false},
    {Opcode::kPulseWidth,
     "PWM",
     58,
     3,
     {Rule::kValue, Rule::kValue, Rule::kWritableBit},
     false,
     false},
    {Opcode::kPulseRamp,
     "PLSR",
     59,
     4,
     {Rule::kValue, Rule::kValue, Rule::kValue, Rule::kWritableBit},
     true,
     false},
    {Opcode::kDogSearch,
     "DSZR",
     150,
     4,
     {Rule::kBit, Rule::kBit, Rule::kWritableBit, Rule::kWritableBit},
     false,
     false},
    {Opcode::kInterruptPosition,
     "DVIT",
     151,
     4,
     {Rule::kValue, Rule::kValue, Rule::kWritableBit, Rule::kWritableBit},
     true,
     false},
    {Opcode::kZeroReturn,
     "ZRN",
     156,
     4,
     {Rule::kValue, Rule::kValue, Rule::kBit, Rule::kWritableBit},
     true,
     false},
    {Opcode::kVariablePulse,
     "PLSV",
     157,
     3,
     {Rule::kValue, Rule::kWritableBit, Rule::kWritableBit},
     true,
     false},
    {Opcode::kRelativePosition,
     "DRVI",
     158,
     4,
     {Rule::kValue, Rule::kValue, Rule::kWritableBit, Rule::kWritableBit},
     true,
     false},
    {Opcode::kAbsolutePosition,
     "DRVA",
     159,
     4,
     {Rule::kValue, Rule::kValue, Rule::kWritableBit, Rule::kWritableBit},
     true,
     false},

    {Opcode::kAbsoluteDrum,
     "ABSD",
     62,
     4,
     {Rule::kValue, Rule::kCounter, Rule::kWritableBit, Rule::kCount},
     true,
     false},
    {Opcode::kIncrementalDrum,
     "INCD",
     63,
     4,
     {Rule::kValue, Rule::kCounter, Rule::kWritableBit, Rule::kCount},
     false,
     false},
    {Opcode::kRotaryTable,
     "ROTC",
     68,
     4,
     {Rule::kWord, Rule::kCount, Rule::kCount, Rule::kWritableBit},
     false,
     false},
    {Opcode::kTenKey,
     "TKY",
     70,
     3,
     {Rule::kBit, Rule::kWritableValue, Rule::kWritableBit},
     true,
     false},
    {Opcode::kHexKey,
     "HKY",
     71,
     4,
     {Rule::kBit, Rule::kWritableBit, Rule::kWritableValue, Rule::kWritableBit},
     true,
     false},
    {Opcode::kDigitalSwitch,
     "DSW",
     72,
     4,
     {Rule::kBit, Rule::kWritableBit, Rule::kWord, Rule::kCount},
     false,
     false},
    {Opcode::kSevenSegmentLatch,
     "SEGL",
     74,
     3,
     {Rule::kValue, Rule::kWritableBit, Rule::kCount},
     false,
     false},
    {Opcode::kArrowSwitch,
     "ARWS",
     75,
     4,
     {Rule::kBit, Rule::kWord, Rule::kWritableBit, Rule::kCount},
     false,
     false},
    {Opcode::kRefreshIo, "REF", 50, 2, {Rule::kBit, Rule::kCount}, false},
    {Opcode::kRefreshFilter, "REFF", 51, 1, {Rule::kCount}, false},
    {Opcode::kInputMatrix,
     "MTR",
     52,
     4,
     {Rule::kBit, Rule::kWritableBit, Rule::kWritableBit, Rule::kCount},
     false,
     false},
    {Opcode::kHighSpeedSet,
     "HSCS",
     53,
     3,
     {Rule::kValue, Rule::kCounter, Rule::kHighSpeedTarget},
     true,
     false,
     true},
    {Opcode::kHighSpeedReset,
     "HSCR",
     54,
     3,
     {Rule::kValue, Rule::kCounter, Rule::kWritableBit},
     true,
     false,
     true},
    {Opcode::kHighSpeedZone,
     "HSZ",
     55,
     4,
     {Rule::kValue, Rule::kValue, Rule::kCounter, Rule::kWritableBit},
     true,
     false,
     true},
    {Opcode::kHighSpeedTable,
     "HSCT",
     280,
     5,
     {Rule::kWord, Rule::kCount, Rule::kCounter, Rule::kWritableBit,
      Rule::kCount},
     true,
     false,
     true},
    {Opcode::kSpeedMeasure,
     "SPD",
     56,
     3,
     {Rule::kBit, Rule::kValue, Rule::kWord},
     true,
     false},
    {Opcode::kReadComment,
     "COMRD",
     182,
     2,
     {Rule::kDevice, Rule::kWord},
     false},
    {Opcode::kIndexPush, "ZPUSH", 102, 1, {Rule::kWord}, false},
    {Opcode::kIndexPop, "ZPOP", 103, 1, {Rule::kWord}, false},
    {Opcode::kLoadExtension,
     "LOADR",
     290,
     2,
     {Rule::kWord, Rule::kCount},
     false},
    {Opcode::kSaveExtension,
     "SAVER",
     291,
     3,
     {Rule::kWord, Rule::kCount, Rule::kWord},
     false,
     false},
    {Opcode::kInitializeExtension,
     "INITR",
     292,
     2,
     {Rule::kWord, Rule::kCount},
     false},
    {Opcode::kLogExtension,
     "LOGR",
     293,
     5,
     {Rule::kWord, Rule::kCount, Rule::kWord, Rule::kCount, Rule::kWord},
     false},
    {Opcode::kRewriteExtension,
     "RWER",
     294,
     2,
     {Rule::kWord, Rule::kCount},
     false,
     false},
    {Opcode::kInitializeExtensionFile,
     "INITER",
     295,
     2,
     {Rule::kWord, Rule::kCount},
     false},
    {Opcode::kTableSort2,
     "SORT2",
     149,
     5,
     {Rule::kWord, Rule::kValue, Rule::kImmediate, Rule::kWord, Rule::kValue},
     true,
     false},
    {Opcode::kAnnunciatorSet,
     "ANS",
     46,
     3,
     {Rule::kTimer, Rule::kValue, Rule::kWritableBit},
     false,
     false},
    {Opcode::kAnnunciatorReset, "ANR", 47, 0, {}, false},
    {Opcode::kTeachingTimer,
     "TTMR",
     64,
     2,
     {Rule::kWord, Rule::kValue},
     false,
     false},
    {Opcode::kSpecialTimer,
     "STMR",
     65,
     3,
     {Rule::kTimer, Rule::kValue, Rule::kWritableBit},
     false,
     false},
    {Opcode::kRamp,
     "RAMP",
     67,
     4,
     {Rule::kWord, Rule::kWord, Rule::kWord, Rule::kCount},
     false,
     false},
    {Opcode::kSevenSegment,
     "SEGD",
     73,
     2,
     {Rule::kValue, Rule::kWritableValue},
     false},
    {Opcode::kAsciiConstant,
     "ASC",
     76,
     2,
     {Rule::kStringValue, Rule::kWord},
     false,
     false},
    {Opcode::kHexToAscii,
     "ASCI",
     82,
     3,
     {Rule::kWord, Rule::kWord, Rule::kCount},
     false},
    {Opcode::kAsciiToHex,
     "HEX",
     83,
     3,
     {Rule::kWord, Rule::kWord, Rule::kCount},
     false},
    {Opcode::kParallelRun, "PRUN", 81, 2, {Rule::kValue, Rule::kWritableValue}},
    {Opcode::kHighSpeedCounterMove,
     "HCMOV",
     189,
     2,
     {Rule::kValue, Rule::kWritableValue},
     true,
     true,
     true},
    {Opcode::kReadModule,
     "FROM",
     78,
     4,
     {Rule::kValue, Rule::kValue, Rule::kWord, Rule::kCount}},
    {Opcode::kWriteModule,
     "TO",
     79,
     4,
     {Rule::kValue, Rule::kValue, Rule::kValue, Rule::kCount}},
    {Opcode::kReadModuleDivided,
     "RBFM",
     278,
     5,
     {Rule::kValue, Rule::kValue, Rule::kWord, Rule::kCount, Rule::kCount},
     false,
     false},
    {Opcode::kWriteModuleDivided,
     "WBFM",
     279,
     5,
     {Rule::kValue, Rule::kValue, Rule::kWord, Rule::kCount, Rule::kCount},
     false,
     false},
    {Opcode::kVolumeRead,
     "VRRD",
     85,
     2,
     {Rule::kValue, Rule::kWritableValue},
     false},
    {Opcode::kVolumeScale,
     "VRSC",
     86,
     2,
     {Rule::kValue, Rule::kWritableValue},
     false},
    {Opcode::kReadAnalog,
     "RD3A",
     176,
     3,
     {Rule::kValue, Rule::kValue, Rule::kWritableValue},
     false},
    {Opcode::kWriteAnalog,
     "WR3A",
     177,
     3,
     {Rule::kValue, Rule::kValue, Rule::kValue},
     false},
    {Opcode::kTimeCompare,
     "TCMP",
     160,
     5,
     {Rule::kValue, Rule::kValue, Rule::kValue, Rule::kWord,
      Rule::kWritableBit},
     false},
    {Opcode::kTimeZoneCompare,
     "TZCP",
     161,
     4,
     {Rule::kWord, Rule::kWord, Rule::kWord, Rule::kWritableBit},
     false},
    {Opcode::kTimeAdd,
     "TADD",
     162,
     3,
     {Rule::kWord, Rule::kWord, Rule::kWord},
     false},
    {Opcode::kTimeSub,
     "TSUB",
     163,
     3,
     {Rule::kWord, Rule::kWord, Rule::kWord},
     false},
    {Opcode::kHoursToSeconds, "HTOS", 164, 2, {Rule::kWord, Rule::kWord}},
    {Opcode::kSecondsToHours, "STOH", 165, 2, {Rule::kValue, Rule::kWord}},
    {Opcode::kHourMeter,
     "HOUR",
     169,
     3,
     {Rule::kValue, Rule::kWord, Rule::kWritableBit},
     true,
     false},
    {Opcode::kRandom, "RND", 184, 1, {Rule::kWritableValue}, false},
    {Opcode::kDuty,
     "DUTY",
     186,
     3,
     {Rule::kValue, Rule::kValue, Rule::kWritableBit},
     false,
     false},
    {Opcode::kCrc,
     "CRC",
     188,
     3,
     {Rule::kWord, Rule::kWord, Rule::kCount},
     false},
    {Opcode::kCheckCode,
     "CCD",
     84,
     3,
     {Rule::kWord, Rule::kWord, Rule::kCount},
     false},
    {Opcode::kZone,
     "ZONE",
     258,
     4,
     {Rule::kValue, Rule::kValue, Rule::kValue, Rule::kWritableValue}},
    {Opcode::kScale,
     "SCL",
     259,
     3,
     {Rule::kValue, Rule::kWord, Rule::kWritableValue}},
    {Opcode::kScale2,
     "SCL2",
     269,
     3,
     {Rule::kValue, Rule::kWord, Rule::kWritableValue}},
    {Opcode::kDecimalAsciiToBinary,
     "DABIN",
     260,
     2,
     {Rule::kWord, Rule::kWritableValue}},
    {Opcode::kBinaryToDecimalAscii,
     "BINDA",
     261,
     2,
     {Rule::kValue, Rule::kWord}},
    {Opcode::kPulseRise, "PLS", 0, 1, {Rule::kWritableBit}, false, false},
    {Opcode::kPulseFall, "PLF", 0, 1, {Rule::kWritableBit}, false, false},
    {Opcode::kMasterControl,
     "MC",
     0,
     2,
     {Rule::kNesting, Rule::kWritableBit},
     false,
     false},
    {Opcode::kMasterReset, "MCR", 0, 1, {Rule::kNesting}, false, false},
    {Opcode::kNop, "NOP", 0, 0, {}, false, false},
    {Opcode::kTableSearch,
     "SER",
     61,
     4,
     {Rule::kWord, Rule::kValue, Rule::kWord, Rule::kCount}},
    {Opcode::kWordSum,
     "WSUM",
     140,
     3,
     {Rule::kWord, Rule::kWord, Rule::kCount}},
    {Opcode::kNibbleCombine,
     "UNI",
     143,
     3,
     {Rule::kWord, Rule::kWord, Rule::kCount},
     false},
    {Opcode::kNibbleSeparate,
     "DIS",
     144,
     3,
     {Rule::kValue, Rule::kWord, Rule::kCount},
     false},
    {Opcode::kTableDelete,
     "FDEL",
     210,
     3,
     {Rule::kWord, Rule::kWord, Rule::kCount},
     false},
    {Opcode::kTableInsert,
     "FINS",
     211,
     3,
     {Rule::kValue, Rule::kWord, Rule::kCount},
     false},
    {Opcode::kStackPop,
     "POP",
     212,
     3,
     {Rule::kWord, Rule::kWord, Rule::kImmediate},
     false},
    {Opcode::kShiftRightCarry,
     "SFR",
     213,
     2,
     {Rule::kWord, Rule::kCount},
     false},
    {Opcode::kShiftLeftCarry,
     "SFL",
     214,
     2,
     {Rule::kWord, Rule::kCount},
     false},
    {Opcode::kBlockAdd,
     "BK+",
     192,
     4,
     {Rule::kWord, Rule::kValue, Rule::kWord, Rule::kCount}},
    {Opcode::kBlockSub,
     "BK-",
     193,
     4,
     {Rule::kWord, Rule::kValue, Rule::kWord, Rule::kCount}},
    {Opcode::kTableSort,
     "SORT",
     69,
     5,
     {Rule::kWord, Rule::kImmediate, Rule::kImmediate, Rule::kWord,
      Rule::kCount},
     false,
     false},
    {Opcode::kBlockEqual,
     "BKCMP=",
     194,
     4,
     {Rule::kValue, Rule::kWord, Rule::kWritableBitRange, Rule::kCount}},
    {Opcode::kBlockGreater,
     "BKCMP>",
     195,
     4,
     {Rule::kValue, Rule::kWord, Rule::kWritableBitRange, Rule::kCount}},
    {Opcode::kBlockLess,
     "BKCMP<",
     196,
     4,
     {Rule::kValue, Rule::kWord, Rule::kWritableBitRange, Rule::kCount}},
    {Opcode::kBlockNotEqual,
     "BKCMP<>",
     197,
     4,
     {Rule::kValue, Rule::kWord, Rule::kWritableBitRange, Rule::kCount}},
    {Opcode::kBlockLessEqual,
     "BKCMP<=",
     198,
     4,
     {Rule::kValue, Rule::kWord, Rule::kWritableBitRange, Rule::kCount}},
    {Opcode::kBlockGreaterEqual,
     "BKCMP>=",
     199,
     4,
     {Rule::kValue, Rule::kWord, Rule::kWritableBitRange, Rule::kCount}},

    {Opcode::kStringMove,
     "$MOV",
     209,
     2,
     {Rule::kStringValue, Rule::kWord},
     false},
    {Opcode::kStringConcat,
     "$+",
     202,
     3,
     {Rule::kStringValue, Rule::kStringValue, Rule::kWord},
     false},
    {Opcode::kStringLength,
     "LEN",
     203,
     2,
     {Rule::kStringValue, Rule::kWord},
     false},
    {Opcode::kStringRight,
     "RIGHT",
     204,
     3,
     {Rule::kStringValue, Rule::kWord, Rule::kCount},
     false},
    {Opcode::kStringLeft,
     "LEFT",
     205,
     3,
     {Rule::kStringValue, Rule::kWord, Rule::kCount},
     false},
    {Opcode::kStringRead,
     "MIDR",
     206,
     3,
     {Rule::kStringValue, Rule::kWord, Rule::kWord},
     false},
    {Opcode::kStringWrite,
     "MIDW",
     207,
     3,
     {Rule::kStringValue, Rule::kWord, Rule::kWord},
     false},
    {Opcode::kStringSearch,
     "INSTR",
     208,
     4,
     {Rule::kStringValue, Rule::kStringValue, Rule::kWord, Rule::kCount},
     false},
    {Opcode::kIntegerToString,
     "STR",
     200,
     3,
     {Rule::kWord, Rule::kValue, Rule::kWord}},
    {Opcode::kStringToInteger,
     "VAL",
     201,
     3,
     {Rule::kStringValue, Rule::kWord, Rule::kWord}},
    {Opcode::kFloatToString,
     "ESTR",
     116,
     3,
     {Rule::kFloatValue, Rule::kWord, Rule::kWord},
     true,
     true,
     true},
    {Opcode::kStringToFloat,
     "EVAL",
     117,
     2,
     {Rule::kStringValue, Rule::kWord},
     true,
     true,
     true},
    {Opcode::kFloatToScientific,
     "EBCD",
     118,
     2,
     {Rule::kWord, Rule::kWord},
     true,
     true,
     true},
    {Opcode::kScientificToFloat,
     "EBIN",
     119,
     2,
     {Rule::kWord, Rule::kWord},
     true,
     true,
     true},

    {Opcode::kToFloat, "FLT", 49, 2, {Rule::kValue, Rule::kWord}},
    {Opcode::kFromFloat, "INT", 129, 2, {Rule::kWord, Rule::kWord}},
    {Opcode::kFloatCompare,
     "ECMP",
     110,
     3,
     {Rule::kFloatValue, Rule::kFloatValue, Rule::kWritableBit},
     true,
     true,
     true},
    {Opcode::kFloatZoneCompare,
     "EZCP",
     111,
     4,
     {Rule::kFloatValue, Rule::kFloatValue, Rule::kFloatValue,
      Rule::kWritableBit},
     true,
     true,
     true},
    {Opcode::kFloatMove,
     "EMOV",
     112,
     2,
     {Rule::kFloatValue, Rule::kWord},
     true,
     true,
     true},
    {Opcode::kFloatAdd,
     "EADD",
     120,
     3,
     {Rule::kFloatValue, Rule::kFloatValue, Rule::kWord},
     true,
     true,
     true},
    {Opcode::kFloatSub,
     "ESUB",
     121,
     3,
     {Rule::kFloatValue, Rule::kFloatValue, Rule::kWord},
     true,
     true,
     true},
    {Opcode::kFloatMul,
     "EMUL",
     122,
     3,
     {Rule::kFloatValue, Rule::kFloatValue, Rule::kWord},
     true,
     true,
     true},
    {Opcode::kFloatDiv,
     "EDIV",
     123,
     3,
     {Rule::kFloatValue, Rule::kFloatValue, Rule::kWord},
     true,
     true,
     true},
    {Opcode::kFloatExp,
     "EXP",
     124,
     2,
     {Rule::kFloatValue, Rule::kWord},
     true,
     true,
     true},
    {Opcode::kFloatLog,
     "LOGE",
     125,
     2,
     {Rule::kFloatValue, Rule::kWord},
     true,
     true,
     true},
    {Opcode::kFloatLog10,
     "LOG10",
     126,
     2,
     {Rule::kFloatValue, Rule::kWord},
     true,
     true,
     true},
    {Opcode::kFloatSqrt,
     "ESQR",
     127,
     2,
     {Rule::kFloatValue, Rule::kWord},
     true,
     true,
     true},
    {Opcode::kFloatNegate, "ENEG", 128, 1, {Rule::kWord}, true, true, true},
    {Opcode::kFloatSin,
     "SIN",
     130,
     2,
     {Rule::kFloatValue, Rule::kWord},
     true,
     true,
     true},
    {Opcode::kFloatCos,
     "COS",
     131,
     2,
     {Rule::kFloatValue, Rule::kWord},
     true,
     true,
     true},
    {Opcode::kFloatTan,
     "TAN",
     132,
     2,
     {Rule::kFloatValue, Rule::kWord},
     true,
     true,
     true},
    {Opcode::kFloatAsin,
     "ASIN",
     133,
     2,
     {Rule::kFloatValue, Rule::kWord},
     true,
     true,
     true},
    {Opcode::kFloatAcos,
     "ACOS",
     134,
     2,
     {Rule::kFloatValue, Rule::kWord},
     true,
     true,
     true},
    {Opcode::kFloatAtan,
     "ATAN",
     135,
     2,
     {Rule::kFloatValue, Rule::kWord},
     true,
     true,
     true},
    {Opcode::kFloatRadians,
     "RAD",
     136,
     2,
     {Rule::kFloatValue, Rule::kWord},
     true,
     true,
     true},
    {Opcode::kFloatDegrees,
     "DEG",
     137,
     2,
     {Rule::kFloatValue, Rule::kWord},
     true,
     true,
     true},

    {Opcode::kLabel, "LABEL", 0, 1, {Rule::kLabel}, false, false},
    {Opcode::kJump, "CJ", 0, 1, {Rule::kPointer}, false},
    {Opcode::kCall, "CALL", 1, 1, {Rule::kPointer}, false},
    {Opcode::kReturn, "SRET", 2, 0, {}, false, false},
    {Opcode::kInterruptReturn, "IRET", 3, 0, {}, false, false},
    {Opcode::kEnableInterrupts, "EI", 4, 0, {}, false, false},
    {Opcode::kDisableInterrupts, "DI", 5, 0, {}, false, false},
    {Opcode::kFend, "FEND", 6, 0, {}, false, false},
    {Opcode::kWatchdog, "WDT", 7, 0, {}, false},
    {Opcode::kFor, "FOR", 8, 1, {Rule::kValue}, false, false},
    {Opcode::kNext, "NEXT", 9, 0, {}, false, false},
    {Opcode::kComplement, "CML", 14, 2, {Rule::kValue, Rule::kWritableValue}},
    {Opcode::kExchange,
     "XCH",
     17,
     2,
     {Rule::kWritableValue, Rule::kWritableValue}},
    {Opcode::kToBcd, "BCD", 18, 2, {Rule::kValue, Rule::kWritableValue}},
    {Opcode::kFromBcd, "BIN", 19, 2, {Rule::kValue, Rule::kWritableValue}},
    {Opcode::kNegate, "NEG", 29, 1, {Rule::kWritableValue}},
    {Opcode::kMean, "MEAN", 45, 3, {Rule::kWord, Rule::kWord, Rule::kCount}},
    {Opcode::kSquareRoot, "SQR", 48, 2, {Rule::kValue, Rule::kWritableValue}},
    {Opcode::kByteSwap, "SWAP", 147, 1, {Rule::kWritableValue}},
    {Opcode::kToGray, "GRY", 170, 2, {Rule::kValue, Rule::kWritableValue}},
    {Opcode::kFromGray, "GBIN", 171, 2, {Rule::kValue, Rule::kWritableValue}},
    {Opcode::kWordToBytes,
     "WTOB",
     141,
     3,
     {Rule::kWord, Rule::kWord, Rule::kCount},
     false},
    {Opcode::kBytesToWord,
     "BTOW",
     142,
     3,
     {Rule::kWord, Rule::kWord, Rule::kCount},
     false},
    {Opcode::kLimit,
     "LIMIT",
     256,
     4,
     {Rule::kValue, Rule::kValue, Rule::kValue, Rule::kWritableValue}},
    {Opcode::kDeadBand,
     "BAND",
     257,
     4,
     {Rule::kValue, Rule::kValue, Rule::kValue, Rule::kWritableValue}},
    {Opcode::kShiftWrite,
     "SFWR",
     38,
     3,
     {Rule::kValue, Rule::kWord, Rule::kImmediate},
     false},
    {Opcode::kShiftRead,
     "SFRD",
     39,
     3,
     {Rule::kWord, Rule::kWord, Rule::kImmediate},
     false},
    {Opcode::kDigitMove,
     "SMOV",
     13,
     5,
     {Rule::kWord, Rule::kImmediate, Rule::kImmediate, Rule::kWord,
      Rule::kImmediate},
     false},
    {Opcode::kReadClock, "TRD", 166, 1, {Rule::kWord}, false},
    {Opcode::kWriteClock, "TWR", 167, 1, {Rule::kWord}, false},

    {Opcode::kRotateRight, "ROR", 30, 2, {Rule::kWord, Rule::kCount}},
    {Opcode::kRotateLeft, "ROL", 31, 2, {Rule::kWord, Rule::kCount}},
    {Opcode::kRotateCarryRight, "RCR", 32, 2, {Rule::kWord, Rule::kCount}},
    {Opcode::kRotateCarryLeft, "RCL", 33, 2, {Rule::kWord, Rule::kCount}},
    {Opcode::kBitShiftRight,
     "SFTR",
     34,
     4,
     {Rule::kBitRange, Rule::kWritableBitRange, Rule::kImmediate,
      Rule::kCount}},
    {Opcode::kBitShiftLeft,
     "SFTL",
     35,
     4,
     {Rule::kBitRange, Rule::kWritableBitRange, Rule::kImmediate,
      Rule::kCount}},
    {Opcode::kWordShiftRight,
     "WSFR",
     36,
     4,
     {Rule::kWord, Rule::kWord, Rule::kImmediate, Rule::kCount}},
    {Opcode::kWordShiftLeft,
     "WSFL",
     37,
     4,
     {Rule::kWord, Rule::kWord, Rule::kImmediate, Rule::kCount}},
    {Opcode::kDecode,
     "DECO",
     41,
     3,
     {Rule::kDataValue, Rule::kWritableDataDevice, Rule::kImmediate}},
    {Opcode::kEncode,
     "ENCO",
     42,
     3,
     {Rule::kDataDevice, Rule::kWord, Rule::kImmediate}},
    {Opcode::kSumBits, "SUM", 43, 2, {Rule::kWord, Rule::kWord}},
    {Opcode::kBitTest,
     "BON",
     44,
     3,
     {Rule::kWord, Rule::kWritableBitRange, Rule::kCount}},
    {Opcode::kOut, "OUT", 0, 1, {Rule::kWritableBit}},
    {Opcode::kSet, "SET", 0, 1, {Rule::kWritableBit}},
    {Opcode::kReset, "RST", 0, 1, {Rule::kWritableDevice}},
    {Opcode::kTimerOn, "TON", 0, 2, {Rule::kTimer, Rule::kValue}},
    {Opcode::kCounterUp, "CTU", 0, 2, {Rule::kCounter, Rule::kValue}},
    {Opcode::kMov, "MOV", 12, 2, {Rule::kValue, Rule::kWritableValue}},
    {Opcode::kAdd,
     "ADD",
     20,
     3,
     {Rule::kValue, Rule::kValue, Rule::kWritableValue}},
    {Opcode::kSub,
     "SUB",
     21,
     3,
     {Rule::kValue, Rule::kValue, Rule::kWritableValue}},
    {Opcode::kMul, "MUL", 22, 3, {Rule::kValue, Rule::kValue, Rule::kWord}},
    {Opcode::kDiv, "DIV", 23, 3, {Rule::kValue, Rule::kValue, Rule::kWord}},
    {Opcode::kInc, "INC", 24, 1, {Rule::kWritableValue}},
    {Opcode::kDec, "DEC", 25, 1, {Rule::kWritableValue}},
    {Opcode::kWordAnd,
     "WAND",
     26,
     3,
     {Rule::kValue, Rule::kValue, Rule::kWritableValue}},
    {Opcode::kWordOr,
     "WOR",
     27,
     3,
     {Rule::kValue, Rule::kValue, Rule::kWritableValue}},
    {Opcode::kWordXor,
     "WXOR",
     28,
     3,
     {Rule::kValue, Rule::kValue, Rule::kWritableValue}},
    {Opcode::kCompare,
     "CMP",
     10,
     3,
     {Rule::kValue, Rule::kValue, Rule::kWritableBit}},
    {Opcode::kZoneCompare,
     "ZCP",
     11,
     4,
     {Rule::kValue, Rule::kValue, Rule::kValue, Rule::kWritableBit}},
    {Opcode::kZeroReset,
     "ZRST",
     40,
     2,
     {Rule::kWritableDevice, Rule::kWritableDevice}},
    {Opcode::kAlternate, "ALT", 66, 1, {Rule::kWritableBit}},
    {Opcode::kBlockMove,
     "BMOV",
     15,
     3,
     {Rule::kWord, Rule::kWord, Rule::kValue}},
    {Opcode::kFillMove,
     "FMOV",
     16,
     3,
     {Rule::kValue, Rule::kWord, Rule::kValue}},
};

std::string Upper(std::string_view text) {
  std::string result(text);
  for (char& ch : result) {
    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  }
  return result;
}

bool Matches(OperandRule rule, const Operand& operand) {
  const bool device = operand.kind == OperandKind::kBitDevice ||
                      operand.kind == OperandKind::kWordDevice ||
                      operand.kind == OperandKind::kPackedBit;
  const DeviceKind kind = operand.device.kind;
  if (rule == Rule::kDevice)
    return operand.kind == OperandKind::kBitDevice ||
           operand.kind == OperandKind::kWordDevice;
  if (rule == Rule::kNesting)
    return operand.kind == OperandKind::kNesting && operand.immediate >= 0 &&
           operand.immediate <= 7;
  if (rule == Rule::kStringValue)
    return operand.kind == OperandKind::kImmediateString ||
           (device && IsRegisterDevice(operand.device));
  if (rule == Rule::kFloatValue)
    return operand.kind == OperandKind::kImmediateFloat ||
           operand.kind == OperandKind::kImmediateDecimal ||
           operand.kind == OperandKind::kImmediateHex ||
           (device && IsRegisterDevice(operand.device));
  if (rule == Rule::kPointer)
    return operand.kind == OperandKind::kPointer;
  if (rule == Rule::kHighSpeedTarget)
    return operand.kind == OperandKind::kInterruptPointer ||
           Matches(Rule::kWritableBit, operand);
  if (rule == Rule::kLabel)
    return operand.kind == OperandKind::kPointer ||
           operand.kind == OperandKind::kInterruptPointer;
  if (operand.kind == OperandKind::kPointer ||
      operand.kind == OperandKind::kInterruptPointer ||
      operand.kind == OperandKind::kImmediateFloat ||
      operand.kind == OperandKind::kImmediateString ||
      operand.kind == OperandKind::kNesting)
    return false;
  if (rule == Rule::kWritableValue)
    return (device && (IsRegisterDevice(operand.device) ||
                       kind == DeviceKind::kT || kind == DeviceKind::kC)) ||
           (operand.kind == OperandKind::kPackedBit &&
            (kind == DeviceKind::kY || kind == DeviceKind::kM ||
             kind == DeviceKind::kS));
  if (operand.kind == OperandKind::kPackedBit)
    return rule == Rule::kValue;
  switch (rule) {
    case Rule::kDevice:
    case Rule::kHighSpeedTarget:
    case Rule::kWritableValue:
    case Rule::kNesting:
      return false;
    case Rule::kStringValue:
    case Rule::kFloatValue:
    case Rule::kPointer:
    case Rule::kLabel:
      return false;
    case Rule::kImmediate:
      return !device;
    case Rule::kCount:
      return !device || IsRegisterDevice(operand.device);
    case Rule::kDataValue:
      if (!device)
        return true;
      return Matches(Rule::kDataDevice, operand);
    case Rule::kDataDevice:
    case Rule::kWritableDataDevice:
      return device && (IsRegisterDevice(operand.device) ||
                        (kind == DeviceKind::kX && rule == Rule::kDataDevice) ||
                        kind == DeviceKind::kY || kind == DeviceKind::kM ||
                        kind == DeviceKind::kS);
    case Rule::kBitRange:
    case Rule::kWritableBitRange:
      return device && (kind == DeviceKind::kY || kind == DeviceKind::kM ||
                        kind == DeviceKind::kS ||
                        (rule == Rule::kBitRange && kind == DeviceKind::kX));
    case Rule::kBit:
      return operand.kind == OperandKind::kBitDevice;
    case Rule::kWritableBit:
      return operand.kind == OperandKind::kBitDevice &&
             kind != DeviceKind::kX &&
             (kind != DeviceKind::kSpecialM ||
              (operand.device.index >= 15 && operand.device.index != 20 &&
               operand.device.index != 21 && operand.device.index != 22 &&
               operand.device.index != 29));
    case Rule::kWritableDevice:
      return device && kind != DeviceKind::kX &&
             (kind != DeviceKind::kSpecialM ||
              (operand.device.index >= 15 && operand.device.index != 20 &&
               operand.device.index != 21 && operand.device.index != 22 &&
               operand.device.index != 29));
    case Rule::kTimer:
      return device && kind == DeviceKind::kT;
    case Rule::kCounter:
      return device && kind == DeviceKind::kC;
    case Rule::kValue:
      return !device || IsRegisterDevice(operand.device) ||
             kind == DeviceKind::kT || kind == DeviceKind::kC;
    case Rule::kWord:
      return device && IsRegisterDevice(operand.device);
  }
  return false;
}
bool ValidateExtendedOperands(const OpenPLCInstruction& instruction,
                              std::string* error) {
  const auto& operands = instruction.operands;
  const Opcode opcode = instruction.opcode;
  const int width = instruction.wide ? 2 : 1;
  const auto fail = [error](const char* message) {
    if (error)
      *error = message;
    return false;
  };
  const auto constant = [](const Operand& operand) -> std::optional<int32_t> {
    if (!operand.device.index_kind &&
        (operand.kind == OperandKind::kImmediateDecimal ||
         operand.kind == OperandKind::kImmediateHex))
      return operand.immediate;
    return std::nullopt;
  };
  const auto words = [&](int operand, int count) {
    const auto device = operands[operand].device;
    return IsRegisterDevice(device) &&
           device.index + count <= WordDeviceLimit(device);
  };
  const auto bits = [&](int operand, int count) {
    auto device = operands[operand].device;
    if (device.kind == DeviceKind::kT || device.kind == DeviceKind::kC)
      return false;
    device = OffsetBitAddress(device, count - 1);
    return ParseDeviceAddress(FormatDeviceAddress(device)) ==
           std::optional(device);
  };
  if (opcode >= Opcode::kCfCreate && opcode <= Opcode::kCfStatus) {
    const auto channel = constant(operands[instruction.operand_count - 1]);
    if (channel && (*channel < 1 || *channel > 2))
      return fail("CF-ADP requires channel 1 or 2");
    const auto normal_words = [&](int operand, int count) {
      const auto device = operands[operand].device;
      return device.bit < 0 &&
             ((device.kind == DeviceKind::kD && device.index + count <= 8000) ||
              (device.kind == DeviceKind::kR && device.index + count <= 32768));
    };
    if ((opcode == Opcode::kCfCreate && !normal_words(2, 4)) ||
        (opcode == Opcode::kCfWrite &&
         (!normal_words(2, 5) || !normal_words(3, 2))) ||
        (opcode == Opcode::kCfRead &&
         (!normal_words(1, 4) || !normal_words(3, 1))))
      return fail("CF-ADP parameter or result block exceeds normal D/R memory");
  }
  if (opcode == Opcode::kStep &&
      (operands[0].device.kind != DeviceKind::kS ||
       operands[0].device.index_kind ||
       (operands[0].device.index >= 900 && operands[0].device.index <= 999)))
    return fail("STL requires a non-indexed initial or practical S relay");
  if (opcode >= Opcode::kInverterCheck && opcode <= Opcode::kInverterMulti) {
    const auto station = constant(operands[0]);
    const auto channel = constant(operands[instruction.operand_count - 1]);
    if ((station && (*station < 0 || *station > 31)) ||
        (channel && (*channel < 1 || *channel > 2)))
      return fail("Inverter station must be 0..31 and channel must be 1/2");
    if (opcode == Opcode::kInverterMulti && (!words(2, 2) || !words(3, 2)))
      return fail("IVMC requires two source and destination words");
    if (opcode == Opcode::kInverterBlockWrite) {
      const auto count = constant(operands[1]);
      if (count && (*count < 1 || *count > 32767 || !words(2, *count * 2)))
        return fail("IVBWR parameter table exceeds register memory");
    }
  }
  if (opcode == Opcode::kInitialState) {
    const auto first = operands[1].device;
    const auto last = operands[2].device;
    if (!bits(0, 8) || first.kind != DeviceKind::kS ||
        last.kind != DeviceKind::kS || first.index_kind || last.index_kind ||
        first.index < 20 || first.index >= last.index ||
        (first.index >= 900 && first.index <= 999) ||
        (last.index >= 900 && last.index <= 999))
      return fail("IST requires eight selector bits and a practical S range");
  }
  if (opcode >= Opcode::kPulseOutput && opcode <= Opcode::kPositionTable) {
    const int output_operand =
        opcode == Opcode::kPositionTable                                ? 0
        : opcode == Opcode::kVariablePulse                              ? 1
        : opcode == Opcode::kPulseRamp || opcode == Opcode::kZeroReturn ? 3
                                                                        : 2;
    const auto output = operands[output_operand].device;
    const bool pulse_counter =
        opcode == Opcode::kPulseOutput || opcode == Opcode::kPulseRamp;
    if (output.kind != DeviceKind::kY ||
        (!output.index_kind && output.index >= (pulse_counter ? 2u : 4u)))
      return fail("Positioning requires Y0..Y3 (PLSY/PLSR Y0..Y1)");
    if (opcode == Opcode::kPositionTable) {
      const auto entry = constant(operands[1]);
      if (entry && (*entry < 1 || *entry > 100))
        return fail("DTBL table number must be 1..100");
    }
  }
  if (opcode == Opcode::kAbsoluteRead &&
      (!bits(0, 3) || !bits(1, 3) || !words(2, 2)))
    return fail("DABS input/output or destination block exceeds memory");
  if (opcode == Opcode::kPid && ((operands[2].device.kind != DeviceKind::kD &&
                                  operands[2].device.kind != DeviceKind::kR) ||
                                 !words(2, 20)))
    return fail("PID requires twenty contiguous D/R parameter words");
  if (opcode == Opcode::kRefreshIo) {
    const auto device = operands[0].device;
    const auto count = constant(operands[1]);
    if ((device.kind != DeviceKind::kX && device.kind != DeviceKind::kY) ||
        device.index % 8 ||
        (count && (*count < 8 || *count % 8 || device.index + *count > 256)))
      return fail("REF requires groups of eight X/Y devices");
  }
  if (opcode == Opcode::kRefreshFilter) {
    const auto count = constant(operands[0]);
    if (count && (*count < 0 || *count > 60))
      return fail("REFF input filter must be 0 to 60ms");
  }
  if (opcode >= Opcode::kHighSpeedSet && opcode <= Opcode::kHighSpeedTable) {
    const int counter_operand =
        opcode == Opcode::kHighSpeedSet || opcode == Opcode::kHighSpeedReset
            ? 1
            : 2;
    const auto counter = operands[counter_operand].device;
    if (counter.kind != DeviceKind::kC || counter.index < 235 ||
        counter.index > 255)
      return fail("High-speed comparison requires C235 to C255");
    if (opcode == Opcode::kHighSpeedZone) {
      const auto target = operands[3].device;
      const bool table = target.kind == DeviceKind::kSpecialM &&
                         (target.index == 130 || target.index == 132);
      if (!table && !bits(3, 3))
        return fail("HSZ result exceeds three writable bits");
    }
  }
  if (opcode == Opcode::kSpeedMeasure) {
    const auto source = operands[0].device;
    const auto period = constant(operands[1]);
    if (source.kind != DeviceKind::kX || source.index >= 8 ||
        (period && *period < 1) || !words(2, width * 3))
      return fail("Invalid SPD input, period or memory range");
  }
  if (opcode == Opcode::kIndexPush || opcode == Opcode::kIndexPop) {
    const auto device = operands[0].device;
    if (device.kind != DeviceKind::kD || device.index >= 8000 ||
        device.index + 17 > 8000)
      return fail("ZPUSH/ZPOP require normal data registers");
  }
  if (opcode >= Opcode::kLoadExtension &&
      opcode <= Opcode::kInitializeExtensionFile) {
    const auto device =
        operands[opcode == Opcode::kLogExtension ? 2 : 0].device;
    if (device.kind != DeviceKind::kR)
      return fail("Extension file operation requires R registers");
    const bool sector = opcode == Opcode::kSaveExtension ||
                        opcode == Opcode::kInitializeExtension ||
                        opcode == Opcode::kInitializeExtensionFile ||
                        opcode == Opcode::kLogExtension;
    if (sector && device.index % 2048)
      return fail("Extension file operation requires a sector head");
  }
  if (opcode == Opcode::kTeachingTimer || opcode == Opcode::kRamp) {
    const int count_index = opcode == Opcode::kRamp ? 3 : 1;
    const auto count = constant(operands[count_index]);
    if (!words(opcode == Opcode::kRamp ? 2 : 0, 2) ||
        (count && (*count < (opcode == Opcode::kRamp ? 1 : 0) ||
                   *count > (opcode == Opcode::kRamp ? 32767 : 2))))
      return fail("Invalid teaching timer or ramp operands");
  }
  if (opcode == Opcode::kAnnunciatorSet || opcode == Opcode::kSpecialTimer) {
    const auto count = constant(operands[1]);
    if (operands[0].device.index >= 200 ||
        (count && (*count < 1 || *count > 32767)))
      return fail("Instruction requires a 100ms timer and positive preset");
    if (opcode == Opcode::kAnnunciatorSet &&
        (operands[2].device.kind != DeviceKind::kS ||
         operands[2].device.index < 900 || operands[2].device.index > 999))
      return fail("ANS destination must be S900 to S999");
    if (opcode == Opcode::kSpecialTimer && !bits(2, 4))
      return fail("STMR result exceeds four writable bits");
  }
  if (opcode == Opcode::kAsciiConstant &&
      (operands[0].kind != OperandKind::kImmediateString ||
       operands[0].text.size() > 8 || !words(1, 4) ||
       std::any_of(operands[0].text.begin(), operands[0].text.end(),
                   [](unsigned char c) { return c < 0x20 || c > 0x7e; })))
    return fail("ASC requires up to eight printable ASCII characters");
  if (opcode == Opcode::kDuty) {
    const auto device = operands[2].device;
    if (device.kind != DeviceKind::kSpecialM || device.index < 330 ||
        device.index > 334)
      return fail("DUTY destination must be M8330 to M8334");
    for (int operand = 0; operand < 2; ++operand)
      if (const auto count = constant(operands[operand]); count && *count < 0)
        return fail("DUTY scan count must be nonnegative");
  }
  if (opcode == Opcode::kTimeAdd || opcode == Opcode::kTimeSub ||
      opcode == Opcode::kTimeZoneCompare)
    for (int operand = 0; operand < 3; ++operand)
      if (!words(operand, 3))
        return fail("Time operation exceeds register range");
  if ((opcode == Opcode::kTimeCompare && !words(3, 3)) ||
      (opcode == Opcode::kHoursToSeconds && !words(0, 3)) ||
      (opcode == Opcode::kSecondsToHours && !words(1, 3)) ||
      (opcode == Opcode::kHourMeter && !words(1, width + 1)))
    return fail("Time operation exceeds register range");
  if ((opcode == Opcode::kDecimalAsciiToBinary &&
       !words(0, instruction.wide ? 6 : 3)) ||
      (opcode == Opcode::kBinaryToDecimalAscii &&
       !words(1, instruction.wide ? 6 : 3)))
    return fail("Decimal ASCII conversion exceeds register range");
  if (opcode == Opcode::kHexToAscii || opcode == Opcode::kAsciiToHex ||
      opcode == Opcode::kCrc || opcode == Opcode::kCheckCode) {
    if (const auto count = constant(operands[2]);
        count && (*count < 1 || *count > 256))
      return fail("Conversion count must be 1 to 256");
  }
  if (opcode == Opcode::kVolumeRead || opcode == Opcode::kVolumeScale)
    if (const auto channel = constant(operands[0]);
        channel && (*channel < 0 || *channel > 7))
      return fail("Volume number must be 0 to 7");
  if (opcode >= Opcode::kReadModule && opcode <= Opcode::kWriteModuleDivided) {
    const auto unit = constant(operands[0]);
    const auto address = constant(operands[1]);
    const auto count = constant(operands[3]);
    if ((unit && (*unit < 0 || *unit > 7)) ||
        (address && (*address < 0 || *address > 32766)) ||
        (count && (*count < 1 || *count > 32767)) ||
        (address && count &&
         static_cast<int64_t>(*address) + *count * width > 32767))
      return fail("Module transfer exceeds unit or BFM range");
    if (count && operands[2].kind == OperandKind::kWordDevice &&
        !words(2, *count * width))
      return fail("Module transfer exceeds PLC word range");
    if (opcode == Opcode::kReadModuleDivided ||
        opcode == Opcode::kWriteModuleDivided)
      if (const auto chunk = constant(operands[4]);
          chunk && (*chunk < 1 || *chunk > 32767))
        return fail("Invalid divided transfer chunk");
  }
  if (opcode == Opcode::kHighSpeedCounterMove) {
    const auto source = operands[0].device;
    if (!(source.kind == DeviceKind::kC && source.index >= 235 &&
          source.index <= 255) &&
        !(source.kind == DeviceKind::kD &&
          (source.index == 8099 || source.index == 8398)))
      return fail("HCMOV requires a high-speed or ring counter");
  }
  if (opcode == Opcode::kTableSort2) {
    const auto rows = constant(operands[1]);
    const auto key = constant(operands[4]);
    const int columns = operands[2].immediate;
    if (columns < 1 || columns > 6 ||
        (rows &&
         (*rows < 1 || *rows > 32 || !words(0, *rows * columns * width) ||
          !words(3, *rows * columns * width))) ||
        (key && (*key < 1 || *key > columns)))
      return fail("Invalid SORT2 dimensions, key or register range");
  }
  return true;
}
}  // namespace

bool IsRegisterDevice(DeviceAddress device) {
  return device.bit < 0 &&
         (device.kind == DeviceKind::kD || device.kind == DeviceKind::kR ||
          device.kind == DeviceKind::kV || device.kind == DeviceKind::kZ ||
          device.kind == DeviceKind::kBufferMemory);
}
uint32_t WordDeviceLimit(DeviceAddress device) {
  switch (device.kind) {
    case DeviceKind::kD:
      return device.index < 8000 ? 8000 : 8512;
    case DeviceKind::kR:
      return 32768;
    case DeviceKind::kV:
    case DeviceKind::kZ:
      return 8;
    case DeviceKind::kBufferMemory:
      return 32767;
    default:
      return 0;
  }
}
DeviceAddress OffsetBitAddress(DeviceAddress device, uint32_t offset) {
  if (device.bit >= 0) {
    const uint32_t bit = static_cast<uint32_t>(device.bit) + offset;
    device.index += bit / 16;
    device.bit = static_cast<int8_t>(bit % 16);
  } else {
    device.index += offset;
  }
  return device;
}

std::optional<DeviceAddress> ParseDeviceAddress(std::string_view text) {
  const size_t modifier = text.find_last_of("VvZz");
  if (modifier != std::string_view::npos && modifier > 0) {
    const auto base = ParseDeviceAddress(text.substr(0, modifier));
    const char kind = static_cast<char>(
        std::toupper(static_cast<unsigned char>(text[modifier])));
    uint32_t number = 0;
    const auto suffix = text.substr(modifier + 1);
    const auto [end, status] =
        std::from_chars(suffix.data(), suffix.data() + suffix.size(), number);
    if (!base || base->index_kind || base->bit >= 0 ||
        base->kind == DeviceKind::kV || base->kind == DeviceKind::kZ ||
        base->kind == DeviceKind::kSpecialM ||
        (!suffix.empty() &&
         (status != std::errc{} || end != suffix.data() + suffix.size())) ||
        number >= 8)
      return std::nullopt;
    auto indexed = *base;
    indexed.index_kind = kind;
    indexed.index_register = static_cast<uint8_t>(number);
    return indexed;
  }
  if (text == "V" || text == "v")
    return DeviceAddress{DeviceKind::kV, 0};
  if (text == "Z" || text == "z")
    return DeviceAddress{DeviceKind::kZ, 0};
  if (text.size() >= 5 && (text.front() == 'U' || text.front() == 'u')) {
    const size_t separator = text.find('\\');
    if (separator == std::string_view::npos || separator + 2 >= text.size() ||
        (text[separator + 1] != 'G' && text[separator + 1] != 'g'))
      return std::nullopt;
    uint32_t unit = 0;
    uint32_t address = 0;
    const auto [unit_end, unit_status] =
        std::from_chars(text.data() + 1, text.data() + separator, unit);
    const auto [address_end, address_status] = std::from_chars(
        text.data() + separator + 2, text.data() + text.size(), address);
    if (unit_status != std::errc{} || unit_end != text.data() + separator ||
        unit >= 8 || address_status != std::errc{} ||
        address_end != text.data() + text.size() || address >= 32767)
      return std::nullopt;
    DeviceAddress device{DeviceKind::kBufferMemory, address};
    device.unit = static_cast<uint8_t>(unit);
    return device;
  }
  const size_t dot = text.find('.');
  if (dot != std::string_view::npos) {
    const auto device = ParseDeviceAddress(text.substr(0, dot));
    uint32_t bit = 0;
    const auto [end, status] = std::from_chars(
        text.data() + dot + 1, text.data() + text.size(), bit, 16);
    if (!device || device->kind != DeviceKind::kD || device->bit >= 0 ||
        status != std::errc{} || end != text.data() + text.size() || bit >= 16)
      return std::nullopt;
    auto selected = *device;
    selected.bit = static_cast<int8_t>(bit);
    return selected;
  }

  if (text.size() < 2)
    return std::nullopt;
  const char prefix =
      static_cast<char>(std::toupper(static_cast<unsigned char>(text.front())));
  DeviceKind kind;
  uint32_t limit;
  switch (prefix) {
    case 'X':
      kind = DeviceKind::kX;
      limit = 256;
      break;
    case 'Y':
      kind = DeviceKind::kY;
      limit = 256;
      break;
    case 'M':
      kind = DeviceKind::kM;
      limit = 8512;
      break;
    case 'S':
      kind = DeviceKind::kS;
      limit = 4096;
      break;
    case 'T':
      kind = DeviceKind::kT;
      limit = 512;
      break;
    case 'C':
      kind = DeviceKind::kC;
      limit = 256;
      break;
    case 'R':
      kind = DeviceKind::kR;
      limit = 32768;
      break;
    case 'V':
      kind = DeviceKind::kV;
      limit = 8;
      break;
    case 'Z':
      kind = DeviceKind::kZ;
      limit = 8;
      break;
    case 'D':
      kind = DeviceKind::kD;
      limit = 8512;
      break;
    default:
      return std::nullopt;
  }
  uint32_t index = 0;
  const int base = prefix == 'X' || prefix == 'Y' ? 8 : 10;
  const auto [end, status] =
      std::from_chars(text.data() + 1, text.data() + text.size(), index, base);
  if (status != std::errc{} || end != text.data() + text.size() ||
      index >= limit)
    return std::nullopt;
  if (kind == DeviceKind::kM) {
    if (index >= 8000) {
      kind = DeviceKind::kSpecialM;
      index -= 8000;
    } else if (index >= 7680) {
      return std::nullopt;
    }
  }
  return DeviceAddress{kind, index};
}

std::string FormatDeviceAddress(DeviceAddress address) {
  if (address.index_kind) {
    const char kind = address.index_kind;
    const int number = address.index_register;
    address.index_kind = 0;
    address.index_register = 0;
    return FormatDeviceAddress(address) + kind + std::to_string(number);
  }
  if (address.kind == DeviceKind::kBufferMemory)
    return "U" + std::to_string(address.unit) + "\\G" +
           std::to_string(address.index);
  if (address.bit >= 0) {
    const int bit = address.bit;
    address.bit = -1;
    if (address.kind != DeviceKind::kD || bit > 15)
      return {};
    return FormatDeviceAddress(address) + "." + "0123456789ABCDEF"[bit];
  }
  constexpr char kPrefixes[] = {'X', 'Y', 'M', 'S', 'T', 'C',
                                'D', 'M', 'R', 'V', 'Z', 'U'};
  if (static_cast<size_t>(address.kind) >= std::size(kPrefixes))
    return {};
  char buffer[16];
  uint32_t index = address.index;
  if (address.kind == DeviceKind::kSpecialM)
    index += 8000;
  const int base =
      address.kind == DeviceKind::kX || address.kind == DeviceKind::kY ? 8 : 10;
  const auto [end, status] =
      std::to_chars(buffer, buffer + sizeof(buffer), index, base);
  if (status != std::errc{})
    return {};
  return std::string(1, kPrefixes[static_cast<size_t>(address.kind)]) +
         std::string(buffer, end);
}

const std::string& FormatIOAddress(char kind, int canonical_index) {
  static const std::array<std::string, 512> kAddresses = [] {
    std::array<std::string, 512> result;
    for (uint32_t i = 0; i < 256; ++i) {
      result[i] = FormatDeviceAddress({DeviceKind::kX, i});
      result[i + 256] = FormatDeviceAddress({DeviceKind::kY, i});
    }
    return result;
  }();
  static const std::string kEmpty;
  if ((kind != 'X' && kind != 'Y') || canonical_index < 0 ||
      canonical_index >= 256) {
    return kEmpty;
  }
  return kAddresses[canonical_index + (kind == 'Y' ? 256 : 0)];
}

std::optional<Operand> ParseOperand(std::string_view text) {
  if (text.empty())
    return std::nullopt;
  if (text.front() == '"') {
    if (text.size() < 2 || text.back() != '"')
      return std::nullopt;
    Operand operand;
    operand.kind = OperandKind::kImmediateString;
    for (size_t index = 1; index + 1 < text.size(); ++index) {
      if (text[index] == '"') {
        if (index + 2 >= text.size() || text[index + 1] != '"')
          return std::nullopt;
        ++index;
      }
      if (text[index] == '\0')
        return std::nullopt;
      operand.text += text[index];
    }
    return operand.text.size() <= 32 ? std::optional(operand) : std::nullopt;
  }
  if (const auto address = ParseDeviceAddress(text)) {
    return Operand{IsRegisterDevice(*address) ? OperandKind::kWordDevice
                                              : OperandKind::kBitDevice,
                   *address, 0};
  }
  const std::string upper = Upper(text);
  const size_t modifier = upper.find_last_of("VZ");
  if (modifier != std::string::npos && modifier > 0 &&
      (upper.front() == 'K' || upper.front() == 'H' || upper.front() == 'P') &&
      upper.find_first_of("XYMS") == std::string::npos) {
    auto operand = ParseOperand(text.substr(0, modifier));
    uint32_t number = 0;
    const auto suffix = text.substr(modifier + 1);
    const auto [end, status] =
        std::from_chars(suffix.data(), suffix.data() + suffix.size(), number);
    if (!operand || operand->device.index_kind || number >= 8 ||
        (!suffix.empty() &&
         (status != std::errc{} || end != suffix.data() + suffix.size())))
      return std::nullopt;
    operand->device.index_kind = upper[modifier];
    operand->device.index_register = static_cast<uint8_t>(number);
    return operand;
  }
  if (upper.front() == 'K' && upper.size() >= 4) {
    const size_t suffix = upper.find_first_of("XYMS", 1);
    if (suffix != std::string::npos) {
      int32_t digits = 0;
      const auto [end, status] =
          std::from_chars(upper.data() + 1, upper.data() + suffix, digits);
      const auto device = ParseDeviceAddress(upper.substr(suffix));
      if (status != std::errc{} || end != upper.data() + suffix || digits < 1 ||
          digits > 8 || !device)
        return std::nullopt;
      auto last = *device;
      last.index += digits * 4 - 1;
      if (ParseDeviceAddress(FormatDeviceAddress(last)) != std::optional(last))
        return std::nullopt;
      return Operand{OperandKind::kPackedBit, *device, digits};
    }
  }
  if (upper.front() == 'N') {
    int32_t level = 0;
    const auto [end, status] =
        std::from_chars(upper.data() + 1, upper.data() + upper.size(), level);
    if (status != std::errc{} || end != upper.data() + upper.size() ||
        level < 0 || level > 7)
      return std::nullopt;
    return Operand{OperandKind::kNesting, {}, level};
  }
  if (upper.front() == 'P' || upper.front() == 'I') {
    int32_t pointer = 0;
    const auto [end, status] =
        std::from_chars(upper.data() + 1, upper.data() + upper.size(), pointer);
    if (status != std::errc{} || end != upper.data() + upper.size() ||
        pointer < 0 || pointer > (upper.front() == 'P' ? 4095 : 899))
      return std::nullopt;
    return Operand{upper.front() == 'P' ? OperandKind::kPointer
                                        : OperandKind::kInterruptPointer,
                   {},
                   pointer};
  }
  if (upper.front() == 'E') {
    std::string digits = upper.substr(1);
    const size_t exponent = digits.find_first_of("+-", 1);
    if (exponent != std::string::npos && digits.find('E') == std::string::npos)
      digits.insert(exponent, "E");
    float value = 0;
    const auto [end, status] =
        std::from_chars(digits.data(), digits.data() + digits.size(), value);
    if (status != std::errc{} || end != digits.data() + digits.size() ||
        !std::isfinite(value) ||
        (value != 0 && std::abs(value) < std::numeric_limits<float>::min()))
      return std::nullopt;
    return Operand{
        OperandKind::kImmediateFloat, {}, std::bit_cast<int32_t>(value)};
  }
  const bool hex = upper.front() == 'H';
  std::string_view digits(upper);
  if (hex || upper.front() == 'K')
    digits.remove_prefix(1);
  if (digits.empty())
    return std::nullopt;
  int64_t value = 0;
  const auto [end, status] = std::from_chars(
      digits.data(), digits.data() + digits.size(), value, hex ? 16 : 10);
  if (status != std::errc{} || end != digits.data() + digits.size() ||
      value < INT32_MIN || value > (hex ? UINT32_MAX : INT32_MAX)) {
    return std::nullopt;
  }
  return Operand{
      hex ? OperandKind::kImmediateHex : OperandKind::kImmediateDecimal,
      {},
      static_cast<int32_t>(static_cast<uint32_t>(value))};
}

std::string FormatOperand(const Operand& operand) {
  if (operand.device.index_kind) {
    auto base = operand;
    base.device.index_kind = 0;
    base.device.index_register = 0;
    return FormatOperand(base) + operand.device.index_kind +
           std::to_string(operand.device.index_register);
  }
  if (operand.kind == OperandKind::kPackedBit)
    return "K" + std::to_string(operand.immediate) +
           FormatDeviceAddress(operand.device);
  if (operand.kind == OperandKind::kNesting)
    return "N" + std::to_string(operand.immediate);
  if (operand.kind == OperandKind::kImmediateString) {
    std::string result = "\"";
    for (char character : operand.text) {
      if (character == '"')
        result += '"';
      result += character;
    }
    return result + '"';
  }
  if (operand.kind == OperandKind::kImmediateFloat) {
    char buffer[48];
    const float value = std::bit_cast<float>(operand.immediate);
    const auto [end, status] = std::to_chars(
        buffer, buffer + sizeof(buffer), value, std::chars_format::general,
        std::numeric_limits<float>::max_digits10);
    if (status != std::errc{})
      return {};
    std::string digits(buffer, end);
    const size_t exponent = digits.find('e');
    if (exponent != std::string::npos)
      digits.erase(exponent, 1);
    return "E" + digits;
  }
  if (operand.kind == OperandKind::kPointer)
    return "P" + std::to_string(operand.immediate);
  if (operand.kind == OperandKind::kInterruptPointer) {
    if (operand.immediate < 0 || operand.immediate > 899)
      return {};
    std::string digits = std::to_string(operand.immediate);
    return "I" + std::string(3 - digits.size(), '0') + digits;
  }
  if (operand.kind == OperandKind::kBitDevice ||
      operand.kind == OperandKind::kWordDevice) {
    return FormatDeviceAddress(operand.device);
  }
  if (operand.kind == OperandKind::kImmediateDecimal) {
    return "K" + std::to_string(operand.immediate);
  }
  char buffer[16];
  const auto [end, status] =
      std::to_chars(buffer, buffer + sizeof(buffer),
                    static_cast<uint32_t>(operand.immediate), 16);
  return status == std::errc{} ? "H" + Upper(std::string_view(buffer, end))
                               : "";
}

std::span<const InstructionDef> GetInstructionDefinitions() {
  return kDefinitions;
}

const InstructionDef* FindInstruction(std::string_view mnemonic) {
  const std::string upper = Upper(mnemonic);
  for (const InstructionDef& definition : kDefinitions) {
    if (definition.mnemonic == upper)
      return &definition;
  }
  return nullptr;
}

bool IsIndependentInstruction(Opcode opcode) {
  return opcode == Opcode::kStep || opcode == Opcode::kStepEnd ||
         opcode == Opcode::kLabel || opcode == Opcode::kReturn ||
         opcode == Opcode::kInterruptReturn ||
         opcode == Opcode::kEnableInterrupts ||
         opcode == Opcode::kDisableInterrupts || opcode == Opcode::kFend ||
         opcode == Opcode::kFor || opcode == Opcode::kNext ||
         opcode == Opcode::kMasterReset || opcode == Opcode::kNop;
}

bool BuildFlowTargets(const ExecutionProgram& program,
                      std::vector<size_t>* targets, std::string* error) {
  if (!targets)
    return false;
  const auto fail = [error](const char* message) {
    if (error)
      *error = message;
    return false;
  };
  const size_t end = program.instructions.size();
  std::vector<size_t> candidate(end, end);
  std::map<std::pair<OperandKind, int32_t>, size_t> labels;
  std::vector<size_t> loops;
  size_t fend = end;
  int master_level = -1;
  bool in_step = false;
  std::array<bool, 4096> used_states{};
  int initial_state_count = 0;
  int consecutive_steps = 0;
  for (size_t index = 0; index < end; ++index) {
    const auto& instruction = program.instructions[index];
    if (instruction.opcode == Opcode::kInitialState &&
        ++initial_state_count > 1)
      return fail("Only one IST instruction is permitted");
    if (instruction.opcode == Opcode::kStep) {
      if (++consecutive_steps > 8)
        return fail("STL parallel recombination exceeds eight states");
      const auto state = instruction.operands[0].device.index;
      if (state >= used_states.size() || used_states[state] || fend != end)
        return fail("Duplicate STL state or STL inside a routine");
      used_states[state] = true;
      in_step = true;
    } else if (instruction.opcode == Opcode::kStepEnd) {
      if (!in_step)
        return fail("RET without an STL block");
      in_step = false;
    } else if (in_step && (instruction.opcode == Opcode::kMasterControl ||
                           instruction.opcode == Opcode::kMasterReset ||
                           instruction.opcode == Opcode::kLabel ||
                           instruction.opcode == Opcode::kFend)) {
      return fail("STL block contains an invalid routine or MC boundary");
    }
    if (instruction.opcode != Opcode::kStep)
      consecutive_steps = 0;
    if (instruction.opcode == Opcode::kMasterControl) {
      const int level = instruction.operands[0].immediate;
      if (level != master_level + 1)
        return fail("MC nesting levels must increase in order");
      master_level = level;
    } else if (instruction.opcode == Opcode::kMasterReset) {
      const int level = instruction.operands[0].immediate;
      if (level > master_level)
        return fail("MCR without matching MC");
      master_level = level - 1;
    } else if (instruction.opcode == Opcode::kFend && master_level >= 0) {
      return fail("MC section must end with MCR before FEND");
    }
    if (instruction.opcode == Opcode::kFend && fend == end)
      fend = index;
    if (instruction.opcode == Opcode::kLabel) {
      if (!loops.empty())
        return fail("FOR/NEXT crosses a label");
      const auto& pointer = instruction.operands[0];
      if (pointer.device.index_kind)
        return fail("Program labels cannot be indexed");
      if (pointer.kind == OperandKind::kPointer && pointer.immediate == 63)
        return fail("P63 is reserved for CJ to END");
      if (!labels.emplace(std::pair{pointer.kind, pointer.immediate}, index)
               .second)
        return fail("Duplicate program label");
    } else if (instruction.opcode == Opcode::kFor) {
      if (loops.size() == 5)
        return fail("FOR/NEXT exceeds five nesting levels");
      loops.push_back(index);
    } else if (instruction.opcode == Opcode::kNext) {
      if (loops.empty())
        return fail("NEXT without FOR");
      candidate[index] = loops.back();
      candidate[loops.back()] = index;
      loops.pop_back();
    } else if ((instruction.opcode == Opcode::kFend ||
                instruction.opcode == Opcode::kReturn ||
                instruction.opcode == Opcode::kInterruptReturn ||
                instruction.opcode == Opcode::kLabel) &&
               !loops.empty()) {
      return fail("FOR/NEXT crosses a routine boundary");
    }
  }
  if (!loops.empty())
    return fail("FOR without NEXT");
  if (in_step)
    return fail("STL block must end with RET");
  if (master_level >= 0)
    return fail("MC without MCR");
  std::map<int32_t, Opcode> pointer_uses;
  for (size_t index = 0; index < end; ++index) {
    const auto& instruction = program.instructions[index];
    if (instruction.opcode != Opcode::kJump &&
        instruction.opcode != Opcode::kCall)
      continue;
    const auto& pointer = instruction.operands[0];
    if (pointer.device.index_kind)
      continue;
    if (instruction.opcode == Opcode::kCall && pointer.immediate == 63)
      return fail("CALL cannot use reserved pointer P63");
    const auto found = labels.find({OperandKind::kPointer, pointer.immediate});
    // P63 is the documented implicit END destination of CJ.
    if (instruction.opcode == Opcode::kJump && pointer.immediate == 63 &&
        found == labels.end())
      continue;
    if (found == labels.end())
      return fail("Undefined program pointer");
    const auto [use, inserted] =
        pointer_uses.emplace(pointer.immediate, instruction.opcode);
    if (!inserted && use->second != instruction.opcode)
      return fail("CJ and CALL cannot share a pointer");
    if (instruction.opcode == Opcode::kCall && found->second <= fend)
      return fail("Subroutine label must follow FEND");
    if (instruction.opcode == Opcode::kJump && found->second > fend)
      return fail("CJ cannot enter a subroutine after FEND");
    candidate[index] = found->second;
  }
  for (const auto& [pointer, index] : labels) {
    if (pointer.first == OperandKind::kInterruptPointer && index <= fend)
      return fail("Interrupt label must follow FEND");
  }
  *targets = std::move(candidate);
  return true;
}

bool ValidateOperands(const InstructionDef& definition,
                      std::span<const Operand> operands, std::string* error) {
  if (operands.size() != definition.operand_count) {
    if (error)
      *error = "Wrong operand count for " + std::string(definition.mnemonic);
    return false;
  }
  for (size_t i = 0; i < operands.size(); ++i) {
    const Operand& operand = operands[i];
    if (operand.kind < OperandKind::kBitDevice ||
        operand.kind > OperandKind::kPackedBit) {
      if (error)
        *error = "Invalid operand kind";
      return false;
    }
    const bool device = operand.kind == OperandKind::kBitDevice ||
                        operand.kind == OperandKind::kWordDevice ||
                        operand.kind == OperandKind::kPackedBit;
    if ((device && ParseDeviceAddress(FormatDeviceAddress(operand.device)) !=
                       std::optional(operand.device)) ||
        (device &&
         ParseOperand(FormatOperand(operand)) != std::optional(operand)) ||
        !Matches(definition.operand_rules[i], operand) ||
        ParseOperand(FormatOperand(operand)) != std::optional(operand)) {
      if (error)
        *error = "Invalid operand " + std::to_string(i + 1) + " for " +
                 std::string(definition.mnemonic);
      return false;
    }
  }
  return true;
}

bool ValidateProgram(const ExecutionProgram& program, std::string* error,
                     bool validate_flow) {
  if (program.instructions.empty()) {
    if (error)
      *error = "Program has no outputs";
    return false;
  }
  const OpenPLCInstruction* previous = nullptr;
  int sort_count = 0;
  int print_count = 0;
  int sort2_count = 0;
  int high_speed_count = 0;
  int high_speed_tables = 0;
  for (const OpenPLCInstruction& instruction : program.instructions) {
    if (instruction.opcode >= Opcode::kHighSpeedSet &&
        instruction.opcode <= Opcode::kHighSpeedTable) {
      if (++high_speed_count > 32 ||
          (instruction.opcode == Opcode::kHighSpeedTable &&
           ++high_speed_tables > 1)) {
        if (error)
          *error = "High-speed comparison instruction limit exceeded";
        return false;
      }
    }
    if (previous && instruction.evaluation_group >= 0 &&
        instruction.evaluation_group == previous->evaluation_group &&
        (instruction.condition.size() < previous->condition.size() ||
         !std::equal(previous->condition.begin(), previous->condition.end(),
                     instruction.condition.begin()))) {
      if (error)
        *error = "Inconsistent saved branch topology";
      return false;
    }
    previous = &instruction;
    const InstructionDef* definition = nullptr;
    for (const InstructionDef& entry : kDefinitions) {
      if (entry.opcode == instruction.opcode)
        definition = &entry;
    }
    if (!definition || instruction.operand_count > kMaxOperands ||
        !ValidateOperands(
            *definition,
            {instruction.operands.data(), instruction.operand_count}, error)) {
      return false;
    }
    if (instruction.condition.empty()) {
      if (error)
        *error = "Missing output condition";
      return false;
    }
    for (const auto& observation : instruction.observations) {
      if (observation.rung < 0 || observation.cell < 0 ||
          observation.gate >= instruction.condition.size() ||
          std::any_of(
              observation.incoming_gates.begin(),
              observation.incoming_gates.end(),
              [&instruction](uint32_t gate) {
                return gate >= instruction.condition.size();
              })) {
        if (error)
          *error = "Invalid editor observation";
        return false;
      }
    }
    if (instruction.condition_begin >= instruction.condition.size() ||
        (instruction.condition_root != UINT32_MAX &&
         (instruction.condition_root < instruction.condition_begin ||
          instruction.condition_root >= instruction.condition.size()))) {
      if (error)
        *error = "Invalid condition root";
      return false;
    }
    if (((instruction.pulse || instruction.wide) && definition->fnc_no == 0 &&
         instruction.opcode != Opcode::kJump) ||
        (instruction.pulse && !definition->supports_pulse)) {
      if (error)
        *error = "Unsupported pulse/word variant";
      return false;
    }
    if (instruction.wide && (instruction.opcode == Opcode::kBlockMove ||
                             instruction.opcode == Opcode::kFillMove ||
                             instruction.opcode == Opcode::kZeroReset ||
                             instruction.opcode == Opcode::kAlternate ||
                             instruction.opcode == Opcode::kDecode ||
                             instruction.opcode == Opcode::kEncode ||
                             (instruction.opcode >= Opcode::kBitShiftRight &&
                              instruction.opcode <= Opcode::kWordShiftLeft))) {
      if (error)
        *error = "Unsupported double-word variant";
      return false;
    }
    if (instruction.opcode >= Opcode::kRotateRight &&
        instruction.opcode <= Opcode::kBitTest) {
      const bool rotate = instruction.opcode <= Opcode::kRotateCarryLeft;
      const bool shifted = instruction.opcode >= Opcode::kBitShiftRight &&
                           instruction.opcode <= Opcode::kWordShiftLeft;
      const size_t count_index = rotate ? 1 : 2;
      const auto immediate = [](const Operand& operand) {
        return !operand.device.index_kind &&
               (operand.kind == OperandKind::kImmediateDecimal ||
                operand.kind == OperandKind::kImmediateHex);
      };
      if (instruction.opcode != Opcode::kSumBits &&
          immediate(instruction.operands[count_index]) &&
          (!shifted || immediate(instruction.operands[3])) &&
          !ValidateDataOperation(
              instruction, instruction.operands[count_index].immediate,
              shifted ? instruction.operands[3].immediate : 0, error))
        return false;
    }
    if (instruction.opcode == Opcode::kPrint && ++print_count > 2) {
      if (error)
        *error = "At most two PR instructions are allowed";
      return false;
    }
    if (!ValidateExtendedOperands(instruction, error))
      return false;
    if (definition->requires_wide && !instruction.wide) {
      if (error)
        *error = "Instruction requires D prefix (32-bit operation)";
      return false;
    }
    if (instruction.wide && !definition->supports_wide) {
      if (error)
        *error = "Unsupported double-word variant";
      return false;
    }
    if (instruction.opcode == Opcode::kTableSort2 && ++sort2_count > 2) {
      if (error)
        *error = "At most two SORT2 instructions are allowed";
      return false;
    }
    if (instruction.opcode >= Opcode::kTableSearch &&
        instruction.opcode <= Opcode::kTableSort) {
      const auto& count =
          instruction.operands[TableCountOperand(instruction.opcode)];
      const bool immediate = !count.device.index_kind &&
                             (count.kind == OperandKind::kImmediateDecimal ||
                              count.kind == OperandKind::kImmediateHex);
      if (!ValidateTableOperation(
              instruction,
              immediate ? std::optional(count.immediate) : std::nullopt, error))
        return false;
      if (instruction.opcode == Opcode::kTableSort && ++sort_count > 1) {
        if (error)
          *error = "Only one SORT instruction is allowed";
        return false;
      }
    }
    if (instruction.opcode >= Opcode::kComplement &&
        instruction.opcode <= Opcode::kWriteClock) {
      const auto& count = instruction.operands[2];
      const bool constant = !count.device.index_kind &&
                            (count.kind == OperandKind::kImmediateDecimal ||
                             count.kind == OperandKind::kImmediateHex);
      if (!ValidateCpuOperation(
              instruction,
              constant ? std::optional(count.immediate) : std::nullopt, error))
        return false;
    }
    if ((instruction.opcode == Opcode::kOut ||
         instruction.opcode == Opcode::kSet ||
         instruction.opcode == Opcode::kAlternate) &&
        (instruction.operands[0].device.kind == DeviceKind::kT ||
         instruction.operands[0].device.kind == DeviceKind::kC)) {
      if (error)
        *error = "Timer/counter OUT requires a preset";
      return false;
    }
    if (instruction.opcode == Opcode::kCompare ||
        instruction.opcode == Opcode::kZoneCompare ||
        instruction.opcode == Opcode::kFloatCompare ||
        instruction.opcode == Opcode::kFloatZoneCompare ||
        instruction.opcode == Opcode::kTimeCompare ||
        instruction.opcode == Opcode::kTimeZoneCompare) {
      DeviceAddress last =
          instruction.operands[instruction.operand_count - 1].device;
      last = OffsetBitAddress(last, 2);
      if (last.kind == DeviceKind::kT || last.kind == DeviceKind::kC ||
          ParseDeviceAddress(FormatDeviceAddress(last)) !=
              std::optional(last)) {
        if (error)
          *error = "Comparison result exceeds writable bit range";
        return false;
      }
    }
    if (instruction.opcode == Opcode::kZeroReset &&
        (instruction.operands[0].device.kind !=
             instruction.operands[1].device.kind ||
         instruction.operands[0].device.index >
             instruction.operands[1].device.index)) {
      if (error)
        *error = "Invalid reset range";
      return false;
    }
    if ((instruction.opcode == Opcode::kTimerOn ||
         instruction.opcode == Opcode::kCounterUp) &&
        !instruction.operands[1].device.index_kind &&
        (instruction.operands[1].kind == OperandKind::kImmediateDecimal ||
         instruction.operands[1].kind == OperandKind::kImmediateHex) &&
        (instruction.operands[1].immediate < 0 &&
         (instruction.opcode != Opcode::kCounterUp ||
          instruction.operands[0].device.index < 200))) {
      if (error)
        *error = "Preset must be nonnegative";
      return false;
    }
    if ((instruction.opcode == Opcode::kBlockMove ||
         instruction.opcode == Opcode::kFillMove) &&
        !instruction.operands[2].device.index_kind &&
        (instruction.operands[2].kind == OperandKind::kImmediateDecimal ||
         instruction.operands[2].kind == OperandKind::kImmediateHex)) {
      const int32_t count = instruction.operands[2].immediate;
      if (count < 0 || count > 32767 ||
          instruction.operands[1].device.index + count >
              WordDeviceLimit(instruction.operands[1].device) ||
          (instruction.opcode == Opcode::kBlockMove &&
           instruction.operands[0].device.index + count >
               WordDeviceLimit(instruction.operands[0].device))) {
        if (error)
          *error = "Block operation exceeds D memory";
        return false;
      }
    }
    for (size_t i = 0; i < instruction.operand_count; ++i) {
      const auto& operand = instruction.operands[i];
      const OperandRule rule = definition->operand_rules[i];
      const bool numeric = rule == Rule::kValue || rule == Rule::kWritableValue;
      if (!numeric || operand.kind != OperandKind::kBitDevice)
        continue;
      const auto device = operand.device;
      const bool counter32 =
          device.kind == DeviceKind::kC && device.index >= 200;
      if (counter32 && !instruction.wide) {
        if (error)
          *error = "32-bit counter requires a double-word instruction";
        return false;
      }
      if (instruction.wide &&
          ((device.kind == DeviceKind::kT && device.index == 511) ||
           (device.kind == DeviceKind::kC && device.index == 199))) {
        if (error)
          *error = "Double-word operation exceeds timer/counter region";
        return false;
      }
    }
    for (size_t i = 0; i < instruction.operand_count; ++i) {
      const Operand& operand = instruction.operands[i];
      if (operand.kind == OperandKind::kPackedBit && !instruction.wide &&
          operand.immediate > 4) {
        if (error)
          *error = "16-bit operation supports K1 to K4 packed bits";
        return false;
      }
      if (!IsRegisterDevice(operand.device) ||
          operand.kind != OperandKind::kWordDevice)
        continue;
      uint32_t words = instruction.wide ? 2 : 1;
      if (instruction.opcode == Opcode::kToFloat && i == 1)
        words = 2;
      if (instruction.opcode == Opcode::kFromFloat && i == 0)
        words = 2;
      if ((instruction.opcode == Opcode::kIntegerToString && i == 0) ||
          (instruction.opcode == Opcode::kStringToInteger && i == 1) ||
          ((instruction.opcode == Opcode::kStringRead ||
            instruction.opcode == Opcode::kStringWrite) &&
           i == 2))
        words = 2;
      if (instruction.opcode == Opcode::kFloatToString && i == 1)
        words = 3;
      if (i + 1 == instruction.operand_count &&
          (instruction.opcode == Opcode::kMul ||
           instruction.opcode == Opcode::kDiv))
        words *= 2;
      if (instruction.wide && (operand.device.kind == DeviceKind::kZ ||
                               operand.device.kind == DeviceKind::kV))
        words = 1;
      if (operand.device.index + words > WordDeviceLimit(operand.device)) {
        if (error)
          *error = "Instruction exceeds D register range";
        return false;
      }
    }
    for (size_t i = 0; i < instruction.condition.size(); ++i) {
      const LogicGate& gate = instruction.condition[i];
      const bool unary = gate.kind == GateKind::kNot ||
                         gate.kind == GateKind::kRising ||
                         gate.kind == GateKind::kFalling;
      const bool binary =
          gate.kind == GateKind::kAnd || gate.kind == GateKind::kOr;
      if (((unary || binary) && gate.left >= i) ||
          (binary && gate.right >= i)) {
        if (error)
          *error = "Invalid or cyclic logic topology";
        return false;
      }
      if (gate.kind >= GateKind::kEqual && gate.kind <= GateKind::kLessEqual) {
        constexpr InstructionDef kComparison{
            Opcode::kCompare, "comparison", 0, 2, {Rule::kValue, Rule::kValue}};
        const std::array<Operand, 2> values{gate.first, gate.second};
        if (!ValidateOperands(kComparison, values, error))
          return false;
        for (const Operand& operand : values) {
          if (!gate.wide && operand.kind == OperandKind::kPackedBit &&
              operand.immediate > 4) {
            if (error)
              *error = "16-bit comparison supports K1 to K4 packed bits";
            return false;
          }
        }
        if (gate.wide) {
          for (const Operand& operand : values) {
            if (operand.kind == OperandKind::kWordDevice &&
                operand.device.index +
                        ((operand.device.kind == DeviceKind::kZ ||
                          operand.device.kind == DeviceKind::kV)
                             ? 1
                             : 2) >
                    WordDeviceLimit(operand.device)) {
              if (error)
                *error = "Comparison exceeds double-word device range";
              return false;
            }
          }
        }
      }
      if (gate.kind < GateKind::kConstant || gate.kind > GateKind::kLessEqual) {
        if (error)
          *error = "Invalid logic gate kind";
        return false;
      }
      if (gate.kind == GateKind::kContact &&
          (gate.first.kind != OperandKind::kBitDevice ||
           !Matches(Rule::kBit, gate.first) ||
           ParseDeviceAddress(FormatDeviceAddress(gate.first.device)) !=
               std::optional(gate.first.device))) {
        if (error)
          *error = "Invalid contact device";
        return false;
      }
    }
  }
  std::vector<size_t> targets;
  return !validate_flow || BuildFlowTargets(program, &targets, error);
}
bool ValidateDataOperation(const OpenPLCInstruction& instruction, int32_t count,
                           int32_t shift, std::string* error) {
  const auto fail = [error](const char* message) {
    if (error)
      *error = message;
    return false;
  };
  const auto range = [](DeviceAddress start, uint32_t length) {
    if (length == 0)
      return true;
    if (start.kind == DeviceKind::kD && start.index < 8000 &&
        start.index + length > 8000)
      return false;
    start.index += length - 1;
    return ParseDeviceAddress(FormatDeviceAddress(start)) ==
           std::optional(start);
  };
  const auto& operands = instruction.operands;
  const Opcode opcode = instruction.opcode;
  if (opcode >= Opcode::kRotateRight && opcode <= Opcode::kRotateCarryLeft) {
    return (count >= 0 && count <= (instruction.wide ? 32 : 16)) ||
           fail("Rotation count exceeds word width");
  }
  if (opcode == Opcode::kBitTest) {
    return (count >= 0 && count < (instruction.wide ? 32 : 16)) ||
           fail("BON bit number exceeds word width");
  }
  if (opcode == Opcode::kDecode || opcode == Opcode::kEncode) {
    const DeviceAddress data =
        operands[opcode == Opcode::kDecode ? 1 : 0].device;
    const int limit = IsRegisterDevice(data) ? 4 : 8;
    if (count < 0 || count > limit)
      return fail("DECO/ENCO bit count exceeds device width");
    if (count == 0)
      return true;
    if (!IsRegisterDevice(data) && !range(data, 1u << count))
      return fail("DECO/ENCO exceeds bit device range");
    if (opcode == Opcode::kDecode &&
        operands[0].kind == OperandKind::kBitDevice &&
        !range(operands[0].device, static_cast<uint32_t>(count)))
      return fail("DECO source exceeds bit device range");
    return true;
  }
  if (opcode >= Opcode::kBitShiftRight && opcode <= Opcode::kWordShiftLeft) {
    const bool word = opcode >= Opcode::kWordShiftRight;
    if (count < 0 || count > (word ? 512 : 1024) || shift < 0 || shift > count)
      return fail("Invalid shift register length or shift count");
    if (operands[0].device == operands[1].device)
      return fail("Shift source and destination must differ on FX3U");
    if (!range(operands[0].device, static_cast<uint32_t>(shift)) ||
        !range(operands[1].device, static_cast<uint32_t>(count)))
      return fail("Shift operation exceeds device range");
  }
  return true;
}

bool ValidateCpuOperation(const OpenPLCInstruction& instruction,
                          std::optional<int32_t> count_value,
                          std::string* error) {
  const auto fail = [error](const char* message) {
    if (error)
      *error = message;
    return false;
  };
  const auto& operands = instruction.operands;
  const Opcode opcode = instruction.opcode;
  if (opcode == Opcode::kReadClock || opcode == Opcode::kWriteClock)
    return operands[0].device.index + 7 <=
               WordDeviceLimit(operands[0].device) ||
           fail("RTC data exceeds D register range");
  if (opcode == Opcode::kDigitMove) {
    const int32_t source_digit = operands[1].immediate;
    const int32_t digits = operands[2].immediate;
    const int32_t destination_digit = operands[4].immediate;
    return (digits >= 1 && digits <= 4 && source_digit >= digits &&
            source_digit <= 4 && destination_digit >= digits &&
            destination_digit <= 4) ||
           fail("Invalid SMOV decimal digit range");
  }
  if (!count_value)
    return true;  // D count is checked before runtime writes.
  const int32_t count = *count_value;
  if (opcode == Opcode::kMean) {
    return (count >= 1 && count <= 64 &&
            operands[0].device.index + count * (instruction.wide ? 2 : 1) <=
                WordDeviceLimit(operands[0].device)) ||
           fail("MEAN count exceeds source register range");
  }
  if (opcode == Opcode::kWordToBytes || opcode == Opcode::kBytesToWord) {
    if (count < 0 || count > 32767)
      return fail("Invalid byte conversion count");
    const uint32_t words = static_cast<uint32_t>((count + 1) / 2);
    const bool separate = opcode == Opcode::kWordToBytes;
    return (operands[0].device.index + (separate ? words : count) <=
                WordDeviceLimit(operands[0].device) &&
            operands[1].device.index + (separate ? count : words) <=
                WordDeviceLimit(operands[1].device)) ||
           fail("Byte conversion exceeds D register range");
  }
  if (opcode == Opcode::kShiftWrite || opcode == Opcode::kShiftRead) {
    const size_t head = opcode == Opcode::kShiftWrite ? 1 : 0;
    return (count >= 2 && count <= 512 &&
            operands[head].device.index + count <=
                WordDeviceLimit(operands[head].device)) ||
           fail("FIFO size exceeds D register range");
  }
  return true;
}

}  // namespace plc_emulator::programming
