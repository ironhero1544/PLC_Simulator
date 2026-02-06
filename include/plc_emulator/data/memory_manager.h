/*
 * memory_manager.h
 *
 * PLC memory regions (X, Y, M, D, T, C).
 * PLC 메모리 영역 (X, Y, M, D, T, C).
 */

#ifndef PLC_EMULATOR_MEMORY_MANAGER_H
#define PLC_EMULATOR_MEMORY_MANAGER_H

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>

namespace plc_emulator {
namespace data {

/*
 * PLC memory types: INPUT=X, OUTPUT=Y, INTERNAL=M, TIMER=T, COUNTER=C, DATA=D.
 * PLC 메모리 타입: INPUT=X, OUTPUT=Y, INTERNAL=M, TIMER=T, COUNTER=C, DATA=D.
 */
enum class MemoryType {
    INPUT,
    OUTPUT,
    INTERNAL,
    TIMER,
    COUNTER,
    DATA
};

/*
 * Memory access modes.
 * 메모리 접근 모드.
 */
enum class AccessMode {
    READ,
    WRITE,
    READ_WRITE
};

/*
 * Memory region configuration.
 * 메모리 영역 설정.
 */
struct MemoryRegion {
    MemoryType type;
    uint32_t startAddress;
    uint32_t size;
    AccessMode accessMode;
    std::string name;
};

/*
 * Manages PLC memory regions with bounds checking.
 * 경계 검사를 포함한 PLC 메모리 영역 관리자.
 */
class MemoryManager {
public:
    MemoryManager();
    ~MemoryManager() = default;

    /*
     * Region initialization.
     * 메모리 영역 초기화.
     */
    void InitializeRegions(const std::vector<MemoryRegion>& regions);
    void InitializeDefaultRegions();
    
    /*
     * Bit/word accessors.
     * 비트/워드 접근.
     */
    bool ReadBit(MemoryType type, uint32_t address, uint8_t bit) const;
    bool WriteBit(MemoryType type, uint32_t address, uint8_t bit, bool value);
    
    uint16_t ReadWord(MemoryType type, uint32_t address) const;
    bool WriteWord(MemoryType type, uint32_t address, uint16_t value);
    
    uint32_t ReadDWord(MemoryType type, uint32_t address) const;
    bool WriteDWord(MemoryType type, uint32_t address, uint32_t value);
    
    /*
     * Bulk reads and writes.
     * 대량 읽기/쓰기.
     */
    bool ReadBytes(MemoryType type, uint32_t address, uint8_t* buffer, size_t size) const;
    bool WriteBytes(MemoryType type, uint32_t address, const uint8_t* buffer, size_t size);
    
    /*
     * Memory management operations.
     * 메모리 관리 동작.
     */
    void ClearRegion(MemoryType type);
    void ClearAll();
    void Reset();
    
    /*
     * Memory region metadata.
     * 메모리 영역 메타데이터.
     */
    uint32_t GetRegionSize(MemoryType type) const;
    uint32_t GetRegionStartAddress(MemoryType type) const;
    AccessMode GetRegionAccessMode(MemoryType type) const;
    bool IsValidAddress(MemoryType type, uint32_t address) const;
    
    /*
     * Memory usage summaries.
     * 메모리 사용 요약.
     */
    size_t GetTotalMemoryUsage() const;
    size_t GetRegionMemoryUsage(MemoryType type) const;
    
    /*
     * Debug and diagnostics.
     * 디버그 및 진단.
     */
    std::string DumpRegion(MemoryType type, uint32_t startAddr, uint32_t count) const;
    void PrintMemoryMap() const;

private:
    /*
     * Internal storage and configuration.
     * 내부 저장소와 설정.
     */
    std::unordered_map<MemoryType, std::vector<uint8_t>> m_memoryRegions;
    std::unordered_map<MemoryType, MemoryRegion> m_regionConfig;
    
    /*
     * Helper methods.
     * 헬퍼 메소드.
     */
    bool ValidateAccess(MemoryType type, uint32_t address, size_t size, AccessMode mode) const;
    std::vector<uint8_t>* GetRegionPointer(MemoryType type);
    const std::vector<uint8_t>* GetRegionPointer(MemoryType type) const;
    
    static std::string MemoryTypeToString(MemoryType type);
    static std::string AccessModeToString(AccessMode mode);
};

}  /* namespace data */
}  /* namespace plc_emulator */

#endif  /* PLC_EMULATOR_MEMORY_MANAGER_H */
