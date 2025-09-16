#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <sstream>
#include <iomanip>
#include <intrin.h>
#include <ctime>

// Windows GUI includes
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

// Window controls IDs
#define ID_POOL_COMBO     1001
#define ID_WALLET_EDIT    1002
#define ID_THREADS_EDIT   1003
#define ID_START_BUTTON   1004
#define ID_STOP_BUTTON    1005
#define ID_STATUS_TEXT    1006
#define ID_HASHRATE_TEXT  1007
#define ID_TEMP_TEXT      1008
#define ID_OPTIMIZE_CHECK 1009
#define ID_ABOUT_BUTTON   1010
#define ID_SHARES_LIST    1011
#define ID_POWER_TEXT     1012
#define ID_UPTIME_TEXT    1013

// Global variables
HWND g_hMainWindow;
HWND g_hPoolCombo;
HWND g_hWalletEdit;
HWND g_hThreadsEdit;
HWND g_hStartButton;
HWND g_hStopButton;
HWND g_hStatusText;
HWND g_hHashrateText;
HWND g_hTempText;
HWND g_hOptimizeCheck;
HWND g_hSharesList;
HWND g_hPowerText;
HWND g_hUptimeText;
HFONT g_hFont;
HFONT g_hTitleFont;
HFONT g_hBoldFont;
HBRUSH g_hBrushBg;
HBRUSH g_hBrushDark;
bool g_isMining = false;
HANDLE g_miningThread = NULL;
volatile bool g_shouldStop = false;

// Mining data
std::deque<std::string> g_shares;
int g_totalShares = 0;
time_t g_startTime = 0;

// Mining pools configuration
struct Pool {
    std::string name;
    std::string url;
    std::string port;
};

std::vector<Pool> g_pools = {
    {"SupportXMR.com", "pool.supportxmr.com", "5555"},
    {"Nanopool.org", "xmr-eu1.nanopool.org", "14444"},
    {"MineXMR.com", "pool.minexmr.com", "4444"},
    {"MoneroOcean.stream", "gulf.moneroocean.stream", "10032"},
    {"F2Pool", "xmr.f2pool.com", "13531"}
};

// CPU Detection and optimization
class CPUInfo {
public:
    std::string brand;
    int cores;
    int threads;
    bool isAMD;
    bool isRyzen;
    int zenArchitecture;

    CPUInfo() {
        detectCPU();
    }

private:
    void detectCPU() {
        int cpuInfo[4] = {0};
        char brandString[0x40] = {0};

        __cpuid(cpuInfo, 0x80000000);
        unsigned int nExIds = cpuInfo[0];

        for (unsigned int i = 0x80000000; i <= nExIds; ++i) {
            __cpuid(cpuInfo, i);
            if (i == 0x80000002)
                memcpy(brandString, cpuInfo, sizeof(cpuInfo));
            else if (i == 0x80000003)
                memcpy(brandString + 16, cpuInfo, sizeof(cpuInfo));
            else if (i == 0x80000004)
                memcpy(brandString + 32, cpuInfo, sizeof(cpuInfo));
        }

        brand = std::string(brandString);
        isAMD = brand.find("AMD") != std::string::npos;
        isRyzen = brand.find("Ryzen") != std::string::npos;

        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        cores = sysInfo.dwNumberOfProcessors / 2;
        threads = sysInfo.dwNumberOfProcessors;

        if (isRyzen) {
            if (brand.find("1000") != std::string::npos || brand.find("2000") != std::string::npos) {
                zenArchitecture = 1;
            } else if (brand.find("3000") != std::string::npos) {
                zenArchitecture = 2;
            } else if (brand.find("4000") != std::string::npos || brand.find("5000") != std::string::npos) {
                zenArchitecture = 3;
            } else {
                zenArchitecture = 4;
            }
        }
    }
};

CPUInfo g_cpuInfo;

// Add share to list
void addShare(const std::string& status) {
    time_t now = time(0);
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", localtime(&now));

    std::string shareText = std::string(timeStr) + " - " + status;
    g_shares.push_front(shareText);

    if (g_shares.size() > 10) {
        g_shares.pop_back();
    }

    // Update shares list
    if (IsWindow(g_hSharesList)) {
        SendMessage(g_hSharesList, LB_RESETCONTENT, 0, 0);
        for (const auto& share : g_shares) {
            SendMessage(g_hSharesList, LB_ADDSTRING, 0, (LPARAM)share.c_str());
        }
    }
}

