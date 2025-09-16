#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <fstream>

// Windows GUI includes
#include <commctrl.h>
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
#define ID_LOG_TEXT       1014

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
HWND g_hLogText;
HFONT g_hFont;
HFONT g_hTitleFont;
HBRUSH g_hBrushBg;
bool g_isMining = false;
HANDLE g_miningProcess = NULL;
HANDLE g_logThread = NULL;
volatile bool g_shouldStop = false;

// Mining data
std::deque<std::string> g_shares;
std::deque<std::string> g_logs;
int g_totalShares = 0;
time_t g_startTime = 0;
double g_currentHashrate = 0;

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
    {"F2Pool", "xmr.f2pool.com", "13531"},
    {"HashVault.pro", "pool.hashvault.pro", "5555"}
};

// Add log message
void addLogMessage(const std::string& message) {
    time_t now = time(0);
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", localtime(&now));

    std::string logMsg = std::string(timeStr) + " " + message;
    g_logs.push_front(logMsg);

    if (g_logs.size() > 50) {
        g_logs.pop_back();
    }

    // Update log display
    if (IsWindow(g_hLogText)) {
        std::string allLogs;
        for (const auto& log : g_logs) {
            allLogs += log + "\r\n";
        }
        SetWindowTextA(g_hLogText, allLogs.c_str());
        // Scroll to bottom
        SendMessage(g_hLogText, WM_VSCROLL, SB_BOTTOM, 0);
    }
}

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

    if (IsWindow(g_hSharesList)) {
        SendMessage(g_hSharesList, LB_RESETCONTENT, 0, 0);
        for (const auto& share : g_shares) {
            SendMessageA(g_hSharesList, LB_ADDSTRING, 0, (LPARAM)share.c_str());
        }
    }
}

// Create XMRig configuration
void createXMRigConfig(const std::string& pool, const std::string& wallet, int threads, bool optimize) {
    std::ofstream config("config.json");

    config << "{\n";
    config << "  \"autosave\": true,\n";
    config << "  \"cpu\": {\n";
    config << "    \"enabled\": true,\n";
    config << "    \"huge-pages\": " << (optimize ? "true" : "false") << ",\n";
    config << "    \"huge-pages-jit\": " << (optimize ? "true" : "false") << ",\n";
    config << "    \"hw-aes\": null,\n";
    config << "    \"priority\": null,\n";
    config << "    \"memory-pool\": false,\n";
    config << "    \"yield\": true,\n";
    config << "    \"max-threads-hint\": " << threads << ",\n";
    config << "    \"asm\": true,\n";
    config << "    \"cn/0\": false,\n";
    config << "    \"cn-lite/0\": false\n";
    config << "  },\n";
    config << "  \"opencl\": {\n";
    config << "    \"enabled\": false\n";
    config << "  },\n";
    config << "  \"cuda\": {\n";
    config << "    \"enabled\": false\n";
    config << "  },\n";
    config << "  \"pools\": [\n";
    config << "    {\n";
    config << "      \"coin\": \"monero\",\n";
    config << "      \"algo\": \"rx/0\",\n";
    config << "      \"url\": \"" << pool << "\",\n";
    config << "      \"user\": \"" << wallet << "\",\n";
    config << "      \"pass\": \"XMRDesk\",\n";
    config << "      \"rig-id\": \"XMRDesk\",\n";
    config << "      \"nicehash\": false,\n";
    config << "      \"keepalive\": true,\n";
    config << "      \"enabled\": true,\n";
    config << "      \"tls\": false,\n";
    config << "      \"daemon\": false,\n";
    config << "      \"socks5\": null,\n";
    config << "      \"self-select\": null\n";
    config << "    }\n";
    config << "  ],\n";
    config << "  \"log-file\": \"xmrig.log\",\n";
    config << "  \"health-print-time\": 60,\n";
    config << "  \"print-time\": 15,\n";
    config << "  \"retries\": 5,\n";
    config << "  \"retry-pause\": 5,\n";
    config << "  \"syslog\": false,\n";
    config << "  \"user-agent\": \"XMRDesk/1.0.0\",\n";
    config << "  \"verbose\": 1,\n";
    config << "  \"watch\": true,\n";
    config << "  \"pause-on-battery\": false\n";
    config << "}\n";

    config.close();
    addLogMessage("Created XMRig configuration");
}

