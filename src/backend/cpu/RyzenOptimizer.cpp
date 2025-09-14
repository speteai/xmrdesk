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

#include "backend/cpu/RyzenOptimizer.h"
#include "backend/cpu/Cpu.h"
#include "base/io/log/Log.h"
#include "base/io/log/Tags.h"

#include <algorithm>
#include <cmath>

namespace xmrig {

RyzenOptimizer::RyzenOptimizer(ICpuInfo* cpuInfo) :
    m_cpuInfo(cpuInfo),
    m_vendor(cpuInfo->vendor()),
    m_arch(cpuInfo->arch()),
    m_cores(static_cast<uint32_t>(cpuInfo->cores())),
    m_threads(static_cast<uint32_t>(cpuInfo->threads())),
    m_l2Cache(cpuInfo->L2()),
    m_l3Cache(cpuInfo->L3()),
    m_isRyzen(false),
    m_hasCCX(false),
    m_hasCCD(false),
    m_has3DVCache(false),
    m_ccxCount(0),
    m_ccdCount(0)
{
    detectRyzenFeatures();
}

std::unique_ptr<RyzenOptimizer> RyzenOptimizer::create(ICpuInfo* cpuInfo)
{
    return std::unique_ptr<RyzenOptimizer>(new RyzenOptimizer(cpuInfo));
}

bool RyzenOptimizer::isRyzenCpu() const
{
    return m_isRyzen;
}

ICpuInfo::Arch RyzenOptimizer::getRyzenArchitecture() const
{
    return m_arch;
}

const char* RyzenOptimizer::getArchitectureName() const
{
    if (!m_isRyzen) {
        return "Not Ryzen";
    }

    switch (m_arch) {
    case ICpuInfo::ARCH_ZEN:     return "Zen (1st Gen)";
    case ICpuInfo::ARCH_ZEN_PLUS: return "Zen+ (2nd Gen)";
    case ICpuInfo::ARCH_ZEN2:    return "Zen 2 (3rd Gen)";
    case ICpuInfo::ARCH_ZEN3:    return "Zen 3 (4th Gen)";
    case ICpuInfo::ARCH_ZEN4:    return "Zen 4 (5th Gen)";
    case ICpuInfo::ARCH_ZEN5:    return "Zen 5 (6th Gen)";
    default:                     return "Unknown Zen";
    }
}

void RyzenOptimizer::detectRyzenFeatures()
{
    m_isRyzen = (m_vendor == ICpuInfo::VENDOR_AMD) &&
                (m_arch >= ICpuInfo::ARCH_ZEN && m_arch <= ICpuInfo::ARCH_ZEN5);

    if (!m_isRyzen) {
        return;
    }

    // Detect Core Complex (CCX) and Chiplet (CCD) configuration
    switch (m_arch) {
    case ICpuInfo::ARCH_ZEN:
    case ICpuInfo::ARCH_ZEN_PLUS:
        // Original Zen: 4 cores per CCX, 2 CCXs per die
        m_hasCCX = true;
        m_ccxCount = std::max(1U, (m_cores + 3) / 4);
        m_ccdCount = 1;
        break;

    case ICpuInfo::ARCH_ZEN2:
        // Zen 2: 4 cores per CCX, 2 CCXs per CCD, multiple CCDs possible
        m_hasCCX = true;
        m_hasCCD = true;
        m_ccxCount = std::max(1U, (m_cores + 3) / 4);
        m_ccdCount = std::max(1U, (m_cores + 7) / 8);
        break;

    case ICpuInfo::ARCH_ZEN3:
        // Zen 3: 8 cores per CCX (unified), 1 CCX per CCD
        m_hasCCX = true;
        m_hasCCD = true;
        m_ccxCount = std::max(1U, (m_cores + 7) / 8);
        m_ccdCount = m_ccxCount;
        // Check for 3D V-Cache (some Zen 3 models)
        if (m_l3Cache > (32 * 1024 * 1024)) { // >32MB suggests 3D V-Cache
            m_has3DVCache = true;
        }
        break;

    case ICpuInfo::ARCH_ZEN4:
    case ICpuInfo::ARCH_ZEN5:
        // Zen 4/5: 8 cores per CCX, advanced chiplet design
        m_hasCCX = true;
        m_hasCCD = true;
        m_ccxCount = std::max(1U, (m_cores + 7) / 8);
        m_ccdCount = m_ccxCount;
        // Many Zen 4/5 models have 3D V-Cache
        if (m_l3Cache > (32 * 1024 * 1024)) {
            m_has3DVCache = true;
        }
        break;

    default:
        break;
    }

    LOG_INFO("%s " CYAN_BOLD("detected %s") " with " WHITE_BOLD("%u") " cores, "
             WHITE_BOLD("%u") " CCX, " WHITE_BOLD("%u") " CCD%s",
             Tags::cpu(), getArchitectureName(), m_cores, m_ccxCount, m_ccdCount,
             m_has3DVCache ? ", 3D V-Cache" : "");
}

RyzenOptimizer::RyzenConfig RyzenOptimizer::getOptimalConfig(const Algorithm &algorithm, uint32_t maxThreads) const
{
    RyzenConfig config = {};

    if (!m_isRyzen) {
        // Fallback for non-Ryzen
        config.threads = std::min(m_threads, maxThreads > 0 ? maxThreads : m_threads);
        config.intensity = 1;
        config.affinity = -1;
        config.memory_per_thread = 2 * 1024 * 1024; // 2MB default
        config.huge_pages = false;
        config.prefetch_l1 = false;
        config.prefetch_l2 = false;
        config.yield = false;
        config.asm_optimized = false;
        config.optimization_name = "Generic";
        return config;
    }

    // Base configuration
    config.threads = getOptimalThreadCount();
    if (maxThreads > 0) {
        config.threads = std::min(config.threads, maxThreads);
    }

    config.intensity = static_cast<uint32_t>(getOptimalIntensity());
    config.affinity = calculateOptimalAffinity();
    config.memory_per_thread = calculateOptimalMemoryPerThread(algorithm);
    config.huge_pages = shouldUseHugePages();
    config.yield = false;
    config.asm_optimized = true;
    config.optimization_name = getArchitectureName();

    // Architecture-specific optimizations
    switch (m_arch) {
    case ICpuInfo::ARCH_ZEN:
        config.prefetch_l1 = true;
        config.prefetch_l2 = false;
        config.intensity = std::min(config.intensity, 2U); // Conservative for Zen 1
        break;

    case ICpuInfo::ARCH_ZEN_PLUS:
        config.prefetch_l1 = true;
        config.prefetch_l2 = true;
        config.intensity = std::min(config.intensity, 3U);
        break;

    case ICpuInfo::ARCH_ZEN2:
        config.prefetch_l1 = true;
        config.prefetch_l2 = true;
        config.intensity = std::min(config.intensity, 4U);
        // Zen 2 benefits from slightly more aggressive threading
        if (config.threads < m_cores) {
            config.threads = std::min(m_cores, maxThreads > 0 ? maxThreads : m_cores);
        }
        break;

    case ICpuInfo::ARCH_ZEN3:
        config.prefetch_l1 = true;
        config.prefetch_l2 = true;
        config.intensity = std::min(config.intensity, 5U);
        // Zen 3 unified cache benefits from higher thread counts
        if (m_has3DVCache) {
            // 3D V-Cache models can handle more intensity
            config.intensity = std::min(config.intensity + 1, 6U);
            config.memory_per_thread = config.memory_per_thread * 3 / 2; // 50% more memory
        }
        break;

    case ICpuInfo::ARCH_ZEN4:
    case ICpuInfo::ARCH_ZEN5:
        config.prefetch_l1 = true;
        config.prefetch_l2 = true;
        config.intensity = std::min(config.intensity, 6U);
        // Modern Zen architectures are very efficient
        if (m_has3DVCache) {
            config.intensity = std::min(config.intensity + 2, 8U);
            config.memory_per_thread = config.memory_per_thread * 2; // Double memory for 3D V-Cache
        }
        break;

    default:
        config.prefetch_l1 = false;
        config.prefetch_l2 = false;
        break;
    }

    return config;
}

uint32_t RyzenOptimizer::getOptimalThreadCount() const
{
    if (!m_isRyzen) {
        return m_threads;
    }

    // For Ryzen, optimal thread count depends on architecture and workload
    switch (m_arch) {
    case ICpuInfo::ARCH_ZEN:
    case ICpuInfo::ARCH_ZEN_PLUS:
        // Early Zen benefits from using physical cores primarily
        return std::min(m_cores, m_threads);

    case ICpuInfo::ARCH_ZEN2:
        // Zen 2 can utilize SMT more effectively
        return std::min(static_cast<uint32_t>(m_threads * 0.85), m_threads);

    case ICpuInfo::ARCH_ZEN3:
    case ICpuInfo::ARCH_ZEN4:
    case ICpuInfo::ARCH_ZEN5:
        // Modern Zen architectures handle SMT very well
        return m_threads;

    default:
        return m_cores;
    }
}

size_t RyzenOptimizer::getOptimalIntensity() const
{
    if (!m_isRyzen) {
        return 1;
    }

    // Intensity based on cache size and architecture
    size_t baseIntensity = 1;

    if (m_l3Cache > 0) {
        // Calculate based on L3 cache size
        baseIntensity = std::min(static_cast<size_t>(8), m_l3Cache / (4 * 1024 * 1024)); // 4MB per intensity level
    }

    // Architecture adjustments
    switch (m_arch) {
    case ICpuInfo::ARCH_ZEN:
        return std::max(static_cast<size_t>(1), baseIntensity / 2);

    case ICpuInfo::ARCH_ZEN_PLUS:
        return std::max(static_cast<size_t>(1), baseIntensity * 2 / 3);

    case ICpuInfo::ARCH_ZEN2:
        return baseIntensity;

    case ICpuInfo::ARCH_ZEN3:
        return baseIntensity + (m_has3DVCache ? 1 : 0);

    case ICpuInfo::ARCH_ZEN4:
    case ICpuInfo::ARCH_ZEN5:
        return baseIntensity + (m_has3DVCache ? 2 : 1);

    default:
        return 1;
    }
}

size_t RyzenOptimizer::calculateOptimalMemoryPerThread(const Algorithm &algorithm) const
{
    if (!m_isRyzen) {
        return 2 * 1024 * 1024; // 2MB default
    }

    // Base memory requirement for RandomX
    size_t baseMemory = 2 * 1024 * 1024; // 2MB

    // Adjust for Ryzen architecture
    switch (m_arch) {
    case ICpuInfo::ARCH_ZEN:
        return baseMemory;

    case ICpuInfo::ARCH_ZEN_PLUS:
        return baseMemory + (512 * 1024); // +512KB

    case ICpuInfo::ARCH_ZEN2:
        return baseMemory + (1024 * 1024); // +1MB

    case ICpuInfo::ARCH_ZEN3:
        if (m_has3DVCache) {
            return baseMemory * 2; // 4MB for 3D V-Cache
        }
        return baseMemory + (1024 * 1024); // +1MB

    case ICpuInfo::ARCH_ZEN4:
    case ICpuInfo::ARCH_ZEN5:
        if (m_has3DVCache) {
            return baseMemory * 3; // 6MB for advanced 3D V-Cache
        }
        return baseMemory + (1536 * 1024); // +1.5MB

    default:
        return baseMemory;
    }
}

bool RyzenOptimizer::shouldUseHugePages() const
{
    return m_isRyzen; // Ryzen benefits significantly from huge pages
}

uint32_t RyzenOptimizer::calculateOptimalAffinity() const
{
    if (!m_isRyzen || !m_hasCCX) {
        return static_cast<uint32_t>(-1); // No specific affinity
    }

    // For Ryzen, prefer to keep threads within CCX boundaries
    // This is a simplified approach - in practice, more complex logic would be needed
    return static_cast<uint32_t>(-1); // Let the system handle it for now
}

void RyzenOptimizer::printOptimizationInfo() const
{
    if (!m_isRyzen) {
        LOG_INFO("%s " WHITE_BOLD("CPU is not AMD Ryzen - using generic optimizations"), Tags::cpu());
        return;
    }

    LOG_INFO("%s " GREEN_BOLD("XMRDesk Ryzen Optimizations Active:"), Tags::cpu());
    LOG_INFO("%s   Architecture: " WHITE_BOLD("%s"), Tags::cpu(), getArchitectureName());
    LOG_INFO("%s   Cores: " WHITE_BOLD("%u") ", Threads: " WHITE_BOLD("%u"), Tags::cpu(), m_cores, m_threads);
    LOG_INFO("%s   CCX Count: " WHITE_BOLD("%u") ", CCD Count: " WHITE_BOLD("%u"), Tags::cpu(), m_ccxCount, m_ccdCount);

    if (m_l3Cache > 0) {
        LOG_INFO("%s   L3 Cache: " WHITE_BOLD("%.1f MB") "%s", Tags::cpu(),
                 static_cast<double>(m_l3Cache) / (1024.0 * 1024.0),
                 m_has3DVCache ? " (3D V-Cache)" : "");
    }

    LOG_INFO("%s   Optimal Threads: " WHITE_BOLD("%u") ", Intensity: " WHITE_BOLD("%zu"),
             Tags::cpu(), getOptimalThreadCount(), getOptimalIntensity());
    LOG_INFO("%s   Memory per Thread: " WHITE_BOLD("%.1f MB") ", Huge Pages: " WHITE_BOLD("%s"),
             Tags::cpu(),
             static_cast<double>(calculateOptimalMemoryPerThread(Algorithm::RX_0)) / (1024.0 * 1024.0),
             shouldUseHugePages() ? "Yes" : "No");
}

void RyzenOptimizer::optimizeMemoryAccess(CpuConfig& config) const
{
    // Implementation would modify the CpuConfig for optimal memory access patterns
    // This is a placeholder for the actual optimization logic
}

void RyzenOptimizer::optimizeCacheAffinity(CpuConfig& config) const
{
    // Implementation would optimize cache affinity settings
}

void RyzenOptimizer::optimizeThreadPlacement(CpuConfig& config) const
{
    // Implementation would optimize thread placement across CCX/CCD boundaries
}

void RyzenOptimizer::optimizePowerSettings(CpuConfig& config) const
{
    // Implementation would optimize power-related settings for Ryzen
}

void RyzenOptimizer::optimizeForZen(CpuConfig& config) const
{
    // Zen 1 specific optimizations
}

void RyzenOptimizer::optimizeForZenPlus(CpuConfig& config) const
{
    // Zen+ specific optimizations
}

void RyzenOptimizer::optimizeForZen2(CpuConfig& config) const
{
    // Zen 2 specific optimizations
}

void RyzenOptimizer::optimizeForZen3(CpuConfig& config) const
{
    // Zen 3 specific optimizations
}

void RyzenOptimizer::optimizeForZen4(CpuConfig& config) const
{
    // Zen 4 specific optimizations
}

void RyzenOptimizer::optimizeForZen5(CpuConfig& config) const
{
    // Zen 5 specific optimizations
}

} // namespace xmrig