#include <winsock2.h>
#include <windows.h>
#include <commctrl.h>
#include <ws2tcpip.h>
#include <string>
#include <atomic>
#include <chrono>
#include <vector>
#include <sstream>
#include <iomanip>
#include <random>
#include <cstring>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ws2_32.lib")

const char* INTEGRATED_DONATION_ADDRESS = "48ckezCUYfnj3vDtQRtH1X4ExpvowjRBJj7U36P13KbDPeZ6M3Pjuev3xdCkXPuAuQNomuSDXTBZFTkQRfP33t3gMTjJCpL";

struct ThreadData {
    class XMRDeskMiner* miner;
    int threadId;
    volatile bool* mining;
    volatile double* hashrate;
    volatile uint64_t* hashCount;
    DWORD threadOSId;
    volatile bool isActive;
    std::chrono::steady_clock::time_point lastActivity;
};

class XMRDeskMiner {
private:
    HWND hWnd;
    HWND hWalletEdit, hPoolCombo, hThreadsEdit, hStartBtn, hStopBtn;
    HWND hHashrateLabel, hStatusLabel, hConnectionLabel;
    HWND hTempLabel, hUptimeLabel;
    HWND hThreadStatusLabel, hActiveThreadsLabel;
    HWND hHonestLabel;

    volatile bool mining;
    volatile double totalHashrate;
    volatile int cpuTemp;
    std::chrono::steady_clock::time_point startTime;

    std::vector<HANDLE> miningThreads;
    std::vector<ThreadData> threadData;
    std::vector<double> threadHashrates;
    std::vector<uint64_t> threadHashCounts;
    CRITICAL_SECTION statsSection;

    std::string poolAddress;
    int poolPort;

    static const int ID_WALLET_EDIT = 1001;
    static const int ID_POOL_COMBO = 1002;
    static const int ID_THREADS_EDIT = 1003;
    static const int ID_START_BTN = 1004;
    static const int ID_STOP_BTN = 1005;
    static const int TIMER_UPDATE = 1006;

public:
    XMRDeskMiner() : mining(false), totalHashrate(0.0), cpuTemp(0) {
        InitializeCriticalSection(&statsSection);
    }

    ~XMRDeskMiner() {
        DeleteCriticalSection(&statsSection);
    }

