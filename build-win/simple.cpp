#include <iostream>
#include <string>
#include <windows.h>
#include <intrin.h>

int main() {
    // XMRDesk Windows Demo
    std::cout << "\n";
    std::cout << "  ██╗  ██╗███╗   ███╗██████╗ ██████╗ ███████╗███████╗██╗  ██╗\n";
    std::cout << "  ╚██╗██╔╝████╗ ████║██╔══██╗██╔══██╗██╔════╝██╔════╝██║ ██╔╝\n";
    std::cout << "   ╚███╔╝ ██╔████╔██║██████╔╝██║  ██║█████╗  ███████╗█████╔╝ \n";
    std::cout << "   ██╔██╗ ██║╚██╔╝██║██╔══██╗██║  ██║██╔══╝  ╚════██║██╔═██╗ \n";
    std::cout << "  ██╔╝ ██╗██║ ╚═╝ ██║██║  ██║██████╔╝███████╗███████║██║  ██╗\n";
    std::cout << "  ╚═╝  ╚═╝╚═╝     ╚═╝╚═╝  ╚═╝╚═════╝ ╚══════╝╚══════╝╚═╝  ╚═╝\n";
    std::cout << "\n";
    std::cout << "              🚀 XMRDesk - Windows Mining GUI v1.0.0\n";
    std::cout << "                     Built with AMD Ryzen Optimizations\n";
    std::cout << "\n";

    // Simple CPU detection
    char vendor[13] = {0};
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 0);
    memcpy(vendor + 0, &cpuInfo[1], 4);
    memcpy(vendor + 4, &cpuInfo[3], 4);
    memcpy(vendor + 8, &cpuInfo[2], 4);

    std::string cpuType;
    if (strstr(vendor, "AuthenticAMD")) {
        cpuType = "🚀 AMD Ryzen detected - Advanced optimizations available!";
    } else if (strstr(vendor, "GenuineIntel")) {
        cpuType = "⚡ Intel processor detected - Intel optimizations active";
    } else {
        cpuType = "💻 Generic processor - Standard optimizations";
    }

    std::cout << "CPU Status: " << cpuType << "\n\n";

    std::cout << "=== XMRDesk Features ===\n";
    std::cout << "✅ Qt6 GUI with real-time hashrate charts\n";
    std::cout << "✅ CPU temperature monitoring\n";
    std::cout << "✅ AMD Ryzen optimizations (Zen 1-5)\n";
    std::cout << "✅ Intel processor optimizations\n";
    std::cout << "✅ Pre-configured mining pools\n";
    std::cout << "✅ Easy wallet configuration\n";
    std::cout << "✅ Professional Windows interface\n\n";

    std::cout << "📋 Supported Mining Pools:\n";
    std::cout << "   • SupportXMR.com (Recommended)\n";
    std::cout << "   • Qubic.org\n";
    std::cout << "   • Nanopool.org\n\n";

    std::cout << "💝 Support Development:\n";
    std::cout << "XMR: 48ckezCUYfnj3vDtQRtH1X4ExpvowjRBJj7U36P13KbDPeZ6M3Pjuev3xdCkXPuAuQNomuSDXTBZFTkQRfP33t3gMTjJCpL\n\n";

    std::cout << "🔧 This is the demo version. The full GUI version includes:\n";
    std::cout << "   → Real-time mining with actual hashrate\n";
    std::cout << "   → Complete Qt6 interface\n";
    std::cout << "   → Advanced CPU optimizations\n";
    std::cout << "   → Temperature monitoring\n";
    std::cout << "   → Pool management\n\n";

    std::cout << "📁 Repository: https://github.com/speteai/xmrdesk\n";
    std::cout << "🌐 Full version available via GitHub Actions builds\n\n";

    std::cout << "Press Enter to exit...";
    std::cin.get();

    return 0;
}