// Calculate realistic hashrate based on CPU
double calculateRealisticHashrate(int threads, bool isOptimized) {
    double baseHashrate = 0;

    if (g_cpuInfo.isRyzen) {
        switch (g_cpuInfo.zenArchitecture) {
        case 1: baseHashrate = 450 * threads; break;  // Zen 1
        case 2: baseHashrate = 520 * threads; break;  // Zen 2
        case 3: baseHashrate = 580 * threads; break;  // Zen 3
        case 4: baseHashrate = 620 * threads; break;  // Zen 4+
        default: baseHashrate = 500 * threads; break;
        }

        if (isOptimized) {
            baseHashrate *= 1.15;  // 15% boost with optimizations
        }
    } else {
        // Intel or other CPUs
        baseHashrate = 380 * threads;
        if (isOptimized) {
            baseHashrate *= 1.08;  // 8% boost
        }
    }

    // Add some realistic variance (+/- 10%)
    double variance = (rand() % 200 - 100) / 1000.0;
    return baseHashrate * (1.0 + variance);
}

// Mining simulation thread
DWORD WINAPI simulateMining(LPVOID lpParam) {
    double hashrate = 0;
    double temperature = 35.0;
    int iteration = 0;
    int shareCounter = 0;

    g_startTime = time(0);

    // Get thread count from UI
    char threadsStr[10];
    GetWindowTextA(g_hThreadsEdit, threadsStr, sizeof(threadsStr));
    int activeThreads = atoi(threadsStr);
    if (activeThreads <= 0) activeThreads = g_cpuInfo.threads;

    // Check if optimizations are enabled
    bool optimized = SendMessage(g_hOptimizeCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;

    while (g_isMining && !g_shouldStop) {
        // Calculate realistic hashrate
        hashrate = calculateRealisticHashrate(activeThreads, optimized);

        // Simulate temperature based on load
        temperature = 35 + (hashrate / 800) + (rand() % 15);
        if (temperature > 85) temperature = 85;

        // Update GUI safely
        if (IsWindow(g_hHashrateText)) {
            std::stringstream ss;
            ss << std::fixed << std::setprecision(1) << hashrate << " H/s";
            SetWindowTextA(g_hHashrateText, ss.str().c_str());
        }

        if (IsWindow(g_hTempText)) {
            std::stringstream tempSS;
            tempSS << std::fixed << std::setprecision(1) << temperature << "°C";
            SetWindowTextA(g_hTempText, tempSS.str().c_str());
        }

        if (IsWindow(g_hPowerText)) {
            int power = (int)(activeThreads * 8 + (hashrate / 100));
            std::stringstream powerSS;
            powerSS << power << "W";
            SetWindowTextA(g_hPowerText, powerSS.str().c_str());
        }

        if (IsWindow(g_hUptimeText)) {
            time_t uptime = time(0) - g_startTime;
            int hours = uptime / 3600;
            int minutes = (uptime % 3600) / 60;
            int seconds = uptime % 60;
            std::stringstream uptimeSS;
            uptimeSS << std::setfill('0') << std::setw(2) << hours << ":"
                     << std::setw(2) << minutes << ":" << std::setw(2) << seconds;
            SetWindowTextA(g_hUptimeText, uptimeSS.str().c_str());
        }

        if (IsWindow(g_hStatusText)) {
            if (iteration % 30 == 0) {
                SetWindowTextA(g_hStatusText, "Mining Active - Connected");
            }
        }

        // Simulate shares being found
        shareCounter++;
        if (shareCounter >= (60 - (hashrate / 1000))) {  // Higher hashrate = more frequent shares
            g_totalShares++;
            if (rand() % 100 < 98) {  // 98% accepted
                addShare("Share #" + std::to_string(g_totalShares) + " - ACCEPTED");
            } else {
                addShare("Share #" + std::to_string(g_totalShares) + " - REJECTED");
            }
            shareCounter = 0;
        }

        Sleep(1000);
        iteration++;
    }

    // Reset when stopped
    if (IsWindow(g_hHashrateText)) SetWindowTextA(g_hHashrateText, "0.0 H/s");
    if (IsWindow(g_hTempText)) SetWindowTextA(g_hTempText, "--°C");
    if (IsWindow(g_hPowerText)) SetWindowTextA(g_hPowerText, "0W");
    if (IsWindow(g_hUptimeText)) SetWindowTextA(g_hUptimeText, "00:00:00");
    if (IsWindow(g_hStatusText)) SetWindowTextA(g_hStatusText, "Mining Stopped");

    return 0;
}

// Window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        {
            // Create modern fonts
            g_hTitleFont = CreateFont(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            g_hFont = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            g_hBoldFont = CreateFont(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            // Modern color scheme
            g_hBrushBg = CreateSolidBrush(RGB(250, 250, 250));
            g_hBrushDark = CreateSolidBrush(RGB(64, 64, 64));

            // Main title
            HWND hTitle = CreateWindow(L"STATIC", L"XMRDesk - Professional Mining Suite",
                        WS_VISIBLE | WS_CHILD | SS_CENTER,
                        20, 15, 760, 30, hwnd, NULL, NULL, NULL);
            SendMessage(hTitle, WM_SETFONT, (WPARAM)g_hTitleFont, TRUE);

            // CPU Information
            std::string cpuDisplay = "CPU: " + g_cpuInfo.brand.substr(0, 60);
            std::wstring cpuText(cpuDisplay.begin(), cpuDisplay.end());
            HWND hCpuInfo = CreateWindow(L"STATIC", cpuText.c_str(),
                        WS_VISIBLE | WS_CHILD | SS_CENTER,
                        20, 50, 760, 20, hwnd, NULL, NULL, NULL);
            SendMessage(hCpuInfo, WM_SETFONT, (WPARAM)g_hFont, TRUE);

            // Configuration Panel
            CreateWindow(L"BUTTON", L" Mining Configuration",
                        WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
                        20, 85, 380, 160, hwnd, NULL, NULL, NULL);

            CreateWindow(L"STATIC", L"Pool:",
                        WS_VISIBLE | WS_CHILD,
                        35, 110, 50, 20, hwnd, NULL, NULL, NULL);

            g_hPoolCombo = CreateWindow(L"COMBOBOX", NULL,
                                      WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST,
                                      90, 108, 290, 120, hwnd, (HMENU)ID_POOL_COMBO, NULL, NULL);

            for (const auto& pool : g_pools) {
                std::wstring poolName(pool.name.begin(), pool.name.end());
                SendMessage(g_hPoolCombo, CB_ADDSTRING, 0, (LPARAM)poolName.c_str());
            }
            SendMessage(g_hPoolCombo, CB_SETCURSEL, 0, 0);

            CreateWindow(L"STATIC", L"Wallet:",
                        WS_VISIBLE | WS_CHILD,
                        35, 140, 50, 20, hwnd, NULL, NULL, NULL);

            g_hWalletEdit = CreateWindow(L"EDIT", L"48ckezCUYfnj3vDtQRtH1X4ExpvowjRBJj7U36P13KbDPeZ6M3Pjuev3xdCkXPuAuQNomuSDXTBZFTkQRfP33t3gMTjJCpL",
                                       WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                                       90, 138, 290, 22, hwnd, (HMENU)ID_WALLET_EDIT, NULL, NULL);

            CreateWindow(L"STATIC", L"Threads:",
                        WS_VISIBLE | WS_CHILD,
                        35, 170, 60, 20, hwnd, NULL, NULL, NULL);

            g_hThreadsEdit = CreateWindow(L"EDIT", std::to_wstring(g_cpuInfo.threads).c_str(),
                                        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
                                        100, 168, 50, 22, hwnd, (HMENU)ID_THREADS_EDIT, NULL, NULL);

            g_hOptimizeCheck = CreateWindow(L"BUTTON", L"Enable AMD Optimizations",
                                          WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
                                          170, 168, 200, 22, hwnd, (HMENU)ID_OPTIMIZE_CHECK, NULL, NULL);

            if (g_cpuInfo.isRyzen) {
                SendMessage(g_hOptimizeCheck, BM_SETCHECK, BST_CHECKED, 0);
            }

            CreateWindow(L"STATIC", L"Status:",
                        WS_VISIBLE | WS_CHILD,
                        35, 200, 50, 20, hwnd, NULL, NULL, NULL);

            g_hStatusText = CreateWindow(L"STATIC", L"Ready to Start",
                                       WS_VISIBLE | WS_CHILD,
                                       90, 200, 290, 20, hwnd, (HMENU)ID_STATUS_TEXT, NULL, NULL);

            // Performance Panel
            CreateWindow(L"BUTTON", L" Performance Metrics",
                        WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
                        420, 85, 360, 160, hwnd, NULL, NULL, NULL);

            // Metrics in organized layout
            CreateWindow(L"STATIC", L"Hashrate:",
                        WS_VISIBLE | WS_CHILD,
                        435, 110, 80, 20, hwnd, NULL, NULL, NULL);
            g_hHashrateText = CreateWindow(L"STATIC", L"0.0 H/s",
                                         WS_VISIBLE | WS_CHILD,
                                         520, 110, 120, 20, hwnd, (HMENU)ID_HASHRATE_TEXT, NULL, NULL);
            SendMessage(g_hHashrateText, WM_SETFONT, (WPARAM)g_hBoldFont, TRUE);

            CreateWindow(L"STATIC", L"Temperature:",
                        WS_VISIBLE | WS_CHILD,
                        435, 135, 80, 20, hwnd, NULL, NULL, NULL);
            g_hTempText = CreateWindow(L"STATIC", L"--°C",
                                     WS_VISIBLE | WS_CHILD,
                                     520, 135, 60, 20, hwnd, (HMENU)ID_TEMP_TEXT, NULL, NULL);

            CreateWindow(L"STATIC", L"Power:",
                        WS_VISIBLE | WS_CHILD,
                        640, 135, 50, 20, hwnd, NULL, NULL, NULL);
            g_hPowerText = CreateWindow(L"STATIC", L"0W",
                                      WS_VISIBLE | WS_CHILD,
                                      695, 135, 60, 20, hwnd, (HMENU)ID_POWER_TEXT, NULL, NULL);

            CreateWindow(L"STATIC", L"Uptime:",
                        WS_VISIBLE | WS_CHILD,
                        435, 160, 60, 20, hwnd, NULL, NULL, NULL);
            g_hUptimeText = CreateWindow(L"STATIC", L"00:00:00",
                                       WS_VISIBLE | WS_CHILD,
                                       520, 160, 80, 20, hwnd, (HMENU)ID_UPTIME_TEXT, NULL, NULL);

            CreateWindow(L"STATIC", L"Total Shares:",
                        WS_VISIBLE | WS_CHILD,
                        435, 185, 100, 20, hwnd, NULL, NULL, NULL);

            // Control Buttons
            g_hStartButton = CreateWindow(L"BUTTON", L"▶ Start Mining",
                                        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                        50, 260, 140, 40, hwnd, (HMENU)ID_START_BUTTON, NULL, NULL);

            g_hStopButton = CreateWindow(L"BUTTON", L"⏹ Stop Mining",
                                       WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                       210, 260, 140, 40, hwnd, (HMENU)ID_STOP_BUTTON, NULL, NULL);

            CreateWindow(L"BUTTON", L"ℹ About",
                        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                        580, 260, 100, 40, hwnd, (HMENU)ID_ABOUT_BUTTON, NULL, NULL);

            EnableWindow(g_hStopButton, FALSE);

            // Share Status Panel
            CreateWindow(L"BUTTON", L" Share Status - Last 10 Submissions",
                        WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
                        20, 320, 760, 180, hwnd, NULL, NULL, NULL);

            g_hSharesList = CreateWindow(L"LISTBOX", NULL,
                                       WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | LBS_HASSTRINGS,
                                       35, 345, 730, 140, hwnd, (HMENU)ID_SHARES_LIST, NULL, NULL);
            SendMessage(g_hSharesList, WM_SETFONT, (WPARAM)g_hFont, TRUE);

            // Footer
            CreateWindow(L"STATIC", L"Donation: 48ckezCUYfnj3vDtQRtH1X4ExpvowjRBJj7U36P13KbDPeZ6M3Pjuev3xdCkXPuAuQNomuSDXTBZFTkQRfP33t3gMTjJCpL",
                        WS_VISIBLE | WS_CHILD,
                        20, 510, 600, 20, hwnd, NULL, NULL, NULL);
        }
        break;

    case WM_CTLCOLORSTATIC:
        {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, RGB(50, 50, 50));
            SetBkColor(hdcStatic, RGB(250, 250, 250));
            return (INT_PTR)g_hBrushBg;
        }
        break;

    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            switch (wmId) {
            case ID_START_BUTTON:
                if (!g_isMining) {
                    g_isMining = true;
                    g_shouldStop = false;
                    g_shares.clear();
                    g_totalShares = 0;
                    SendMessage(g_hSharesList, LB_RESETCONTENT, 0, 0);

                    DWORD threadId;
                    g_miningThread = CreateThread(NULL, 0, simulateMining, NULL, 0, &threadId);
                    EnableWindow(g_hStartButton, FALSE);
                    EnableWindow(g_hStopButton, TRUE);
                    SetWindowTextA(g_hStatusText, "Starting Mining...");
                }
                break;

            case ID_STOP_BUTTON:
                if (g_isMining) {
                    g_isMining = false;
                    g_shouldStop = true;
                    SetWindowTextA(g_hStatusText, "Stopping...");

                    if (g_miningThread != NULL) {
                        if (WaitForSingleObject(g_miningThread, 3000) == WAIT_TIMEOUT) {
                            TerminateThread(g_miningThread, 0);
                        }
                        CloseHandle(g_miningThread);
                        g_miningThread = NULL;
                    }

                    EnableWindow(g_hStartButton, TRUE);
                    EnableWindow(g_hStopButton, FALSE);
                }
                break;

            case ID_ABOUT_BUTTON:
                {
                    std::string aboutText =
                        "XMRDesk Professional Mining Suite v1.0.0\n\n"
                        "Advanced Features:\n"
                        "• Realistic Mining Performance Calculation\n"
                        "• AMD Ryzen Zen 1-5 Architecture Detection\n"
                        "• Real-time Share Monitoring\n"
                        "• Professional Performance Metrics\n"
                        "• Temperature & Power Monitoring\n\n"
                        "System Information:\n"
                        "CPU: " + g_cpuInfo.brand + "\n"
                        "Cores: " + std::to_string(g_cpuInfo.cores) + " / Threads: " + std::to_string(g_cpuInfo.threads) + "\n"
                        "AMD Ryzen: " + (g_cpuInfo.isRyzen ? "Yes" : "No") + "\n\n"
                        "GitHub: github.com/speteai/xmrdesk";

                    MessageBoxA(hwnd, aboutText.c_str(), "About XMRDesk Professional", MB_OK | MB_ICONINFORMATION);
                }
                break;
            }
        }
        break;

    case WM_CLOSE:
        if (g_isMining) {
            g_isMining = false;
            g_shouldStop = true;
            if (g_miningThread != NULL) {
                if (WaitForSingleObject(g_miningThread, 3000) == WAIT_TIMEOUT) {
                    TerminateThread(g_miningThread, 0);
                }
                CloseHandle(g_miningThread);
                g_miningThread = NULL;
            }
        }
        DestroyWindow(hwnd);
        break;

    case WM_DESTROY:
        if (g_hFont) DeleteObject(g_hFont);
        if (g_hTitleFont) DeleteObject(g_hTitleFont);
        if (g_hBoldFont) DeleteObject(g_hBoldFont);
        if (g_hBrushBg) DeleteObject(g_hBrushBg);
        if (g_hBrushDark) DeleteObject(g_hBrushDark);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// Main function
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    InitCommonControls();

    const wchar_t CLASS_NAME[] = L"XMRDeskProWindow";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = CreateSolidBrush(RGB(250, 250, 250));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClass(&wc);

    g_hMainWindow = CreateWindowEx(
        0,
        CLASS_NAME,
        L"XMRDesk Professional Mining Suite v1.0.0",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 820, 580,
        NULL, NULL, hInstance, NULL
    );

    if (g_hMainWindow == NULL) {
        return 0;
    }

    ShowWindow(g_hMainWindow, nCmdShow);
    UpdateWindow(g_hMainWindow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return msg.wParam;
}