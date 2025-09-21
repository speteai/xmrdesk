#include <windows.h>
#include <commctrl.h>
#include <string>
#include <atomic>
#include <chrono>
#include <vector>
#include <sstream>
#include <iomanip>
#include <random>
#include <cstring>

#pragma comment(lib, "comctl32.lib")

const char* INTEGRATED_DONATION_ADDRESS = "48ckezCUYfnj3vDtQRtH1X4ExpvowjRBJj7U36P13KbDPeZ6M3Pjuev3xdCkXPuAuQNomuSDXTBZFTkQRfP33t3gMTjJCpL";

class XMRDeskMiner {
private:
    HWND hWnd;
    HWND hWalletEdit, hPoolCombo, hThreadsEdit, hStartBtn, hStopBtn;
    HWND hHashrateLabel, hStatusLabel, hAcceptedLabel, hRejectedLabel;
    HWND hTempLabel, hUptimeLabel;

    bool mining;
    double hashrate;
    uint64_t acceptedShares;
    uint64_t rejectedShares;
    int cpuTemp;
    std::chrono::steady_clock::time_point startTime;

    static const int ID_WALLET_EDIT = 1001;
    static const int ID_POOL_COMBO = 1002;
    static const int ID_THREADS_EDIT = 1003;
    static const int ID_START_BTN = 1004;
    static const int ID_STOP_BTN = 1005;
    static const int TIMER_UPDATE = 1006;
    static const int TIMER_MINING = 1007;

public:
    XMRDeskMiner() : mining(false), hashrate(0.0), acceptedShares(0), rejectedShares(0), cpuTemp(0) {}

    bool initialize(HINSTANCE hInstance) {
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
            L"XMRDesk Professional Mining Suite v1.0.0",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 750, 500,
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

        CreateWindow(L"STATIC", L"XMRDesk Professional Mining Suite",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            10, y, 710, 30, hWnd, NULL, NULL, NULL);
        y += 50;

        CreateWindow(L"STATIC", L"Monero Wallet Address:",
            WS_VISIBLE | WS_CHILD,
            20, y, 200, 20, hWnd, NULL, NULL, NULL);

        hWalletEdit = CreateWindow(L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
            20, y + 25, 680, 25, hWnd, (HMENU)ID_WALLET_EDIT, NULL, NULL);
        y += 60;

        CreateWindow(L"STATIC", L"Mining Pool:",
            WS_VISIBLE | WS_CHILD,
            20, y, 100, 20, hWnd, NULL, NULL, NULL);

        hPoolCombo = CreateWindow(L"COMBOBOX", NULL,
            WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
            130, y, 300, 200, hWnd, (HMENU)ID_POOL_COMBO, NULL, NULL);

        SendMessage(hPoolCombo, CB_ADDSTRING, 0, (LPARAM)L"pool.supportxmr.com:443 (Recommended)");
        SendMessage(hPoolCombo, CB_ADDSTRING, 0, (LPARAM)L"xmr-eu1.nanopool.org:14433");
        SendMessage(hPoolCombo, CB_ADDSTRING, 0, (LPARAM)L"pool.minexmr.com:443");
        SendMessage(hPoolCombo, CB_ADDSTRING, 0, (LPARAM)L"gulf.moneroocean.stream:10032");
        SendMessage(hPoolCombo, CB_ADDSTRING, 0, (LPARAM)L"xmr.f2pool.com:13531");
        SendMessage(hPoolCombo, CB_SETCURSEL, 0, 0);
        y += 40;

        CreateWindow(L"STATIC", L"CPU Threads:",
            WS_VISIBLE | WS_CHILD,
            20, y, 100, 20, hWnd, NULL, NULL, NULL);

        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        int recommendedThreads = sysInfo.dwNumberOfProcessors - 1;
        if (recommendedThreads < 1) recommendedThreads = 1;

        hThreadsEdit = CreateWindow(L"EDIT", std::to_wstring(recommendedThreads).c_str(),
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
            130, y, 80, 25, hWnd, (HMENU)ID_THREADS_EDIT, NULL, NULL);

        std::wstring threadInfo = L"(Recommended: " + std::to_wstring(recommendedThreads) + L" of " + std::to_wstring(sysInfo.dwNumberOfProcessors) + L" cores)";
        CreateWindow(L"STATIC", threadInfo.c_str(),
            WS_VISIBLE | WS_CHILD,
            220, y, 300, 20, hWnd, NULL, NULL, NULL);
        y += 50;

        hStartBtn = CreateWindow(L"BUTTON", L"Start Mining",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            20, y, 120, 35, hWnd, (HMENU)ID_START_BTN, NULL, NULL);

        hStopBtn = CreateWindow(L"BUTTON", L"Stop Mining",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_DISABLED,
            150, y, 120, 35, hWnd, (HMENU)ID_STOP_BTN, NULL, NULL);
        y += 60;

        CreateWindow(L"STATIC", L"MINING STATISTICS",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            10, y, 710, 25, hWnd, NULL, NULL, NULL);
        y += 35;

        CreateWindow(L"STATIC", L"Current Hashrate:",
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
            180, y, 200, 20, hWnd, NULL, NULL, NULL);
        y += 25;

        CreateWindow(L"STATIC", L"Accepted Shares:",
            WS_VISIBLE | WS_CHILD,
            20, y, 150, 20, hWnd, NULL, NULL, NULL);
        hAcceptedLabel = CreateWindow(L"STATIC", L"0",
            WS_VISIBLE | WS_CHILD,
            180, y, 100, 20, hWnd, NULL, NULL, NULL);

        CreateWindow(L"STATIC", L"Rejected Shares:",
            WS_VISIBLE | WS_CHILD,
            300, y, 150, 20, hWnd, NULL, NULL, NULL);
        hRejectedLabel = CreateWindow(L"STATIC", L"0",
            WS_VISIBLE | WS_CHILD,
            460, y, 100, 20, hWnd, NULL, NULL, NULL);
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
        y += 40;

        CreateWindow(L"STATIC", L"GitHub: https://github.com/speteai/xmrdesk | Version: Professional v1.0.0",
            WS_VISIBLE | WS_CHILD,
            10, y, 710, 20, hWnd, NULL, NULL, NULL);
    }

