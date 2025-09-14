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

#include "backend/cpu/CpuOptimizer.h"
#include "backend/cpu/RyzenOptimizer.h"
#include "backend/cpu/Cpu.h"
#include "base/io/log/Log.h"
#include "base/io/log/Tags.h"

#include <algorithm>

namespace xmrig {

CpuOptimizer::CpuOptimizer() :
    m_cpuInfo(nullptr),
    m_vendor(ICpuInfo::VENDOR_UNKNOWN),
    m_arch(ICpuInfo::ARCH_UNKNOWN),
    m_cores(0),
    m_threads(0),
    m_l2Cache(0),
    m_l3Cache(0),
    m_optimizationsInitialized(false)
{
    initializeCpuInfo();
    initializeOptimizers();
}

std::unique_ptr<CpuOptimizer> CpuOptimizer::create()
{
    return std::unique_ptr<CpuOptimizer>(new CpuOptimizer());
}

void CpuOptimizer::initializeCpuInfo()
{
    m_cpuInfo = Cpu::info();
    if (!m_cpuInfo) {
        LOG_ERR("%s failed to get CPU information", Tags::cpu());
        return;
    }

    m_vendor = m_cpuInfo->vendor();
    m_arch = m_cpuInfo->arch();
    m_cores = static_cast<uint32_t>(m_cpuInfo->cores());
    m_threads = static_cast<uint32_t>(m_cpuInfo->threads());
    m_l2Cache = m_cpuInfo->L2();
    m_l3Cache = m_cpuInfo->L3();
}

void CpuOptimizer::initializeOptimizers()
{
    if (!m_cpuInfo) {
        return;
    }

    // Initialize Ryzen optimizer if AMD CPU
    if (m_vendor == ICpuInfo::VENDOR_AMD) {
        m_ryzenOptimizer = RyzenOptimizer::create(m_cpuInfo);
    }

    m_optimizationsInitialized = true;

    LOG_INFO("%s " CYAN_BOLD("XMRDesk CPU Optimizer initialized") " for " WHITE_BOLD("%s %s"),
             Tags::cpu(), getCpuVendorName(), getCpuArchitectureName());
}

CpuOptimizer::OptimizationResult CpuOptimizer::optimize(CpuConfig& config, const Algorithm &algorithm, uint32_t maxThreads)
{
    if (!m_optimizationsInitialized) {
        return optimizeGeneric(config, algorithm, maxThreads);
    }

    OptimizationResult result = {};

    switch (m_vendor) {
    case ICpuInfo::VENDOR_AMD:
        result = optimizeRyzen(config, algorithm, maxThreads);
        break;

    case ICpuInfo::VENDOR_INTEL:
        result = optimizeIntel(config, algorithm, maxThreads);
        break;

    default:
        result = optimizeGeneric(config, algorithm, maxThreads);
        break;
    }

    m_lastResult = result;
    return result;
}

CpuOptimizer::OptimizationResult CpuOptimizer::optimizeRyzen(CpuConfig& config, const Algorithm &algorithm, uint32_t maxThreads)
{
    OptimizationResult result = {};
    result.vendor = m_vendor;
    result.arch = m_arch;
    result.vendor_name = getCpuVendorName();
    result.arch_name = getCpuArchitectureName();
    result.optimizations_applied = false;

    if (!m_ryzenOptimizer || !m_ryzenOptimizer->isRyzenCpu()) {
        return optimizeGeneric(config, algorithm, maxThreads);
    }

    auto ryzenConfig = m_ryzenOptimizer->getOptimalConfig(algorithm, maxThreads);

    result.optimal_threads = ryzenConfig.threads;
    result.optimal_intensity = ryzenConfig.intensity;
    result.memory_per_thread = ryzenConfig.memory_per_thread;
    result.huge_pages_recommended = ryzenConfig.huge_pages;
    result.optimizations_applied = true;
    result.optimization_details = ryzenConfig.optimization_name;

    // Apply Ryzen-specific optimizations to the config
    m_ryzenOptimizer->optimizeMemoryAccess(config);
    m_ryzenOptimizer->optimizeCacheAffinity(config);
    m_ryzenOptimizer->optimizeThreadPlacement(config);
    m_ryzenOptimizer->optimizePowerSettings(config);

    LOG_INFO("%s " GREEN_BOLD("AMD Ryzen optimizations applied:") " %s, "
             "Threads: " WHITE_BOLD("%u") ", Intensity: " WHITE_BOLD("%u") ", "
             "Memory: " WHITE_BOLD("%.1f MB") "/thread",
             Tags::cpu(), result.optimization_details, result.optimal_threads,
             result.optimal_intensity,
             static_cast<double>(result.memory_per_thread) / (1024.0 * 1024.0));

    return result;
}

CpuOptimizer::OptimizationResult CpuOptimizer::optimizeIntel(CpuConfig& config, const Algorithm &algorithm, uint32_t maxThreads)
{
    OptimizationResult result = {};
    result.vendor = m_vendor;
    result.arch = m_arch;
    result.vendor_name = getCpuVendorName();
    result.arch_name = "Intel";
    result.optimizations_applied = true;

    // Intel-specific optimizations
    result.optimal_threads = maxThreads > 0 ? std::min(m_threads, maxThreads) : m_threads;
    result.optimal_intensity = std::min(static_cast<uint32_t>(4), std::max(1U, static_cast<uint32_t>(m_l3Cache / (8 * 1024 * 1024))));
    result.memory_per_thread = 2 * 1024 * 1024; // 2MB base
    result.huge_pages_recommended = true;
    result.optimization_details = "Intel Optimized";

    // Intel CPUs generally benefit from hyperthreading in mining
    // but we limit intensity based on cache size
    if (m_l3Cache > 16 * 1024 * 1024) { // >16MB L3
        result.optimal_intensity = std::min(result.optimal_intensity + 1, 6U);
        result.memory_per_thread += 1024 * 1024; // +1MB
    }

    LOG_INFO("%s " CYAN_BOLD("Intel optimizations applied:") " "
             "Threads: " WHITE_BOLD("%u") ", Intensity: " WHITE_BOLD("%u") ", "
             "Memory: " WHITE_BOLD("%.1f MB") "/thread",
             Tags::cpu(), result.optimal_threads, result.optimal_intensity,
             static_cast<double>(result.memory_per_thread) / (1024.0 * 1024.0));

    return result;
}

CpuOptimizer::OptimizationResult CpuOptimizer::optimizeGeneric(CpuConfig& config, const Algorithm &algorithm, uint32_t maxThreads)
{
    OptimizationResult result = {};
    result.vendor = m_vendor;
    result.arch = m_arch;
    result.vendor_name = getCpuVendorName();
    result.arch_name = "Generic";
    result.optimizations_applied = false;

    // Conservative generic settings
    result.optimal_threads = maxThreads > 0 ? std::min(m_cores, maxThreads) : m_cores;
    result.optimal_intensity = 1;
    result.memory_per_thread = 2 * 1024 * 1024; // 2MB
    result.huge_pages_recommended = false;
    result.optimization_details = "Generic (Conservative)";

    LOG_INFO("%s " YELLOW_BOLD("Generic optimizations applied:") " "
             "Threads: " WHITE_BOLD("%u") ", Intensity: " WHITE_BOLD("%u"),
             Tags::cpu(), result.optimal_threads, result.optimal_intensity);

    return result;
}

ICpuInfo::Vendor CpuOptimizer::getCpuVendor() const
{
    return m_vendor;
}

ICpuInfo::Arch CpuOptimizer::getCpuArchitecture() const
{
    return m_arch;
}

const char* CpuOptimizer::getCpuVendorName() const
{
    switch (m_vendor) {
    case ICpuInfo::VENDOR_AMD:     return "AMD";
    case ICpuInfo::VENDOR_INTEL:   return "Intel";
    default:                       return "Unknown";
    }
}

const char* CpuOptimizer::getCpuArchitectureName() const
{
    if (m_ryzenOptimizer && m_ryzenOptimizer->isRyzenCpu()) {
        return m_ryzenOptimizer->getArchitectureName();
    }

    switch (m_vendor) {
    case ICpuInfo::VENDOR_INTEL:   return "Intel x86";
    case ICpuInfo::VENDOR_AMD:     return "AMD x86";
    default:                       return "Generic x86";
    }
}

bool CpuOptimizer::isOptimizationSupported() const
{
    return m_optimizationsInitialized;
}

bool CpuOptimizer::isRyzenCpu() const
{
    return m_ryzenOptimizer && m_ryzenOptimizer->isRyzenCpu();
}

bool CpuOptimizer::isIntelCpu() const
{
    return m_vendor == ICpuInfo::VENDOR_INTEL;
}

void CpuOptimizer::printOptimizationSummary() const
{
    if (!m_optimizationsInitialized) {
        LOG_WARN("%s CPU optimizations not initialized", Tags::cpu());
        return;
    }

    LOG_INFO("%s " MAGENTA_BOLD("=== XMRDesk CPU Optimization Summary ==="), Tags::cpu());
    LOG_INFO("%s CPU: " WHITE_BOLD("%s %s") " (" WHITE_BOLD("%u") " cores, " WHITE_BOLD("%u") " threads)",
             Tags::cpu(), getCpuVendorName(), getCpuArchitectureName(), m_cores, m_threads);

    if (m_l3Cache > 0) {
        LOG_INFO("%s Cache: L3 " WHITE_BOLD("%.1f MB"), Tags::cpu(),
                 static_cast<double>(m_l3Cache) / (1024.0 * 1024.0));
    }

    if (isRyzenCpu()) {
        m_ryzenOptimizer->printOptimizationInfo();
    } else if (isIntelCpu()) {
        LOG_INFO("%s " CYAN_BOLD("Intel optimizations available"), Tags::cpu());
    } else {
        LOG_INFO("%s " YELLOW_BOLD("Generic optimizations only"), Tags::cpu());
    }
}

void CpuOptimizer::printDetailedInfo() const
{
    printOptimizationSummary();

    if (m_optimizationsInitialized && m_lastResult.optimizations_applied) {
        LOG_INFO("%s " GREEN_BOLD("Last Applied Optimizations:"), Tags::cpu());
        LOG_INFO("%s   Threads: " WHITE_BOLD("%u") ", Intensity: " WHITE_BOLD("%u"),
                 Tags::cpu(), m_lastResult.optimal_threads, m_lastResult.optimal_intensity);
        LOG_INFO("%s   Memory/Thread: " WHITE_BOLD("%.1f MB") ", Huge Pages: " WHITE_BOLD("%s"),
                 Tags::cpu(),
                 static_cast<double>(m_lastResult.memory_per_thread) / (1024.0 * 1024.0),
                 m_lastResult.huge_pages_recommended ? "Recommended" : "Not needed");
    }
}

void CpuOptimizer::applyOptimalConfig(CpuConfig& config, const OptimizationResult& result)
{
    // This would apply the optimization result to the actual CpuConfig
    // Implementation depends on the specific CpuConfig structure
    // For now, this is a placeholder
}

uint32_t CpuOptimizer::getRecommendedThreadCount(ICpuInfo* cpuInfo)
{
    if (!cpuInfo) {
        return 1;
    }

    auto optimizer = create();
    return optimizer->getRecommendedThreadCount(cpuInfo);
}

bool CpuOptimizer::shouldUseHugePages(ICpuInfo* cpuInfo)
{
    if (!cpuInfo) {
        return false;
    }

    // AMD Ryzen and modern Intel benefit from huge pages
    return cpuInfo->vendor() == ICpuInfo::VENDOR_AMD || cpuInfo->vendor() == ICpuInfo::VENDOR_INTEL;
}

} // namespace xmrig