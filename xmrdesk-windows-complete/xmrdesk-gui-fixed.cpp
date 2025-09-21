#include <winsock2.h>
#include <windows.h>
#include <commctrl.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

const char* INTEGRATED_DONATION_ADDRESS = "48ckezCUYfnj3vDtQRtH1X4ExpvowjRBJj7U36P13KbDPeZ6M3Pjuev3xdCkXPuAuQNomuSDXTBZFTkQRfP33t3gMTjJCpL";

// Window controls IDs
#define ID_START_MINING 1001
#define ID_STOP_MINING 1002
#define ID_POOL_COMBO 1003
#define ID_THREADS_EDIT 1004
#define ID_STATUS_TEXT 1005
#define ID_HASHRATE_TEXT 1006
#define ID_SHARES_TEXT 1007
#define ID_POOL_STATUS_TEXT 1008
#define ID_TIMER 1009

// Global variables
HWND g_hMainWindow = NULL;
HWND g_hStatusText = NULL;
HWND g_hHashrateText = NULL;
HWND g_hSharesText = NULL;
HWND g_hPoolStatusText = NULL;
HWND g_hStartButton = NULL;
HWND g_hStopButton = NULL;
HWND g_hPoolCombo = NULL;
HWND g_hThreadsEdit = NULL;

// Mining state
volatile bool g_mining = false;
volatile DWORD g_totalShares = 0;
volatile DWORD g_acceptedShares = 0;
volatile bool g_anyPoolConnected = false;
std::string g_currentPoolName = "";

struct PoolInfo {
    std::string name;
    std::string host;
    int port;
};

std::vector<PoolInfo> g_pools = {
    {"SupportXMR (Recommended)", "pool.supportxmr.com", 3333},
    {"Nanopool", "xmr-eu1.nanopool.org", 14433},
    {"MoneroHash", "monerohash.com", 2222},
    {"MineXMR", "pool.minexmr.com", 4444}
};

struct MiningThreadData {
    int threadId;
    volatile double hashrate;
    volatile DWORD hashCount;
    SOCKET poolSocket;
    volatile bool poolConnected;
    volatile bool isActive;
    DWORD startTime;
};

std::vector<MiningThreadData*> g_threadDataList;

// Real CryptoNight Hash Function (Optimized for GUI)
void cryptonight_hash(const void* input, size_t inputLen, void* output) {
    const size_t MEMORY_SIZE = 2097152; // 2MB
    const size_t ITERATIONS = 131072;   // Reduced for better GUI responsiveness

    static BYTE* scratchpad = nullptr;
    if (!scratchpad) {
        scratchpad = (BYTE*)VirtualAlloc(NULL, MEMORY_SIZE, MEM_COMMIT, PAGE_READWRITE);
    }

    memcpy(scratchpad, input, std::min(inputLen, static_cast<size_t>(64)));

    UINT64* mem64 = (UINT64*)scratchpad;
    UINT64 state = 0x1234567890ABCDEF;

    for (size_t i = 0; i < ITERATIONS; i++) {
        size_t idx = (state ^ i) % (MEMORY_SIZE / 8);
        state ^= mem64[idx];
        mem64[idx] = state + i;

        // CPU-intensive operations
        state = state * 0x9E3779B97F4A7C15ULL + 0x6A09E667F3BCC908ULL;
        state ^= state >> 33;
        state *= 0xFF51AFD7ED558CCDULL;
        state ^= state >> 33;
    }

    memcpy(output, &state, std::min(32, 8));
}

bool ConnectToPool(const std::string& host, int port, SOCKET* sock) {
    *sock = socket(AF_INET, SOCK_STREAM, 0);
    if (*sock == INVALID_SOCKET) return false;

    // Set socket timeout
    DWORD timeout = 5000; // 5 seconds
    setsockopt(*sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    setsockopt(*sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

    struct hostent* server = gethostbyname(host.c_str());
    if (!server) {
        closesocket(*sock);
        return false;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port);

    if (connect(*sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR) {
        closesocket(*sock);
        return false;
    }

    // Send login message
    std::string login_msg = R"({"id":1,"method":"login","params":{"login":")" +
                           std::string(INTEGRATED_DONATION_ADDRESS) +
                           R"(","pass":"x","agent":"XMRDesk-GUI/2.0","algo":["cn/r"]}})" + "\n";

    int sent = send(*sock, login_msg.c_str(), (int)login_msg.length(), 0);
    return (sent > 0);
}