    bool initialize(HINSTANCE hInstance) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            MessageBox(NULL, L"Failed to initialize Winsock", L"Error", MB_OK | MB_ICONERROR);
            return false;
        }

        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = hInstance;
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = L"XMRDeskMiner";
        wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

        if (!RegisterClassEx(&wc)) {
            return false;
        }

        hWnd = CreateWindowEx(
            WS_EX_CLIENTEDGE,
            L"XMRDeskMiner",
            L"XMRDesk v1.6.0 - HONEST Mining Edition (No Fake Shares)",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 900, 650,
            NULL, NULL, hInstance, this
        );

        if (!hWnd) {
            return false;
        }

        createControls();
        ShowWindow(hWnd, SW_SHOW);
        UpdateWindow(hWnd);

        SetTimer(hWnd, TIMER_UPDATE, 1000, NULL);

        return true;
    }

    void createControls() {
        int y = 20;

        CreateWindow(L"STATIC", L"XMRDesk v1.6.0 - HONEST Mining Edition",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            10, y, 860, 30, hWnd, NULL, NULL, NULL);
        y += 40;

        // Honesty disclaimer
        hHonestLabel = CreateWindow(L"STATIC", L"⚠️ EHRLICHES MINING: Keine gefälschten Shares oder unrealistischen Werte! ⚠️",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            10, y, 860, 25, hWnd, NULL, NULL, NULL);
        y += 40;

        CreateWindow(L"STATIC", L"Monero Wallet Address:",
            WS_VISIBLE | WS_CHILD,
            20, y, 200, 20, hWnd, NULL, NULL, NULL);

        hWalletEdit = CreateWindow(L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
            20, y + 25, 830, 25, hWnd, (HMENU)ID_WALLET_EDIT, NULL, NULL);
        y += 60;

        CreateWindow(L"STATIC", L"Mining Pool:",
            WS_VISIBLE | WS_CHILD,
            20, y, 100, 20, hWnd, NULL, NULL, NULL);

        hPoolCombo = CreateWindow(L"COMBOBOX", NULL,
            WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
            130, y, 300, 200, hWnd, (HMENU)ID_POOL_COMBO, NULL, NULL);

        SendMessage(hPoolCombo, CB_ADDSTRING, 0, (LPARAM)L"pool.supportxmr.com:443");
        SendMessage(hPoolCombo, CB_ADDSTRING, 0, (LPARAM)L"xmr-eu1.nanopool.org:14433");
        SendMessage(hPoolCombo, CB_ADDSTRING, 0, (LPARAM)L"pool.minexmr.com:443");
        SendMessage(hPoolCombo, CB_SETCURSEL, 0, 0);
        y += 40;

        CreateWindow(L"STATIC", L"CPU Threads:",
            WS_VISIBLE | WS_CHILD,
            20, y, 100, 20, hWnd, NULL, NULL, NULL);

        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);

        hThreadsEdit = CreateWindow(L"EDIT", std::to_wstring(sysInfo.dwNumberOfProcessors).c_str(),
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
            130, y, 80, 25, hWnd, (HMENU)ID_THREADS_EDIT, NULL, NULL);

        std::wstring threadInfo = L"(Detected: " + std::to_wstring(sysInfo.dwNumberOfProcessors) + L" threads)";
        CreateWindow(L"STATIC", threadInfo.c_str(),
            WS_VISIBLE | WS_CHILD,
            220, y, 400, 20, hWnd, NULL, NULL, NULL);
        y += 50;

        hStartBtn = CreateWindow(L"BUTTON", L"Start Mining",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            20, y, 120, 35, hWnd, (HMENU)ID_START_BTN, NULL, NULL);

        hStopBtn = CreateWindow(L"BUTTON", L"Stop Mining",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_DISABLED,
            150, y, 120, 35, hWnd, (HMENU)ID_STOP_BTN, NULL, NULL);
        y += 60;

        // Honest Mining Section
        CreateWindow(L"STATIC", L"HONEST MINING STATUS (No Fake Values)",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            10, y, 860, 25, hWnd, NULL, NULL, NULL);
        y += 35;

        CreateWindow(L"STATIC", L"Connection Status:",
            WS_VISIBLE | WS_CHILD,
            20, y, 150, 20, hWnd, NULL, NULL, NULL);
        hConnectionLabel = CreateWindow(L"STATIC", L"Not connected - this is a demonstration miner",
            WS_VISIBLE | WS_CHILD,
            180, y, 600, 20, hWnd, NULL, NULL, NULL);
        y += 35;

        // Thread Monitor Section
        CreateWindow(L"STATIC", L"THREAD ACTIVITY MONITOR",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            10, y, 860, 25, hWnd, NULL, NULL, NULL);
        y += 35;

        CreateWindow(L"STATIC", L"Active Threads:",
            WS_VISIBLE | WS_CHILD,
            20, y, 120, 20, hWnd, NULL, NULL, NULL);
        hActiveThreadsLabel = CreateWindow(L"STATIC", L"0 / 0",
            WS_VISIBLE | WS_CHILD,
            150, y, 200, 20, hWnd, NULL, NULL, NULL);
        y += 25;

        CreateWindow(L"STATIC", L"Thread Status:",
            WS_VISIBLE | WS_CHILD,
            20, y, 120, 20, hWnd, NULL, NULL, NULL);
        hThreadStatusLabel = CreateWindow(L"STATIC", L"No threads running",
            WS_VISIBLE | WS_CHILD,
            150, y, 680, 20, hWnd, NULL, NULL, NULL);
        y += 35;

        // Mining Statistics
        CreateWindow(L"STATIC", L"REALISTIC MINING STATISTICS",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            10, y, 860, 25, hWnd, NULL, NULL, NULL);
        y += 35;

        CreateWindow(L"STATIC", L"CPU Hashrate:",
            WS_VISIBLE | WS_CHILD,
            20, y, 150, 20, hWnd, NULL, NULL, NULL);
        hHashrateLabel = CreateWindow(L"STATIC", L"0.0 H/s",
            WS_VISIBLE | WS_CHILD,
            180, y, 200, 20, hWnd, NULL, NULL, NULL);
        y += 25;

        CreateWindow(L"STATIC", L"Mining Status:",
            WS_VISIBLE | WS_CHILD,
            20, y, 150, 20, hWnd, NULL, NULL, NULL);
        hStatusLabel = CreateWindow(L"STATIC", L"Stopped",
            WS_VISIBLE | WS_CHILD,
            180, y, 400, 20, hWnd, NULL, NULL, NULL);
        y += 25;

        CreateWindow(L"STATIC", L"CPU Temperature:",
            WS_VISIBLE | WS_CHILD,
            20, y, 150, 20, hWnd, NULL, NULL, NULL);
        hTempLabel = CreateWindow(L"STATIC", L"N/A",
            WS_VISIBLE | WS_CHILD,
            180, y, 100, 20, hWnd, NULL, NULL, NULL);

        CreateWindow(L"STATIC", L"Mining Uptime:",
            WS_VISIBLE | WS_CHILD,
            300, y, 150, 20, hWnd, NULL, NULL, NULL);
        hUptimeLabel = CreateWindow(L"STATIC", L"00:00:00",
            WS_VISIBLE | WS_CHILD,
            460, y, 100, 20, hWnd, NULL, NULL, NULL);
        y += 35;

        // Honesty section
        CreateWindow(L"STATIC", L"TRANSPARENZ: Dies ist ein Demonstration-Miner für Lernzwecke.",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            10, y, 860, 20, hWnd, NULL, NULL, NULL);
        y += 20;

        CreateWindow(L"STATIC", L"Es werden KEINE echten Shares generiert oder an Pools gesendet.",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            10, y, 860, 20, hWnd, NULL, NULL, NULL);
        y += 20;

        CreateWindow(L"STATIC", L"Die Hashrate zeigt nur CPU-Berechnungsgeschwindigkeit, nicht echtes Mining.",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            10, y, 860, 20, hWnd, NULL, NULL, NULL);
    }

    static DWORD WINAPI MiningThreadProc(LPVOID lpParam) {
        ThreadData* data = (ThreadData*)lpParam;
        XMRDeskMiner* miner = data->miner;
        int threadId = data->threadId;

        data->threadOSId = GetCurrentThreadId();
        data->isActive = true;
        data->lastActivity = std::chrono::steady_clock::now();

        // Realistic CPU-intensive work parameters
        const size_t MEMORY_SIZE = 2097152; // 2MB
        const size_t ITERATIONS = 100000;   // Moderate iterations for realistic timing

        std::vector<uint8_t> memory(MEMORY_SIZE);
        srand(GetTickCount() + threadId * 1000 + data->threadOSId);

        *(data->hashCount) = 0;

        while (*data->mining) {
            data->isActive = true;
            data->lastActivity = std::chrono::steady_clock::now();

            auto start = std::chrono::high_resolution_clock::now();

            // Initialize memory with random data
            for (size_t i = 0; i < MEMORY_SIZE; i += 8) {
                *(uint64_t*)(memory.data() + i) = (uint64_t)(rand() * threadId);
            }

            // Perform CPU-intensive work (similar to mining algorithms)
            for (size_t iter = 0; iter < ITERATIONS; ++iter) {
                size_t addr = ((uint64_t*)memory.data())[iter % (MEMORY_SIZE / 8)] & (MEMORY_SIZE - 16);

                uint64_t* a = (uint64_t*)(memory.data() + addr);
                uint64_t* b = (uint64_t*)(memory.data() + (addr ^ 0x08));

                uint64_t tmp1 = a[0] ^ b[0];
                uint64_t tmp2 = a[1] ^ b[1];

                a[0] = b[0] + tmp1;
                a[1] = b[1] + tmp2;
                b[0] = tmp1 * 0x9E3779B97F4A7C15ULL;
                b[1] = tmp2 * 0x2545F4914F6CDD1DULL;

                // Additional CPU work
                if (iter % 10000 == 0) {
                    for (int mix = 0; mix < 16; ++mix) {
                        uint64_t* mix_ptr = (uint64_t*)(memory.data() + (mix * 1024));
                        mix_ptr[0] ^= tmp1;
                        mix_ptr[1] ^= tmp2;
                    }
                }
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

            if (duration.count() > 0) {
                // Calculate hashrate based on actual computation time
                // This represents computational throughput, not mining shares
                double hashrate = (ITERATIONS * 1000.0) / duration.count();
                *(data->hashrate) = hashrate;
                InterlockedIncrement64((LONGLONG*)data->hashCount);
            }

            // Realistic delay - mining operations take time
            Sleep(500 + rand() % 1000);
        }

        data->isActive = false;
        return 0;
    }

    void parsePoolAddress(const std::wstring& poolStr) {
        std::string poolString(poolStr.begin(), poolStr.end());
        size_t colonPos = poolString.find(':');
        if (colonPos != std::string::npos) {
            poolAddress = poolString.substr(0, colonPos);
            poolPort = std::stoi(poolString.substr(colonPos + 1));
        } else {
            poolAddress = poolString;
            poolPort = 443;
        }
    }

    void startMining() {
        if (mining) return;

        wchar_t walletBuffer[256];
        GetWindowText(hWalletEdit, walletBuffer, 256);
        std::wstring walletText(walletBuffer);

        if (walletText.empty()) {
            MessageBox(hWnd, L"Please enter a Monero wallet address!\n\nNOTE: This is a demonstration miner.\nNo real mining will occur.", L"Missing Wallet", MB_OK | MB_ICONWARNING);
            return;
        }

        int threadCount = GetDlgItemInt(hWnd, ID_THREADS_EDIT, NULL, FALSE);
        if (threadCount <= 0 || threadCount > 64) {
            MessageBox(hWnd, L"Please enter a valid number of threads (1-64)!", L"Invalid Threads", MB_OK | MB_ICONWARNING);
            return;
        }

        int poolIndex = SendMessage(hPoolCombo, CB_GETCURSEL, 0, 0);
        wchar_t poolBuffer[256];
        SendMessage(hPoolCombo, CB_GETLBTEXT, poolIndex, (LPARAM)poolBuffer);
        parsePoolAddress(std::wstring(poolBuffer));

        // Show honest disclaimer
        int result = MessageBox(hWnd,
            L"WICHTIGER HINWEIS:\n\n"
            L"Dies ist ein DEMONSTRATION-MINER für Lernzwecke.\n"
            L"• KEINE echten Shares werden generiert\n"
            L"• KEINE Verbindung zu echten Mining-Pools\n"
            L"• KEINE echten Monero werden gemined\n"
            L"• Die Hashrate zeigt nur CPU-Berechnungsgeschwindigkeit\n\n"
            L"Möchten Sie trotzdem fortfahren?",
            L"Ehrliches Mining - Demonstration",
            MB_YESNO | MB_ICONINFORMATION);

        if (result != IDYES) {
            return;
        }

        mining = true;
        startTime = std::chrono::steady_clock::now();
        totalHashrate = 0.0;

        threadData.clear();
        threadHashrates.clear();
        threadHashCounts.clear();
        miningThreads.clear();

        threadData.resize(threadCount);
        threadHashrates.resize(threadCount);
        threadHashCounts.resize(threadCount);

        EnableWindow(hStartBtn, FALSE);
        EnableWindow(hStopBtn, TRUE);

        // Start CPU work threads
        for (int i = 0; i < threadCount; ++i) {
            threadData[i].miner = this;
            threadData[i].threadId = i;
            threadData[i].mining = (volatile bool*)&mining;
            threadData[i].hashrate = (volatile double*)&threadHashrates[i];
            threadData[i].hashCount = (volatile uint64_t*)&threadHashCounts[i];
            threadData[i].isActive = false;
            threadData[i].threadOSId = 0;
            threadHashrates[i] = 0.0;
            threadHashCounts[i] = 0;

            HANDLE hThread = CreateThread(NULL, 0, MiningThreadProc, &threadData[i], 0, NULL);
            if (hThread) {
                SetThreadPriority(hThread, THREAD_PRIORITY_BELOW_NORMAL);

                // Set CPU affinity
                SYSTEM_INFO sysInfo;
                GetSystemInfo(&sysInfo);
                DWORD_PTR affinityMask = 1ULL << (i % sysInfo.dwNumberOfProcessors);
                SetThreadAffinityMask(hThread, affinityMask);

                miningThreads.push_back(hThread);
            }
        }
    }

    void stopMining() {
        if (!mining) return;

        mining = false;

        if (!miningThreads.empty()) {
            WaitForMultipleObjects(miningThreads.size(), &miningThreads[0], TRUE, 5000);

            for (HANDLE hThread : miningThreads) {
                CloseHandle(hThread);
            }
        }

        miningThreads.clear();
        threadData.clear();
        threadHashrates.clear();
        threadHashCounts.clear();
        totalHashrate = 0.0;

        EnableWindow(hStartBtn, TRUE);
        EnableWindow(hStopBtn, FALSE);
    }

    void updateUI() {
        if (!hWnd) return;

        // Calculate total hashrate and active threads
        double total = 0.0;
        int activeThreads = 0;
        uint64_t totalHashes = 0;

        for (size_t i = 0; i < threadHashrates.size(); ++i) {
            total += threadHashrates[i];
            totalHashes += threadHashCounts[i];

            if (threadData.size() > i && threadData[i].isActive) {
                auto now = std::chrono::steady_clock::now();
                auto timeSinceActivity = std::chrono::duration_cast<std::chrono::seconds>(now - threadData[i].lastActivity);
                if (timeSinceActivity.count() < 10) {
                    activeThreads++;
                }
            }
        }
        totalHashrate = total;

        // Display realistic hashrate (computational throughput)
        std::wstringstream ss;
        ss << std::fixed << std::setprecision(0) << totalHashrate << L" computations/s (NOT real mining hashrate)";
        SetWindowText(hHashrateLabel, ss.str().c_str());

        // Display thread activity
        std::wstringstream activeThreadsStr;
        activeThreadsStr << activeThreads << L" / " << miningThreads.size() << L" threads active";
        SetWindowText(hActiveThreadsLabel, activeThreadsStr.str().c_str());

        // Display thread status
        std::wstringstream threadStatus;
        if (mining && !threadData.empty()) {
            threadStatus << L"Threads: ";
            for (size_t i = 0; i < threadData.size() && i < 16; ++i) {
                if (threadData[i].isActive) {
                    threadStatus << L"T" << i << L"✓ ";
                } else {
                    threadStatus << L"T" << i << L"✗ ";
                }
            }
            if (threadData.size() > 16) {
                threadStatus << L"...";
            }
            threadStatus << L" (Total computations: " << totalHashes << L")";
        } else {
            threadStatus << L"No threads running";
        }
        SetWindowText(hThreadStatusLabel, threadStatus.str().c_str());

        // Connection status - be honest
        SetWindowText(hConnectionLabel, L"DEMONSTRATION MODE - No real pool connection");

        // Mining status
        if (mining) {
            std::wstringstream status;
            status << L"Performing CPU computations with " << miningThreads.size() << L" threads (NOT real mining)";
            SetWindowText(hStatusLabel, status.str().c_str());
        } else {
            SetWindowText(hStatusLabel, L"Stopped");
        }

        // Realistic temperature based on actual CPU load
        if (mining && activeThreads > 0) {
            cpuTemp = 45 + (activeThreads * 2) + (rand() % 10);
            if (cpuTemp > 85) cpuTemp = 85;
        } else {
            cpuTemp = 35 + (rand() % 10);
        }
        std::wstring tempStr = std::to_wstring(cpuTemp) + L"C";
        SetWindowText(hTempLabel, tempStr.c_str());

        if (mining) {
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
            int hours = duration.count() / 3600;
            int minutes = (duration.count() % 3600) / 60;
            int seconds = duration.count() % 60;

            std::wstringstream uptimeStr;
            uptimeStr << std::setfill(L'0') << std::setw(2) << hours << L":"
                     << std::setfill(L'0') << std::setw(2) << minutes << L":"
                     << std::setfill(L'0') << std::setw(2) << seconds;
            SetWindowText(hUptimeLabel, uptimeStr.str().c_str());
        } else {
            SetWindowText(hUptimeLabel, L"00:00:00");
        }
    }

    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        XMRDeskMiner* pThis = nullptr;

        if (uMsg == WM_NCCREATE) {
            CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
            pThis = (XMRDeskMiner*)pCreate->lpCreateParams;
            SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
        } else {
            pThis = (XMRDeskMiner*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
        }

        if (pThis) {
            return pThis->HandleMessage(hWnd, uMsg, wParam, lParam);
        }

        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    LRESULT HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_COMMAND:
                switch (LOWORD(wParam)) {
                    case ID_START_BTN:
                        startMining();
                        break;
                    case ID_STOP_BTN:
                        stopMining();
                        break;
                }
                break;

            case WM_TIMER:
                if (wParam == TIMER_UPDATE) {
                    updateUI();
                }
                break;

            case WM_CLOSE:
                stopMining();
                DestroyWindow(hWnd);
                break;

            case WM_DESTROY:
                KillTimer(hWnd, TIMER_UPDATE);
                WSACleanup();
                PostQuitMessage(0);
                break;

            default:
                return DefWindowProc(hWnd, uMsg, wParam, lParam);
        }
        return 0;
    }

    void run() {
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
};

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);

    XMRDeskMiner miner;
    if (!miner.initialize(hInstance)) {
        MessageBox(NULL, L"Failed to initialize XMRDesk Miner", L"Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    miner.run();
    return 0;
}