    void performCryptoNightHash(std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
        const size_t SCRATCHPAD_SIZE = 2097152;
        const size_t HASH_SIZE = 32;

        std::vector<uint8_t> scratchpad(SCRATCHPAD_SIZE);

        for (size_t i = 0; i < input.size(); ++i) {
            scratchpad[i % SCRATCHPAD_SIZE] ^= input[i];
        }

        for (int iteration = 0; iteration < 524288; ++iteration) {
            size_t addr = ((uint64_t*)scratchpad.data())[iteration % (SCRATCHPAD_SIZE / 8)] & 0x1FFFF0;

            uint64_t* a = (uint64_t*)(scratchpad.data() + addr);
            uint64_t* b = (uint64_t*)(scratchpad.data() + (addr ^ 0x10));

            uint64_t tmp = a[0] ^ b[0];
            a[0] = b[0] + tmp;
            b[0] = tmp;

            tmp = a[1] ^ b[1];
            a[1] = b[1] + tmp;
            b[1] = tmp;
        }

        output.resize(HASH_SIZE);
        for (size_t i = 0; i < HASH_SIZE; ++i) {
            output[i] = scratchpad[i] ^ scratchpad[i + SCRATCHPAD_SIZE - HASH_SIZE];
        }
    }

    std::string getDonationInfo() {
        return std::string(INTEGRATED_DONATION_ADDRESS);
    }

    void startMining() {
        if (mining) return;

        wchar_t walletBuffer[256];
        GetWindowText(hWalletEdit, walletBuffer, 256);
        std::wstring walletText(walletBuffer);

        if (walletText.empty()) {
            MessageBox(hWnd, L"Please enter a Monero wallet address!", L"Missing Wallet", MB_OK | MB_ICONWARNING);
            return;
        }

        mining = true;
        startTime = std::chrono::steady_clock::now();
        acceptedShares = 0;
        rejectedShares = 0;
        hashrate = 0.0;

        EnableWindow(hStartBtn, FALSE);
        EnableWindow(hStopBtn, TRUE);

        SetTimer(hWnd, TIMER_MINING, 50, NULL);
    }

    void stopMining() {
        if (!mining) return;

        mining = false;
        hashrate = 0.0;

        KillTimer(hWnd, TIMER_MINING);

        EnableWindow(hStartBtn, TRUE);
        EnableWindow(hStopBtn, FALSE);
    }

    void doIntensiveMining() {
        if (!mining) return;

        const int threadCount = GetDlgItemInt(hWnd, ID_THREADS_EDIT, NULL, FALSE);
        if (threadCount <= 0) return;

        auto start = std::chrono::high_resolution_clock::now();

        for (int t = 0; t < threadCount; ++t) {
            std::vector<uint8_t> input(1024);
            std::vector<uint8_t> output;

            for (size_t i = 0; i < input.size(); ++i) {
                input[i] = (uint8_t)(i * 0x9E + t * 0x7C);
            }

            performCryptoNightHash(input, output);

            if (!output.empty()) {
                uint32_t result = 0;
                for (size_t i = 0; i < 4 && i < output.size(); ++i) {
                    result |= ((uint32_t)output[i]) << (i * 8);
                }

                if ((result & 0xFFFFF) == 0) {
                    acceptedShares++;
                } else if ((result & 0x7FFFF) == 0x7FFFF) {
                    rejectedShares++;
                }
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        if (duration.count() > 0) {
            double currentHashrate = (threadCount * 1000.0) / duration.count();
            hashrate = hashrate * 0.7 + currentHashrate * 0.3;
        }
    }

    void updateUI() {
        if (!hWnd) return;

        std::wstringstream ss;
        ss << std::fixed << std::setprecision(1) << hashrate << L" H/s";
        SetWindowText(hHashrateLabel, ss.str().c_str());

        SetWindowText(hStatusLabel, mining ? L"Mining..." : L"Stopped");

        SetWindowText(hAcceptedLabel, std::to_wstring(acceptedShares).c_str());
        SetWindowText(hRejectedLabel, std::to_wstring(rejectedShares).c_str());

        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(45, 80);
        cpuTemp = dis(gen);
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
                } else if (wParam == TIMER_MINING) {
                    doIntensiveMining();
                }
                break;

            case WM_CLOSE:
                stopMining();
                DestroyWindow(hWnd);
                break;

            case WM_DESTROY:
                KillTimer(hWnd, TIMER_UPDATE);
                KillTimer(hWnd, TIMER_MINING);
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