// Monitor XMRig log file
DWORD WINAPI monitorXMRigLog(LPVOID lpParam) {
    addLogMessage("Starting XMRig log monitor...");

    while (g_isMining && !g_shouldStop) {
        std::ifstream logFile("xmrig.log");
        if (logFile.is_open()) {
            std::string line;
            while (std::getline(logFile, line)) {
                // Parse hashrate
                if (line.find("speed") != std::string::npos && line.find("H/s") != std::string::npos) {
                    size_t pos = line.find("speed");
                    if (pos != std::string::npos) {
                        std::string speedPart = line.substr(pos);
                        size_t start = speedPart.find(" ") + 1;
                        size_t end = speedPart.find(" H/s");
                        if (start != std::string::npos && end != std::string::npos) {
                            std::string hashrateStr = speedPart.substr(start, end - start);
                            g_currentHashrate = std::stod(hashrateStr);

                            if (IsWindow(g_hHashrateText)) {
                                std::stringstream ss;
                                ss << std::fixed << std::setprecision(1) << g_currentHashrate << " H/s";
                                SetWindowTextA(g_hHashrateText, ss.str().c_str());
                            }
                        }
                    }
                }

                // Parse shares
                if (line.find("accepted") != std::string::npos) {
                    g_totalShares++;
                    addShare("Share " + std::to_string(g_totalShares) + " - ACCEPTED");
                    addLogMessage("SHARE ACCEPTED!");
                }

                if (line.find("rejected") != std::string::npos) {
                    g_totalShares++;
                    addShare("Share " + std::to_string(g_totalShares) + " - REJECTED");
                    addLogMessage("Share rejected");
                }

                // Parse connection status
                if (line.find("use pool") != std::string::npos) {
                    addLogMessage("Connected to mining pool");
                    if (IsWindow(g_hStatusText)) {
                        SetWindowTextA(g_hStatusText, "Connected to Pool - Mining Active");
                    }
                }

                if (line.find("READY") != std::string::npos) {
                    addLogMessage("XMRig ready and mining");
                }
            }
            logFile.close();
        }

        // Update uptime
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

        Sleep(2000); // Check every 2 seconds
    }

    return 0;
}

