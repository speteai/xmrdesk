/* XMRDesk
 * Copyright (c) 2024 speteai          <https://github.com/speteai>
 * Copyright (c) 2018-2023 SChernykh   <https://github.com/SChernykh>
 * Copyright (c) 2016-2023 XMRig       <https://github.com/xmrig>, <support@xmrig.com>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef XMRDESK_RYZEN_OPTIMIZER_H
#define XMRDESK_RYZEN_OPTIMIZER_H

#include "backend/cpu/interfaces/ICpuInfo.h"
#include "backend/cpu/CpuConfig.h"
#include "base/crypto/Algorithm.h"
#include <memory>

namespace xmrig {

class RyzenOptimizer
{
public:
    struct RyzenConfig {
        uint32_t threads;
        uint32_t intensity;
        int32_t affinity;
        size_t memory_per_thread;
        bool huge_pages;
        bool prefetch_l1;
        bool prefetch_l2;
        bool yield;
        bool asm_optimized;
        const char* optimization_name;
    };

    static std::unique_ptr<RyzenOptimizer> create(ICpuInfo* cpuInfo);

    bool isRyzenCpu() const;
    ICpuInfo::Arch getRyzenArchitecture() const;
    const char* getArchitectureName() const;
    RyzenConfig getOptimalConfig(const Algorithm &algorithm, uint32_t maxThreads = 0) const;

    // Ryzen-specific optimizations
    void optimizeMemoryAccess(CpuConfig& config) const;
    void optimizeCacheAffinity(CpuConfig& config) const;
    void optimizeThreadPlacement(CpuConfig& config) const;
    void optimizePowerSettings(CpuConfig& config) const;

    // Architecture-specific tuning
    void optimizeForZen(CpuConfig& config) const;
    void optimizeForZenPlus(CpuConfig& config) const;
    void optimizeForZen2(CpuConfig& config) const;
    void optimizeForZen3(CpuConfig& config) const;
    void optimizeForZen4(CpuConfig& config) const;
    void optimizeForZen5(CpuConfig& config) const;

    // Performance analysis
    bool shouldUseHugePages() const;
    uint32_t getOptimalThreadCount() const;
    size_t getOptimalIntensity() const;

    // Debug information
    void printOptimizationInfo() const;

private:
    explicit RyzenOptimizer(ICpuInfo* cpuInfo);

    // CPU Information
    ICpuInfo* m_cpuInfo;
    ICpuInfo::Vendor m_vendor;
    ICpuInfo::Arch m_arch;
    uint32_t m_cores;
    uint32_t m_threads;
    size_t m_l2Cache;
    size_t m_l3Cache;

    // Ryzen-specific properties
    bool m_isRyzen;
    bool m_hasCCX; // Core Complex support
    bool m_hasCCD; // Core Chiplet Die support
    bool m_has3DVCache; // 3D V-Cache support
    uint32_t m_ccxCount; // Number of Core Complexes
    uint32_t m_ccdCount; // Number of Core Chiplet Dies

    // Architecture-specific optimizations
    void detectRyzenFeatures();
    uint32_t getCoreComplexCount() const;
    uint32_t getChipletCount() const;
    bool hasAdvancedCache() const;

    // Memory optimization helpers
    size_t calculateOptimalMemoryPerThread(const Algorithm &algorithm) const;
    bool shouldPreferL3Cache(const Algorithm &algorithm) const;
    uint32_t calculateOptimalAffinity() const;
};

} // namespace xmrig

#endif // XMRDESK_RYZEN_OPTIMIZER_H