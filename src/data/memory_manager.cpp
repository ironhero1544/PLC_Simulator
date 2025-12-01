// memory_manager.cpp
// Copyright 2024 PLC Emulator Project
//
// Implementation of PLC memory manager.

#include "plc_emulator/data/memory_manager.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace plc_emulator {
namespace data {

MemoryManager::MemoryManager() {
    InitializeDefaultRegions();
}

void MemoryManager::InitializeRegions(const std::vector<MemoryRegion>& regions) {
    m_memoryRegions.clear();
    m_regionConfig.clear();
    
    for (const auto& region : regions) {
        m_regionConfig[region.type] = region;
        m_memoryRegions[region.type].resize(region.size, 0);
    }
}

void MemoryManager::InitializeDefaultRegions() {
    std::vector<MemoryRegion> defaultRegions = {
        {MemoryType::INPUT, 0x0000, 1024, AccessMode::READ_WRITE, "Input (X)"},
        {MemoryType::OUTPUT, 0x0000, 1024, AccessMode::READ_WRITE, "Output (Y)"},
        {MemoryType::INTERNAL, 0x0000, 8192, AccessMode::READ_WRITE, "Internal (M)"},
        {MemoryType::TIMER, 0x0000, 512, AccessMode::READ_WRITE, "Timer (T)"},
        {MemoryType::COUNTER, 0x0000, 512, AccessMode::READ_WRITE, "Counter (C)"},
        {MemoryType::DATA, 0x0000, 8192, AccessMode::READ_WRITE, "Data (D)"}
    };
    
    InitializeRegions(defaultRegions);
}

bool MemoryManager::ReadBit(MemoryType type, uint32_t address, uint8_t bit) const {
    if (bit > 7) {
        return false;
    }
    
    if (!ValidateAccess(type, address, 1, AccessMode::READ)) {
        return false;
    }
    
    const auto* region = GetRegionPointer(type);
    if (!region) {
        return false;
    }
    
    return ((*region)[address] & (1 << bit)) != 0;
}

bool MemoryManager::WriteBit(MemoryType type, uint32_t address, uint8_t bit, bool value) {
    if (bit > 7) {
        return false;
    }
    
    if (!ValidateAccess(type, address, 1, AccessMode::WRITE)) {
        return false;
    }
    
    auto* region = GetRegionPointer(type);
    if (!region) {
        return false;
    }
    
    if (value) {
        (*region)[address] |= (1 << bit);
    } else {
        (*region)[address] &= ~(1 << bit);
    }
    
    return true;
}

uint16_t MemoryManager::ReadWord(MemoryType type, uint32_t address) const {
    if (!ValidateAccess(type, address, 2, AccessMode::READ)) {
        return 0;
    }
    
    const auto* region = GetRegionPointer(type);
    if (!region || address + 1 >= region->size()) {
        return 0;
    }
    
    return static_cast<uint16_t>((*region)[address]) | 
           (static_cast<uint16_t>((*region)[address + 1]) << 8);
}

bool MemoryManager::WriteWord(MemoryType type, uint32_t address, uint16_t value) {
    if (!ValidateAccess(type, address, 2, AccessMode::WRITE)) {
        return false;
    }
    
    auto* region = GetRegionPointer(type);
    if (!region || address + 1 >= region->size()) {
        return false;
    }
    
    (*region)[address] = static_cast<uint8_t>(value & 0xFF);
    (*region)[address + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    
    return true;
}

uint32_t MemoryManager::ReadDWord(MemoryType type, uint32_t address) const {
    if (!ValidateAccess(type, address, 4, AccessMode::READ)) {
        return 0;
    }
    
    const auto* region = GetRegionPointer(type);
    if (!region || address + 3 >= region->size()) {
        return 0;
    }
    
    return static_cast<uint32_t>((*region)[address]) |
           (static_cast<uint32_t>((*region)[address + 1]) << 8) |
           (static_cast<uint32_t>((*region)[address + 2]) << 16) |
           (static_cast<uint32_t>((*region)[address + 3]) << 24);
}

bool MemoryManager::WriteDWord(MemoryType type, uint32_t address, uint32_t value) {
    if (!ValidateAccess(type, address, 4, AccessMode::WRITE)) {
        return false;
    }
    
    auto* region = GetRegionPointer(type);
    if (!region || address + 3 >= region->size()) {
        return false;
    }
    
    (*region)[address] = static_cast<uint8_t>(value & 0xFF);
    (*region)[address + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    (*region)[address + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    (*region)[address + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
    
    return true;
}

bool MemoryManager::ReadBytes(MemoryType type, uint32_t address, uint8_t* buffer, size_t size) const {
    if (!buffer || size == 0) {
        return false;
    }
    
    if (!ValidateAccess(type, address, size, AccessMode::READ)) {
        return false;
    }
    
    const auto* region = GetRegionPointer(type);
    if (!region || address + size > region->size()) {
        return false;
    }
    
    std::copy(region->begin() + address, region->begin() + address + size, buffer);
    return true;
}

bool MemoryManager::WriteBytes(MemoryType type, uint32_t address, const uint8_t* buffer, size_t size) {
    if (!buffer || size == 0) {
        return false;
    }
    
    if (!ValidateAccess(type, address, size, AccessMode::WRITE)) {
        return false;
    }
    
    auto* region = GetRegionPointer(type);
    if (!region || address + size > region->size()) {
        return false;
    }
    
    std::copy(buffer, buffer + size, region->begin() + address);
    return true;
}

void MemoryManager::ClearRegion(MemoryType type) {
    auto* region = GetRegionPointer(type);
    if (region) {
        std::fill(region->begin(), region->end(), 0);
    }
}

void MemoryManager::ClearAll() {
    for (auto& pair : m_memoryRegions) {
        std::fill(pair.second.begin(), pair.second.end(), 0);
    }
}

void MemoryManager::Reset() {
    InitializeDefaultRegions();
}

uint32_t MemoryManager::GetRegionSize(MemoryType type) const {
    auto it = m_regionConfig.find(type);
    return (it != m_regionConfig.end()) ? it->second.size : 0;
}

uint32_t MemoryManager::GetRegionStartAddress(MemoryType type) const {
    auto it = m_regionConfig.find(type);
    return (it != m_regionConfig.end()) ? it->second.startAddress : 0;
}

AccessMode MemoryManager::GetRegionAccessMode(MemoryType type) const {
    auto it = m_regionConfig.find(type);
    return (it != m_regionConfig.end()) ? it->second.accessMode : AccessMode::READ_WRITE;
}

bool MemoryManager::IsValidAddress(MemoryType type, uint32_t address) const {
    const auto* region = GetRegionPointer(type);
    return region && address < region->size();
}

size_t MemoryManager::GetTotalMemoryUsage() const {
    size_t total = 0;
    for (const auto& pair : m_memoryRegions) {
        total += pair.second.size();
    }
    return total;
}

size_t MemoryManager::GetRegionMemoryUsage(MemoryType type) const {
    const auto* region = GetRegionPointer(type);
    return region ? region->size() : 0;
}

std::string MemoryManager::DumpRegion(MemoryType type, uint32_t startAddr, uint32_t count) const {
    const auto* region = GetRegionPointer(type);
    if (!region || startAddr >= region->size()) {
        return "Invalid region or address";
    }
    
    std::stringstream ss;
    ss << MemoryTypeToString(type) << " Memory Dump:\n";
    ss << "Address | Hex                              | ASCII\n";
    ss << "--------|----------------------------------|------------------\n";
    
    uint32_t endAddr = std::min(startAddr + count, static_cast<uint32_t>(region->size()));
    
    for (uint32_t addr = startAddr; addr < endAddr; addr += 16) {
        ss << std::hex << std::setfill('0') << std::setw(6) << addr << " | ";
        
        // Hex dump
        for (uint32_t i = 0; i < 16 && (addr + i) < endAddr; ++i) {
            ss << std::hex << std::setfill('0') << std::setw(2) 
               << static_cast<int>((*region)[addr + i]) << " ";
        }
        
        // Padding
        for (uint32_t i = endAddr - addr; i < 16; ++i) {
            ss << "   ";
        }
        
        ss << "| ";
        
        // ASCII dump
        for (uint32_t i = 0; i < 16 && (addr + i) < endAddr; ++i) {
            uint8_t byte = (*region)[addr + i];
            ss << (isprint(byte) ? static_cast<char>(byte) : '.');
        }
        
        ss << "\n";
    }
    
    return ss.str();
}

void MemoryManager::PrintMemoryMap() const {
    std::cout << "=== PLC Memory Map ===\n";
    std::cout << "Region      | Start  | Size   | Access     | Name\n";
    std::cout << "------------|--------|--------|------------|------------------\n";
    
    for (const auto& pair : m_regionConfig) {
        const auto& config = pair.second;
        std::cout << std::setw(11) << MemoryTypeToString(config.type) << " | "
                  << "0x" << std::hex << std::setfill('0') << std::setw(4) << config.startAddress << " | "
                  << std::dec << std::setw(6) << config.size << " | "
                  << std::setw(10) << AccessModeToString(config.accessMode) << " | "
                  << config.name << "\n";
    }
    
    std::cout << "\nTotal Memory Usage: " << GetTotalMemoryUsage() << " bytes ("
              << (GetTotalMemoryUsage() / 1024.0) << " KB)\n";
}

bool MemoryManager::ValidateAccess(MemoryType type, uint32_t address, size_t size, AccessMode mode) const {
    auto configIt = m_regionConfig.find(type);
    if (configIt == m_regionConfig.end()) {
        return false;
    }
    
    const auto& config = configIt->second;
    
    // Check address bounds
    if (address + size > config.size) {
        return false;
    }
    
    // Check access mode
    if (mode == AccessMode::READ && config.accessMode == AccessMode::WRITE) {
        return false;
    }
    if (mode == AccessMode::WRITE && config.accessMode == AccessMode::READ) {
        return false;
    }
    
    return true;
}

std::vector<uint8_t>* MemoryManager::GetRegionPointer(MemoryType type) {
    auto it = m_memoryRegions.find(type);
    return (it != m_memoryRegions.end()) ? &it->second : nullptr;
}

const std::vector<uint8_t>* MemoryManager::GetRegionPointer(MemoryType type) const {
    auto it = m_memoryRegions.find(type);
    return (it != m_memoryRegions.end()) ? &it->second : nullptr;
}

std::string MemoryManager::MemoryTypeToString(MemoryType type) {
    switch (type) {
        case MemoryType::INPUT: return "INPUT";
        case MemoryType::OUTPUT: return "OUTPUT";
        case MemoryType::INTERNAL: return "INTERNAL";
        case MemoryType::TIMER: return "TIMER";
        case MemoryType::COUNTER: return "COUNTER";
        case MemoryType::DATA: return "DATA";
        default: return "UNKNOWN";
    }
}

std::string MemoryManager::AccessModeToString(AccessMode mode) {
    switch (mode) {
        case AccessMode::READ: return "READ";
        case AccessMode::WRITE: return "WRITE";
        case AccessMode::READ_WRITE: return "READ_WRITE";
        default: return "UNKNOWN";
    }
}

} // namespace data
} // namespace plc_emulator
