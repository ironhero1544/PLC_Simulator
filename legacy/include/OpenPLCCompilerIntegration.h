#pragma once

#include <string>
#include <vector>
#include <memory>

namespace plc {

// 🔥 **NEW**: LadderIR 전방 선언
class LadderIRProgram;
struct LadderProgram;

/**
 * @brief OpenPLC 컴파일러를 통합하여 .ld 파일을 C++ 실행 코드로 변환하는 클래스
 * 
 * 이 클래스는 .ld 파일을 입력받아 OpenPLC 컴파일러 로직을 사용하여
 * 실행 가능한 C++ 코드를 생성합니다.
 * 
 * 주요 기능:
 * - .ld 파일 파싱 및 중간 코드 생성
 * - FX3U-32M I/O 매핑 (X0~X15, Y0~Y15)
 * - 최적화된 C++ 실행 코드 생성
 * - 런타임 메모리 관리
 */
class OpenPLCCompilerIntegration {
public:
    struct CompilationResult {
        bool success = false;
        std::string errorMessage;
        std::string generatedCode;
        std::string intermediateCode;
        int inputCount = 0;
        int outputCount = 0;
        int memoryCount = 0;
    };

    OpenPLCCompilerIntegration();
    ~OpenPLCCompilerIntegration();

    /**
     * @brief .ld 파일을 C++ 실행 코드로 컴파일
     * @param ldFilePath 입력 .ld 파일 경로
     * @return 컴파일 결과
     */
    CompilationResult CompileLDFile(const std::string& ldFilePath);

    /**
     * @brief .ld 문자열을 C++ 실행 코드로 컴파일
     * @param ldContent .ld 파일 내용
     * @return 컴파일 결과
     */
    CompilationResult CompileLDString(const std::string& ldContent);

    /**
     * @brief FX3U-32M I/O 매핑 설정
     * @param inputs 입력 개수 (기본값: 16)
     * @param outputs 출력 개수 (기본값: 16)
     */
    void SetIOConfiguration(int inputs = 16, int outputs = 16);

    /**
     * @brief 디버그 모드 설정
     * @param enable 디버그 모드 활성화 여부
     */
    void SetDebugMode(bool enable) { m_debugMode = enable; }

    /**
     * @brief 최적화 레벨 설정
     * @param level 최적화 레벨 (0: 없음, 1: 기본, 2: 고급)
     */
    void SetOptimizationLevel(int level) { m_optimizationLevel = level; }

    // 🔥 **NEW**: IR 기반 컴파일 함수들 (Phase 4)

    /**
     * @brief LadderIR을 이용한 고정밀도 컴파일
     * 병렬(OR) 회로의 정확한 컴파일을 보장합니다.
     * @param ladderProgram 원본 래더 프로그램
     * @return 컴파일 결과
     */
    CompilationResult CompileLadderProgramWithIR(const LadderProgram& ladderProgram);

    /**
     * @brief LadderIR 프로그램을 직접 컴파일
     * @param irProgram LadderIR 프로그램
     * @return 컴파일 결과
     */
    CompilationResult CompileIRProgram(const LadderIRProgram& irProgram);

    /**
     * @brief 생성된 C++ 코드를 파일로 저장
     * @param result 컴파일 결과
     * @param outputPath 출력 파일 경로
     * @return 성공 시 true
     */
    bool SaveGeneratedCode(const CompilationResult& result, const std::string& outputPath);

private:
    bool m_debugMode = false;
    int m_optimizationLevel = 1;
    int m_inputCount = 16;   // X0~X15
    int m_outputCount = 16;  // Y0~Y15
    int m_memoryCount = 1000; // M0~M999

    // OpenPLC 컴파일러 핵심 구성요소
    struct LDInstruction {
        enum Type {
            LD, LDN, AND, ANDN, OR, ORN, ST, S, R,
            TON, TOF, CTU, CTD, EQ, NE, GT, LT, GE, LE,
            ADD, SUB, MUL, DIV, MOD
        };
        
        Type type;
        std::string operand;
        std::string preset;
        int lineNumber = 0;
    };

    struct Variable {
        enum Type { BOOL_INPUT, BOOL_OUTPUT, BOOL_MEMORY, INT_MEMORY, TIMER, COUNTER };
        Type type;
        std::string name;
        std::string address;
        int index = 0;
    };

    std::vector<LDInstruction> m_instructions;
    std::vector<Variable> m_variables;
    std::string m_lastError;

    // 🔥 **NEW**: IR 기반 헬퍼 함수들
    std::string GenerateOpenPLCHeader();

    /**
     * @brief .ld 파일 내용 파싱
     * @param ldContent .ld 파일 내용
     * @return 파싱 성공 여부
     */
    bool ParseLDContent(const std::string& ldContent);

    /**
     * @brief 변수 선언부 파싱
     * @param content VAR 섹션 내용
     * @return 파싱 성공 여부
     */
    bool ParseVariableDeclarations(const std::string& content);

    /**
     * @brief 래더 명령어 파싱
     * @param content 프로그램 섹션 내용
     * @return 파싱 성공 여부
     */
    bool ParseInstructions(const std::string& content);

    /**
     * @brief 명령어를 C++ 코드로 변환
     * @param instructions 명령어 목록
     * @return 생성된 C++ 코드
     */
    std::string GenerateCPPCode(const std::vector<LDInstruction>& instructions);

    /**
     * @brief C++ 함수 헤더 생성
     * @return 함수 헤더 코드
     */
    std::string GenerateFunctionHeader();

    /**
     * @brief 변수 선언 코드 생성
     * @return 변수 선언 코드
     */
    std::string GenerateVariableDeclarations();

    /**
     * @brief I/O 매핑 코드 생성
     * @return I/O 매핑 코드
     */
    std::string GenerateIOMapping();

    /**
     * @brief 래더 로직 실행 코드 생성
     * @param instructions 명령어 목록
     * @return 실행 코드
     */
    std::string GenerateExecutionCode(const std::vector<LDInstruction>& instructions);

    /**
     * @brief 개별 명령어를 C++ 코드로 변환
     * @param instruction 명령어
     * @return C++ 코드 라인
     */
    std::string TranslateInstruction(const LDInstruction& instruction);

    /**
     * @brief 오류 메시지 설정
     * @param error 오류 메시지
     */
    void SetError(const std::string& error);

    /**
     * @brief 디버그 로그 출력
     * @param message 로그 메시지
     */
    void DebugLog(const std::string& message);

    /**
     * @brief 문자열 트림
     * @param str 입력 문자열
     * @return 트림된 문자열
     */
    std::string Trim(const std::string& str);

    /**
     * @brief 명령어 타입 문자열을 enum으로 변환
     * @param typeStr 명령어 타입 문자열
     * @return 명령어 타입
     */
    LDInstruction::Type StringToInstructionType(const std::string& typeStr);
};

} // namespace plc