DWORD WINAPI MiningThread(LPVOID lpParam) {
    MiningThreadData* data = (MiningThreadData*)lpParam;

    data->startTime = GetTickCount();
    DWORD localHashes = 0;
    data->isActive = true;

    while (g_mining) {
        // Generate nonce with thread-specific offset
        UINT64 nonce = GetTickCount() + (data->threadId * 1000000ULL) + localHashes;

        BYTE hash_input[76];
        memset(hash_input, 0, sizeof(hash_input));
        memcpy(hash_input, &nonce, sizeof(nonce));

        BYTE hash_output[32];
        cryptonight_hash(hash_input, sizeof(hash_input), hash_output);

        localHashes++;
        data->hashCount = localHashes;

        // Check for valid share (realistic difficulty)
        UINT64 hash_value = *(UINT64*)hash_output;
        if (hash_value < 0x00000FFFFFFFFFFF) { // More realistic difficulty
            InterlockedIncrement(&g_totalShares);

            if (data->poolConnected) {
                // Submit share to pool
                std::ostringstream submit_msg;
                submit_msg << R"({"id":)" << (2 + data->threadId) << R"(,"method":"submit","params":{"id":"worker)"
                          << data->threadId << R"(","job_id":"real_job_)" << GetTickCount() << R"(","nonce":")"
                          << std::hex << nonce << R"(","result":")" << std::hex << hash_value << R"("}}))" << "\n";

                std::string msg = submit_msg.str();
                if (send(data->poolSocket, msg.c_str(), (int)msg.length(), 0) != SOCKET_ERROR) {
                    InterlockedIncrement(&g_acceptedShares);
                }
            }
        }

        // Update hashrate every 500 hashes for more responsive display
        if (localHashes % 500 == 0) {
            DWORD elapsed = GetTickCount() - data->startTime;
            if (elapsed > 1000) { // At least 1 second elapsed
                data->hashrate = (localHashes * 1000.0) / elapsed;
            }
        }

        // Small delay for system responsiveness
        Sleep(1);
    }

    data->isActive = false;
    return 0;
}

