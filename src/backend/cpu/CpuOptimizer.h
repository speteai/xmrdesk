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

#ifndef XMRDESK_CPU_OPTIMIZER_H
#define XMRDESK_CPU_OPTIMIZER_H

#include "backend/cpu/interfaces/ICpuInfo.h"
#include "backend/cpu/CpuConfig.h"
#include "base/crypto/Algorithm.h"
#include <memory>

namespace xmrig {

class RyzenOptimizer;

/**
 * @brief Central CPU optimization manager for XMRDesk
 *
 * This class automatically detects the CPU type and applies appropriate optimizations:
 * - AMD Ryzen: Specialized optimizations for all Zen architectures
 * - Intel: Generic optimizations with Intel-specific tuning
 * - Others: Conservative generic optimizations
 */
class CpuOptimizer
{
public:
    struct OptimizationResult {
        ICpuInfo::Vendor vendor;
        ICpuInfo::Arch arch;
        const char* vendor_name;
        const char* arch_name;
        uint32_t optimal_threads;
        uint32_t optimal_intensity;
        size_t memory_per_thread;
        bool huge_pages_recommended;
        bool optimizations_applied;
        const char* optimization_details;
    };

    static std::unique_ptr<CpuOptimizer> create();

    // Main optimization interface
    OptimizationResult optimize(CpuConfig& config, const Algorithm &algorithm, uint32_t maxThreads = 0);

    // Information getters
    ICpuInfo::Vendor getCpuVendor() const;
    ICpuInfo::Arch getCpuArchitecture() const;
    const char* getCpuVendorName() const;
    const char* getCpuArchitectureName() const;
    bool isOptimizationSupported() const;

    // Specialized optimizers
    bool isRyzenCpu() const;
    bool isIntelCpu() const;

    // Performance analysis
    void printOptimizationSummary() const;
    void printDetailedInfo() const;

    // Configuration helpers
    static void applyOptimalConfig(CpuConfig& config, const OptimizationResult& result);
    static uint32_t getRecommendedThreadCount(ICpuInfo* cpuInfo);
    static bool shouldUseHugePages(ICpuInfo* cpuInfo);

private:
    CpuOptimizer();

    ICpuInfo* m_cpuInfo;
    std::unique_ptr<RyzenOptimizer> m_ryzenOptimizer;

    // CPU information
    ICpuInfo::Vendor m_vendor;
    ICpuInfo::Arch m_arch;
    uint32_t m_cores;
    uint32_t m_threads;
    size_t m_l2Cache;
    size_t m_l3Cache;

    // Optimization state
    bool m_optimizationsInitialized;
    OptimizationResult m_lastResult;

    // Internal optimization methods
    OptimizationResult optimizeRyzen(CpuConfig& config, const Algorithm &algorithm, uint32_t maxThreads);
    OptimizationResult optimizeIntel(CpuConfig& config, const Algorithm &algorithm, uint32_t maxThreads);
    OptimizationResult optimizeGeneric(CpuConfig& config, const Algorithm &algorithm, uint32_t maxThreads);

    void initializeCpuInfo();
    void initializeOptimizers();
};

} // namespace xmrig

#endif // XMRDESK_CPU_OPTIMIZER_H