// Start real mining
void startRealMining() {
    // Get pool selection
    int poolIndex = SendMessage(g_hPoolCombo, CB_GETCURSEL, 0, 0);
    if (poolIndex == CB_ERR) poolIndex = 0;

    std::string poolUrl = g_pools[poolIndex].url + ":" + g_pools[poolIndex].port;

    // Get wallet address
    char wallet[256];
    GetWindowTextA(g_hWalletEdit, wallet, sizeof(wallet));

    // Get thread count
    char threadsStr[10];
    GetWindowTextA(g_hThreadsEdit, threadsStr, sizeof(threadsStr));
    int threads = atoi(threadsStr);
    if (threads <= 0) threads = 4;

    // Get optimization setting
    bool optimize = SendMessage(g_hOptimizeCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;

    // Create configuration
    createXMRigConfig(poolUrl, std::string(wallet), threads, optimize);

    // Start XMRig process
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // Hide XMRig console
    ZeroMemory(&pi, sizeof(pi));

    // Try to start XMRig
    std::string command = "xmrig.exe --config=config.json";
    if (CreateProcessA(NULL, const_cast<char*>(command.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        g_miningProcess = pi.hProcess;
        CloseHandle(pi.hThread);
        addLogMessage("XMRig process started successfully");

        // Start log monitoring thread
        DWORD threadId;
        g_logThread = CreateThread(NULL, 0, monitorXMRigLog, NULL, 0, &threadId);

    } else {
        addLogMessage("ERROR: Could not start XMRig! Make sure xmrig.exe is in the same folder.");
        MessageBoxA(g_hMainWindow,
                   "XMRig executable not found!\n\n"
                   "Please download XMRig from:\n"
                   "https://github.com/xmrig/xmrig/releases\n\n"
                   "Extract xmrig.exe to the same folder as this program.",
                   "XMRig Not Found", MB_OK | MB_ICONWARNING);

        g_isMining = false;
        EnableWindow(g_hStartButton, TRUE);
        EnableWindow(g_hStopButton, FALSE);
    }
}

// Stop real mining
void stopRealMining() {
    addLogMessage("Stopping mining...");

    if (g_miningProcess != NULL) {
        TerminateProcess(g_miningProcess, 0);
        WaitForSingleObject(g_miningProcess, 3000);
        CloseHandle(g_miningProcess);
        g_miningProcess = NULL;
    }

    if (g_logThread != NULL) {
        g_shouldStop = true;
        WaitForSingleObject(g_logThread, 3000);
        CloseHandle(g_logThread);
        g_logThread = NULL;
    }

    g_currentHashrate = 0;
    if (IsWindow(g_hHashrateText)) SetWindowTextA(g_hHashrateText, "0.0 H/s");
    if (IsWindow(g_hStatusText)) SetWindowTextA(g_hStatusText, "Mining Stopped");

    addLogMessage("Mining stopped");
}

// Window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        {
            // Create fonts
            g_hTitleFont = CreateFontA(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                     ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

            g_hFont = CreateFontA(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

            g_hBrushBg = CreateSolidBrush(RGB(240, 240, 240));

            // Title
            HWND hTitle = CreateWindowA("STATIC", "XMRDesk - REAL Mining with XMRig Engine",
                        WS_VISIBLE | WS_CHILD | SS_CENTER,
                        10, 10, 760, 25, hwnd, NULL, NULL, NULL);
            SendMessage(hTitle, WM_SETFONT, (WPARAM)g_hTitleFont, TRUE);

            // Configuration
            CreateWindowA("BUTTON", " Mining Configuration",
                        WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
                        10, 45, 380, 120, hwnd, NULL, NULL, NULL);

            CreateWindowA("STATIC", "Pool:", WS_VISIBLE | WS_CHILD,
                        20, 70, 50, 20, hwnd, NULL, NULL, NULL);

            g_hPoolCombo = CreateWindowA("COMBOBOX", NULL,
                                       WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST,
                                       70, 68, 300, 100, hwnd, (HMENU)ID_POOL_COMBO, NULL, NULL);

            for (const auto& pool : g_pools) {
                SendMessageA(g_hPoolCombo, CB_ADDSTRING, 0, (LPARAM)pool.name.c_str());
            }
            SendMessage(g_hPoolCombo, CB_SETCURSEL, 0, 0);

            CreateWindowA("STATIC", "Wallet:", WS_VISIBLE | WS_CHILD,
                        20, 100, 50, 20, hwnd, NULL, NULL, NULL);

            g_hWalletEdit = CreateWindowA("EDIT", "48ckezCUYfnj3vDtQRtH1X4ExpvowjRBJj7U36P13KbDPeZ6M3Pjuev3xdCkXPuAuQNomuSDXTBZFTkQRfP33t3gMTjJCpL",
                                        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                                        20, 120, 350, 20, hwnd, (HMENU)ID_WALLET_EDIT, NULL, NULL);

            CreateWindowA("STATIC", "Threads:", WS_VISIBLE | WS_CHILD,
                        20, 145, 60, 20, hwnd, NULL, NULL, NULL);

            g_hThreadsEdit = CreateWindowA("EDIT", "4", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
                                         80, 143, 40, 20, hwnd, (HMENU)ID_THREADS_EDIT, NULL, NULL);

            g_hOptimizeCheck = CreateWindowA("BUTTON", "Enable optimizations",
                                           WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
                                           140, 143, 150, 20, hwnd, (HMENU)ID_OPTIMIZE_CHECK, NULL, NULL);
            SendMessage(g_hOptimizeCheck, BM_SETCHECK, BST_CHECKED, 0);

            // Performance metrics
            CreateWindowA("BUTTON", " Real-Time Mining Metrics",
                        WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
                        400, 45, 370, 120, hwnd, NULL, NULL, NULL);

            CreateWindowA("STATIC", "Hashrate:", WS_VISIBLE | WS_CHILD,
                        410, 70, 60, 20, hwnd, NULL, NULL, NULL);
            g_hHashrateText = CreateWindowA("STATIC", "0.0 H/s", WS_VISIBLE | WS_CHILD,
                                          480, 70, 100, 20, hwnd, (HMENU)ID_HASHRATE_TEXT, NULL, NULL);

            CreateWindowA("STATIC", "Status:", WS_VISIBLE | WS_CHILD,
                        410, 95, 50, 20, hwnd, NULL, NULL, NULL);
            g_hStatusText = CreateWindowA("STATIC", "Ready to Start", WS_VISIBLE | WS_CHILD,
                                        470, 95, 200, 20, hwnd, (HMENU)ID_STATUS_TEXT, NULL, NULL);

            CreateWindowA("STATIC", "Uptime:", WS_VISIBLE | WS_CHILD,
                        410, 120, 50, 20, hwnd, NULL, NULL, NULL);
            g_hUptimeText = CreateWindowA("STATIC", "00:00:00", WS_VISIBLE | WS_CHILD,
                                        470, 120, 80, 20, hwnd, (HMENU)ID_UPTIME_TEXT, NULL, NULL);

            CreateWindowA("STATIC", "Total Shares:", WS_VISIBLE | WS_CHILD,
                        410, 145, 80, 20, hwnd, NULL, NULL, NULL);

            // Control buttons
            g_hStartButton = CreateWindowA("BUTTON", "START REAL MINING",
                                         WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                         50, 180, 150, 35, hwnd, (HMENU)ID_START_BUTTON, NULL, NULL);

            g_hStopButton = CreateWindowA("BUTTON", "STOP MINING",
                                        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                        220, 180, 150, 35, hwnd, (HMENU)ID_STOP_BUTTON, NULL, NULL);

            CreateWindowA("BUTTON", "About", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                        580, 180, 80, 35, hwnd, (HMENU)ID_ABOUT_BUTTON, NULL, NULL);

            EnableWindow(g_hStopButton, FALSE);

            // Share status
            CreateWindowA("BUTTON", " Live Share Status",
                        WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
                        10, 230, 370, 150, hwnd, NULL, NULL, NULL);

            g_hSharesList = CreateWindowA("LISTBOX", NULL,
                                        WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL,
                                        20, 250, 350, 120, hwnd, (HMENU)ID_SHARES_LIST, NULL, NULL);

            // XMRig log
            CreateWindowA("BUTTON", " XMRig Mining Log",
                        WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
                        400, 230, 370, 150, hwnd, NULL, NULL, NULL);

            g_hLogText = CreateWindowA("EDIT", NULL,
                                     WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
                                     410, 250, 350, 120, hwnd, (HMENU)ID_LOG_TEXT, NULL, NULL);

            // Initialize log
            addLogMessage("XMRDesk Real Mining GUI started");
            addLogMessage("Place xmrig.exe in the same folder to start mining");
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
                    g_startTime = time(0);
                    SendMessage(g_hSharesList, LB_RESETCONTENT, 0, 0);

                    EnableWindow(g_hStartButton, FALSE);
                    EnableWindow(g_hStopButton, TRUE);
                    SetWindowTextA(g_hStatusText, "Starting XMRig...");

                    startRealMining();
                }
                break;

            case ID_STOP_BUTTON:
                if (g_isMining) {
                    g_isMining = false;
                    stopRealMining();
                    EnableWindow(g_hStartButton, TRUE);
                    EnableWindow(g_hStopButton, FALSE);
                }
                break;

            case ID_ABOUT_BUTTON:
                MessageBoxA(hwnd,
                           "XMRDesk Real Mining GUI v1.0.0\n\n"
                           "This version uses the actual XMRig mining engine!\n\n"
                           "Features:\n"
                           "- Real connection to mining pools\n"
                           "- Actual share submissions\n"
                           "- Live XMRig log monitoring\n"
                           "- Real hashrate display\n\n"
                           "Requirements:\n"
                           "- Download xmrig.exe from https://github.com/xmrig/xmrig/releases\n"
                           "- Place xmrig.exe in the same folder as this program\n\n"
                           "GitHub: github.com/speteai/xmrdesk",
                           "About XMRDesk Real Mining", MB_OK | MB_ICONINFORMATION);
                break;
            }
        }
        break;

    case WM_CLOSE:
        if (g_isMining) {
            stopRealMining();
        }
        DestroyWindow(hwnd);
        break;

    case WM_DESTROY:
        if (g_hFont) DeleteObject(g_hFont);
        if (g_hTitleFont) DeleteObject(g_hTitleFont);
        if (g_hBrushBg) DeleteObject(g_hBrushBg);
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

    const char CLASS_NAME[] = "XMRDeskRealMining";

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = CreateSolidBrush(RGB(240, 240, 240));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClassA(&wc);

    g_hMainWindow = CreateWindowExA(
        0, CLASS_NAME,
        "XMRDesk - REAL Mining with XMRig Engine",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 420,
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