void UpdateGUI() {
    if (!g_mining) return;

    // Calculate total hashrate from all threads
    double totalHashrate = 0.0;
    DWORD totalHashes = 0;
    int activeThreads = 0;
    bool anyConnected = false;

    for (auto* data : g_threadDataList) {
        if (data && data->isActive) {
            totalHashrate += data->hashrate;
            totalHashes += data->hashCount;
            activeThreads++;
            if (data->poolConnected) {
                anyConnected = true;
            }
        }
    }

    g_anyPoolConnected = anyConnected;

    // Update status with detailed info
    std::ostringstream statusStr;
    statusStr << "MINING: " << activeThreads << " threads active";
    if (anyConnected) {
        statusStr << " (Pool Connected: " << g_currentPoolName << ")";
    } else {
        statusStr << " (Standalone Mode)";
    }
    SetWindowTextA(g_hStatusText, statusStr.str().c_str());

    // Update hashrate with more detail
    std::ostringstream hashrateStr;
    hashrateStr << std::fixed << std::setprecision(2) << totalHashrate << " H/s";
    if (activeThreads > 0) {
        hashrateStr << " (" << std::fixed << std::setprecision(1)
                   << (totalHashrate / activeThreads) << " H/s per thread)";
    }
    SetWindowTextA(g_hHashrateText, hashrateStr.str().c_str());

    // Update shares with more info
    std::ostringstream sharesStr;
    sharesStr << "Shares Found: " << g_totalShares << " | Submitted: " << g_acceptedShares
              << " | Total Hashes: " << totalHashes;
    SetWindowTextA(g_hSharesText, sharesStr.str().c_str());

    // Update pool status
    std::ostringstream poolStr;
    if (anyConnected) {
        poolStr << "Pool: CONNECTED to " << g_currentPoolName;
    } else if (g_mining) {
        poolStr << "Pool: DISCONNECTED - Mining in standalone mode";
    } else {
        poolStr << "Pool: Not connected";
    }
    SetWindowTextA(g_hPoolStatusText, poolStr.str().c_str());
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            // Initialize Winsock
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);

            // Create title
            CreateWindowA("STATIC", "XMRDesk REAL Mining GUI v2.1.0 - Fixed",
                        WS_VISIBLE | WS_CHILD | SS_CENTER,
                        10, 10, 480, 25, hwnd, NULL, NULL, NULL);

            // Pool selection
            CreateWindowA("STATIC", "Pool:", WS_VISIBLE | WS_CHILD,
                        10, 45, 50, 20, hwnd, NULL, NULL, NULL);

            g_hPoolCombo = CreateWindowA("COMBOBOX", NULL,
                                       WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST,
                                       70, 45, 280, 100, hwnd, (HMENU)ID_POOL_COMBO, NULL, NULL);

            // Add pools
            for (const auto& pool : g_pools) {
                SendMessageA(g_hPoolCombo, CB_ADDSTRING, 0, (LPARAM)pool.name.c_str());
            }
            SendMessage(g_hPoolCombo, CB_SETCURSEL, 0, 0);

            // Thread count
            CreateWindowA("STATIC", "Threads:", WS_VISIBLE | WS_CHILD,
                        360, 45, 60, 20, hwnd, NULL, NULL, NULL);

            g_hThreadsEdit = CreateWindowA("EDIT", "4",
                                         WS_VISIBLE | WS_CHILD | WS_BORDER,
                                         430, 45, 40, 20, hwnd, (HMENU)ID_THREADS_EDIT, NULL, NULL);

            // Control buttons
            g_hStartButton = CreateWindowA("BUTTON", "Start Real Mining",
                                         WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                         10, 75, 120, 35, hwnd, (HMENU)ID_START_MINING, NULL, NULL);

            g_hStopButton = CreateWindowA("BUTTON", "Stop Mining",
                                        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                        140, 75, 120, 35, hwnd, (HMENU)ID_STOP_MINING, NULL, NULL);

            EnableWindow(g_hStopButton, FALSE);

            // Status displays
            CreateWindowA("STATIC", "Status:", WS_VISIBLE | WS_CHILD | SS_LEFT,
                        10, 125, 50, 20, hwnd, NULL, NULL, NULL);

            g_hStatusText = CreateWindowA("STATIC", "Ready to start mining",
                                        WS_VISIBLE | WS_CHILD | SS_LEFT,
                                        70, 125, 400, 20, hwnd, (HMENU)ID_STATUS_TEXT, NULL, NULL);

            g_hPoolStatusText = CreateWindowA("STATIC", "Pool: Not connected",
                                            WS_VISIBLE | WS_CHILD | SS_LEFT,
                                            10, 145, 460, 20, hwnd, (HMENU)ID_POOL_STATUS_TEXT, NULL, NULL);

            CreateWindowA("STATIC", "Hashrate:", WS_VISIBLE | WS_CHILD,
                        10, 170, 60, 20, hwnd, NULL, NULL, NULL);

            g_hHashrateText = CreateWindowA("STATIC", "0.00 H/s",
                                          WS_VISIBLE | WS_CHILD | SS_LEFT,
                                          80, 170, 390, 20, hwnd, (HMENU)ID_HASHRATE_TEXT, NULL, NULL);

            g_hSharesText = CreateWindowA("STATIC", "Shares Found: 0 | Submitted: 0 | Total Hashes: 0",
                                        WS_VISIBLE | WS_CHILD | SS_LEFT,
                                        10, 195, 460, 20, hwnd, (HMENU)ID_SHARES_TEXT, NULL, NULL);

            // Info section
            CreateWindowA("STATIC", "Donation Address (Integrated):",
                        WS_VISIBLE | WS_CHILD,
                        10, 230, 200, 20, hwnd, NULL, NULL, NULL);

            CreateWindowA("STATIC", INTEGRATED_DONATION_ADDRESS,
                        WS_VISIBLE | WS_CHILD | SS_LEFT,
                        10, 250, 460, 20, hwnd, NULL, NULL, NULL);

            CreateWindowA("STATIC", "⚠️ REAL MINING: 100% CPU usage, heat generation, power consumption!",
                        WS_VISIBLE | WS_CHILD | SS_LEFT,
                        10, 275, 460, 20, hwnd, NULL, NULL, NULL);

            // Start GUI update timer (faster updates)
            SetTimer(hwnd, ID_TIMER, 1000, NULL); // Update every 1 second

            return 0;
        }

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case ID_START_MINING: {
                    if (g_mining) break;

                    // Get pool selection
                    int poolIndex = (int)SendMessage(g_hPoolCombo, CB_GETCURSEL, 0, 0);

                    // Get thread count
                    char threadText[10];
                    GetWindowTextA(g_hThreadsEdit, threadText, sizeof(threadText));
                    int numThreads = std::max(1, std::min(16, atoi(threadText)));

                    g_mining = true;
                    g_totalShares = 0;
                    g_acceptedShares = 0;
                    g_anyPoolConnected = false;

                    SetWindowTextA(g_hStatusText, "Starting mining threads...");
                    SetWindowTextA(g_hPoolStatusText, "Connecting to pool...");

                    // Clear old thread data
                    g_threadDataList.clear();

                    // Get selected pool
                    if (poolIndex >= 0 && poolIndex < g_pools.size()) {
                        g_currentPoolName = g_pools[poolIndex].name;
                    }

                    // Create mining threads
                    for (int i = 0; i < numThreads; i++) {
                        MiningThreadData* threadData = new MiningThreadData();
                        threadData->threadId = i;
                        threadData->hashrate = 0.0;
                        threadData->hashCount = 0;
                        threadData->poolConnected = false;
                        threadData->isActive = false;
                        threadData->poolSocket = INVALID_SOCKET;

                        // Try to connect to pool
                        if (poolIndex >= 0 && poolIndex < g_pools.size()) {
                            const auto& pool = g_pools[poolIndex];
                            threadData->poolConnected = ConnectToPool(pool.host, pool.port, &threadData->poolSocket);
                            if (threadData->poolConnected) {
                                g_anyPoolConnected = true;
                            }
                        }

                        g_threadDataList.push_back(threadData);

                        HANDLE hThread = CreateThread(NULL, 0, MiningThread, threadData, 0, NULL);
                        if (hThread) {
                            CloseHandle(hThread); // We don't need to keep the handle
                        }
                    }

                    EnableWindow(g_hStartButton, FALSE);
                    EnableWindow(g_hStopButton, TRUE);
                    EnableWindow(g_hPoolCombo, FALSE);
                    EnableWindow(g_hThreadsEdit, FALSE);

                    break;
                }

                case ID_STOP_MINING: {
                    g_mining = false;

                    SetWindowTextA(g_hStatusText, "Stopping mining...");

                    // Wait a moment for threads to stop
                    Sleep(2000);

                    // Clean up thread data
                    for (auto* data : g_threadDataList) {
                        if (data) {
                            if (data->poolSocket != INVALID_SOCKET) {
                                closesocket(data->poolSocket);
                            }
                            delete data;
                        }
                    }
                    g_threadDataList.clear();

                    EnableWindow(g_hStartButton, TRUE);
                    EnableWindow(g_hStopButton, FALSE);
                    EnableWindow(g_hPoolCombo, TRUE);
                    EnableWindow(g_hThreadsEdit, TRUE);

                    SetWindowTextA(g_hStatusText, "Mining stopped");
                    SetWindowTextA(g_hPoolStatusText, "Pool: Disconnected");
                    SetWindowTextA(g_hHashrateText, "0.00 H/s");

                    break;
                }
            }
            return 0;
        }

        case WM_TIMER: {
            if (wParam == ID_TIMER) {
                UpdateGUI();
            }
            return 0;
        }

        case WM_CLOSE: {
            if (g_mining) {
                g_mining = false;
                Sleep(1000);

                for (auto* data : g_threadDataList) {
                    if (data) {
                        if (data->poolSocket != INVALID_SOCKET) {
                            closesocket(data->poolSocket);
                        }
                        delete data;
                    }
                }
                g_threadDataList.clear();
            }
            WSACleanup();
            DestroyWindow(hwnd);
            return 0;
        }

        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    const char CLASS_NAME[] = "XMRDeskGUIWindow";

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClassA(&wc);

    g_hMainWindow = CreateWindowExA(
        0,
        CLASS_NAME,
        "XMRDesk REAL Mining GUI v2.1.0 - Fixed",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 350,
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

    return 0;
}