#ifndef PLC_EMULATOR_MEMORY_MANAGER_H
#define PLC_EMULATOR_MEMORY_MANAGER_H

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>

namespace plc_emulator {
namespace data {

// Memory types supported by PLC (메모리 타입)
enum class MemoryType {
    INPUT,      // Input registers (X) - 입력 레지스터
    OUTPUT,     // Output registers (Y) - 출력 레지스터
    INTERNAL,   // Internal registers (M) - 내부 레지스터
    TIMER,      // Timer registers (T) - 타이머 레지스터
    COUNTER,    // Counter registers (C) - 카운터 레지스터
    DATA        // Data registers (D) - 데이터 레지스터
};

// Memory access modes (메모리 접근 모드)
enum class AccessMode {
    READ,       // Read-only access - 읽기 전용
    WRITE,      // Write-only access - 쓰기 전용
    READ_WRITE  // Read and write access - 읽기/쓰기
};

// Memory region configuration (메모리 영역 설정)
struct MemoryRegion {
    MemoryType type;
    uint32_t startAddress;
    uint32_t size;
    AccessMode accessMode;
    std::string name;
};

/**
 * @brief Manages PLC memory regions (Input, Output, Internal, Timer, Counter, Data)
 * @details Provides centralized memory management for all PLC data types with bounds checking
 * 
 * PLC 메모리 영역(입력, 출력, 내부, 타이머, 카운터, 데이터)을 관리합니다.
 * 경계 검사를 통한 중앙화된 메모리 관리를 제공합니다.
 */
class MemoryManager {
public:
    MemoryManager();
    ~MemoryManager() = default;

    // Memory region initialization (메모리 영역 초기화)
    void InitializeRegions(const std::vector<MemoryRegion>& regions);
    void InitializeDefaultRegions();  // Standard PLC memory layout
    
    // Memory access (메모리 접근)
    bool ReadBit(MemoryType type, uint32_t address, uint8_t bit) const;
    bool WriteBit(MemoryType type, uint32_t address, uint8_t bit, bool value);
    
    uint16_t ReadWord(MemoryType type, uint32_t address) const;
    bool WriteWord(MemoryType type, uint32_t address, uint16_t value);
    
    uint32_t ReadDWord(MemoryType type, uint32_t address) const;
    bool WriteDWord(MemoryType type, uint32_t address, uint32_t value);
    
    // Bulk operations (대량 작업)
    bool ReadBytes(MemoryType type, uint32_t address, uint8_t* buffer, size_t size) const;
    bool WriteBytes(MemoryType type, uint32_t address, const uint8_t* buffer, size_t size);
    
    // Memory management (메모리 관리)
    void ClearRegion(MemoryType type);
    void ClearAll();
    void Reset();
    
    // Memory information (메모리 정보)
    uint32_t GetRegionSize(MemoryType type) const;
    uint32_t GetRegionStartAddress(MemoryType type) const;
    AccessMode GetRegionAccessMode(MemoryType type) const;
    bool IsValidAddress(MemoryType type, uint32_t address) const;
    
    // Memory statistics (메모리 통계)
    size_t GetTotalMemoryUsage() const;
    size_t GetRegionMemoryUsage(MemoryType type) const;
    
    // Debug and diagnostics (디버그 및 진단)
    std::string DumpRegion(MemoryType type, uint32_t startAddr, uint32_t count) const;
    void PrintMemoryMap() const;

private:
    // Internal storage for each memory type (각 메모리 타입의 내부 저장소)
    std::unordered_map<MemoryType, std::vector<uint8_t>> m_memoryRegions;
    std::unordered_map<MemoryType, MemoryRegion> m_regionConfig;
    
    // Helper methods (헬퍼 메소드)
    bool ValidateAccess(MemoryType type, uint32_t address, size_t size, AccessMode mode) const;
    std::vector<uint8_t>* GetRegionPointer(MemoryType type);
    const std::vector<uint8_t>* GetRegionPointer(MemoryType type) const;
    
    static std::string MemoryTypeToString(MemoryType type);
    static std::string AccessModeToString(AccessMode mode);
};

} // namespace data
} // namespace plc_emulator

#endif // PLC_EMULATOR_MEMORY_MANAGER_H
