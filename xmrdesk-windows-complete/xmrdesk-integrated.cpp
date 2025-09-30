#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include <string>
#include <vector>
#include <atomic>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <queue>
#include <process.h>
#include <fstream>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "kernel32.lib")

// Modern Color Scheme
namespace Colors {
    const COLORREF DARK_BG = RGB(37, 37, 38);
    const COLORREF CARD_BG = RGB(45, 45, 48);
    const COLORREF PRIMARY = RGB(246, 151, 34);
    const COLORREF TEXT_PRIMARY = RGB(255, 255, 255);
    const COLORREF TEXT_SECONDARY = RGB(204, 204, 204);
    const COLORREF SUCCESS = RGB(52, 199, 89);
    const COLORREF LOG_ERROR = RGB(255, 69, 58);
}

// Control IDs
enum ControlID {
    ID_WALLET_EDIT = 3001,
    ID_POOL_COMBO = 3002,
    ID_THREADS_SPIN = 3003,
    ID_START_BUTTON = 3004,
    ID_STOP_BUTTON = 3005,
    ID_STATUS_LABEL = 3010,
    ID_HASHRATE_LABEL = 3011,
    ID_SHARES_LABEL = 3012,
    ID_LOG_LISTBOX = 3020,
    ID_CLEAR_LOG_BUTTON = 3021,
    ID_UPDATE_TIMER = 3030
};

// Pool Configuration
struct PoolInfo {
    std::string name;
    std::string host;
    int port;
    std::string description;
};

const std::vector<PoolInfo> MINING_POOLS = {
    {"SupportXMR", "pool.supportxmr.com", 3333, "Low fees, reliable"},
    {"MoneroOcean", "gulf.moneroocean.stream", 10001, "Auto-switching algo"},
    {"Nanopool", "xmr-eu1.nanopool.org", 14433, "Large stable pool"},
    {"HashVault", "pool.hashvault.pro", 3333, "Professional mining"},
    {"MineXMR", "pool.minexmr.com", 4444, "Established community"},
    {"F2Pool", "xmr.f2pool.com", 13531, "Global mining pool"}
};

// Mining statistics
struct MiningStats {
    std::atomic<bool> is_mining{false};
    std::atomic<double> current_hashrate{0.0};
    std::atomic<uint64_t> shares_found{0};
    std::atomic<uint64_t> shares_accepted{0};
    std::atomic<uint64_t> shares_rejected{0};
    std::chrono::steady_clock::time_point start_time;
};

// Log entry structure
struct LogEntry {
    std::chrono::steady_clock::time_point timestamp;
    std::string message;
    enum Type { INFO, SUCCESS, WARNING, LOG_ERROR } type;
};

// Global variables
MiningStats g_stats;
std::vector<LogEntry> g_log_entries;
CRITICAL_SECTION g_log_mutex;
HWND g_main_window = nullptr;
HWND g_wallet_edit = nullptr;
HWND g_pool_combo = nullptr;
HWND g_threads_spin = nullptr;
HWND g_start_button = nullptr;
HWND g_stop_button = nullptr;
HWND g_log_listbox = nullptr;
HWND g_status_label = nullptr;
HWND g_hashrate_label = nullptr;
HWND g_shares_label = nullptr;

// Fonts
HFONT g_font_normal = nullptr;
HFONT g_font_bold = nullptr;

// External symbols from embedded binary
extern "C" {
    extern char _binary_xmrig_6_24_0_xmrig_exe_start[];
    extern char _binary_xmrig_6_24_0_xmrig_exe_end[];
    extern size_t _binary_xmrig_6_24_0_xmrig_exe_size;
}

// Resource extraction function
bool extract_xmrig() {
    size_t size = static_cast<size_t>(_binary_xmrig_6_24_0_xmrig_exe_end - _binary_xmrig_6_24_0_xmrig_exe_start);

    std::ofstream file("xmrig.exe", std::ios::binary);
    if (!file.is_open()) return false;

    file.write(_binary_xmrig_6_24_0_xmrig_exe_start, size);
    file.close();

    return true;
}

// XMRig process management
class XMRigManager {
private:
    HANDLE process_handle = nullptr;
    HANDLE stdout_read = nullptr;
    HANDLE stdout_write = nullptr;
    HANDLE output_thread = nullptr;
    std::atomic<bool> should_stop{false};

public:
    bool start_mining(const std::string& wallet, const std::string& pool_host, int pool_port, int threads) {
        if (process_handle) {
            stop_mining();
        }

        // Extract XMRig if not present
        if (GetFileAttributes("xmrig.exe") == INVALID_FILE_ATTRIBUTES) {
            add_log("Extracting XMRig from embedded resource...", LogEntry::INFO);
            if (!extract_xmrig()) {
                add_log("Failed to extract XMRig executable", LogEntry::LOG_ERROR);
                return false;
            }
            add_log("XMRig extracted successfully", LogEntry::SUCCESS);
        }

        // Create pipe for stdout
        SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0)) {
            add_log("Failed to create pipe", LogEntry::LOG_ERROR);
            return false;
        }

        // Prepare XMRig command line with your donation wallet
        std::string donation_wallet = "48ckezCUYfnj3vDtQRtH1X4ExpvowjRBJj7U36P13KbDPeZ6M3Pjuev3xdCkXPuAuQNomuSDXTBZFTkQRfP33t3gMTjJCpL";

        std::ostringstream cmd;
        cmd << "xmrig.exe"
            << " --url=" << pool_host << ":" << pool_port
            << " --user=" << wallet
            << " --pass=x"
            << " --threads=" << threads
            << " --algo=rx/0"
            << " --donate-level=1"
            << " --print-time=5"
            << " --no-color";

        std::string command = cmd.str();

        // Setup process info
        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.hStdOutput = stdout_write;
        si.hStdError = stdout_write;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi = {};

        // Start process
        if (!CreateProcessA(nullptr, const_cast<char*>(command.c_str()), nullptr, nullptr,
                           TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            add_log("Failed to start XMRig process", LogEntry::LOG_ERROR);
            CloseHandle(stdout_read);
            CloseHandle(stdout_write);
            return false;
        }

        process_handle = pi.hProcess;
        CloseHandle(pi.hThread);
        CloseHandle(stdout_write);
        stdout_write = nullptr;

        // Start output reading thread
        should_stop = false;
        output_thread = CreateThread(nullptr, 0, &XMRigManager::read_output_static, this, 0, nullptr);

        g_stats.is_mining = true;
        g_stats.start_time = std::chrono::steady_clock::now();

        add_log("Mining started with integrated XMRig", LogEntry::SUCCESS);
        add_log("Donation address: " + donation_wallet, LogEntry::INFO);
        return true;
    }

    void stop_mining() {
        should_stop = true;

        if (process_handle) {
            TerminateProcess(process_handle, 0);
            WaitForSingleObject(process_handle, 5000);
            CloseHandle(process_handle);
            process_handle = nullptr;
        }

        if (stdout_read) {
            CloseHandle(stdout_read);
            stdout_read = nullptr;
        }

        if (output_thread) {
            WaitForSingleObject(output_thread, 5000);
            CloseHandle(output_thread);
            output_thread = nullptr;
        }

        g_stats.is_mining = false;
        add_log("Mining stopped", LogEntry::INFO);
    }

    bool is_mining() const {
        return process_handle != nullptr && g_stats.is_mining;
    }

private:
    static DWORD WINAPI read_output_static(LPVOID param) {
        return static_cast<XMRigManager*>(param)->read_output();
    }

    DWORD read_output() {
        char buffer[1024];
        DWORD bytes_read;

        while (!should_stop && stdout_read) {
            if (ReadFile(stdout_read, buffer, sizeof(buffer) - 1, &bytes_read, nullptr) && bytes_read > 0) {
                buffer[bytes_read] = '\0';
                parse_xmrig_output(std::string(buffer));
            } else {
                Sleep(100);
            }
        }
        return 0;
    }

    void parse_xmrig_output(const std::string& output) {
        std::istringstream iss(output);
        std::string line;

        while (std::getline(iss, line)) {
            if (line.empty()) continue;

            // Parse hashrate
            if (line.find("speed") != std::string::npos || line.find("H/s") != std::string::npos) {
                size_t pos = line.find("H/s");
                if (pos != std::string::npos) {
                    // Extract hashrate value
                    std::string hashrate_str;
                    for (int i = pos - 1; i >= 0; i--) {
                        char c = line[i];
                        if (std::isdigit(c) || c == '.') {
                            hashrate_str = c + hashrate_str;
                        } else if (!hashrate_str.empty()) {
                            break;
                        }
                    }

                    if (!hashrate_str.empty()) {
                        double hashrate = std::stod(hashrate_str);
                        g_stats.current_hashrate = hashrate;
                    }
                }
            }

            // Parse shares
            if (line.find("accepted") != std::string::npos) {
                g_stats.shares_accepted++;
                g_stats.shares_found++;
                add_log("Share accepted: " + line, LogEntry::SUCCESS);
            } else if (line.find("rejected") != std::string::npos) {
                g_stats.shares_rejected++;
                add_log("Share rejected: " + line, LogEntry::LOG_ERROR);
            }

            // Log important messages
            if (line.find("new job") != std::string::npos ||
                line.find("use pool") != std::string::npos ||
                line.find("READY") != std::string::npos ||
                line.find("connected to") != std::string::npos) {
                add_log(line, LogEntry::INFO);
            }
        }
    }

    static void add_log(const std::string& message, LogEntry::Type type) {
        EnterCriticalSection(&g_log_mutex);
        LogEntry entry;
        entry.timestamp = std::chrono::steady_clock::now();
        entry.message = message;
        entry.type = type;
        g_log_entries.push_back(entry);

        // Keep only last 100 entries
        if (g_log_entries.size() > 100) {
            g_log_entries.erase(g_log_entries.begin());
        }

        // Update log listbox on main thread
        if (g_log_listbox) {
            PostMessage(g_main_window, WM_USER + 1, 0, 0);
        }

        LeaveCriticalSection(&g_log_mutex);
    }
};

XMRigManager g_xmrig_manager;

// Utility functions
std::string format_time(const std::chrono::steady_clock::time_point& start) {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start);

    int hours = duration.count() / 3600;
    int minutes = (duration.count() % 3600) / 60;
    int seconds = duration.count() % 60;

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << hours << ":"
        << std::setfill('0') << std::setw(2) << minutes << ":"
        << std::setfill('0') << std::setw(2) << seconds;
    return oss.str();
}

bool is_valid_monero_address(const std::string& address) {
    if (address.length() != 95) return false;
    if (address[0] != '4') return false;

    const std::string valid_chars = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
    for (char c : address) {
        if (valid_chars.find(c) == std::string::npos) {
            return false;
        }
    }
    return true;
}

// Window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            g_main_window = hwnd;

            // Create fonts
            g_font_normal = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, VARIABLE_PITCH, "Segoe UI");

            g_font_bold = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, VARIABLE_PITCH, "Segoe UI");

            // Title
            CreateWindowA("STATIC", "XMRDesk Integrated v5.1.0 - Single File Solution",
                         WS_VISIBLE | WS_CHILD | SS_LEFT,
                         20, 20, 500, 25, hwnd, nullptr, nullptr, nullptr);

            // Wallet address section
            CreateWindowA("STATIC", "Wallet Address:",
                         WS_VISIBLE | WS_CHILD | SS_LEFT,
                         20, 60, 150, 20, hwnd, nullptr, nullptr, nullptr);

            g_wallet_edit = CreateWindowA("EDIT", "",
                                        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                                        20, 85, 560, 25, hwnd, (HMENU)ID_WALLET_EDIT, nullptr, nullptr);

            // Pool selection
            CreateWindowA("STATIC", "Mining Pool:",
                         WS_VISIBLE | WS_CHILD | SS_LEFT,
                         20, 125, 100, 20, hwnd, nullptr, nullptr, nullptr);

            g_pool_combo = CreateWindowA("COMBOBOX", "",
                                       WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST,
                                       20, 150, 300, 200, hwnd, (HMENU)ID_POOL_COMBO, nullptr, nullptr);

            // Add pools to combo
            for (const auto& pool : MINING_POOLS) {
                std::string pool_text = pool.name + " (" + pool.description + ")";
                SendMessage(g_pool_combo, CB_ADDSTRING, 0, (LPARAM)pool_text.c_str());
            }
            SendMessage(g_pool_combo, CB_SETCURSEL, 0, 0);

            // Threads
            CreateWindowA("STATIC", "Threads:",
                         WS_VISIBLE | WS_CHILD | SS_LEFT,
                         340, 125, 60, 20, hwnd, nullptr, nullptr, nullptr);

            g_threads_spin = CreateWindowA("EDIT", "4",
                                         WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
                                         340, 150, 60, 25, hwnd, (HMENU)ID_THREADS_SPIN, nullptr, nullptr);

            // Control buttons
            g_start_button = CreateWindowA("BUTTON", "Start Mining",
                                         WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                         20, 190, 120, 35, hwnd, (HMENU)ID_START_BUTTON, nullptr, nullptr);

            g_stop_button = CreateWindowA("BUTTON", "Stop Mining",
                                        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                        150, 190, 120, 35, hwnd, (HMENU)ID_STOP_BUTTON, nullptr, nullptr);

            EnableWindow(g_stop_button, FALSE);

            // Status area
            g_status_label = CreateWindowA("STATIC", "Status: STOPPED",
                                         WS_VISIBLE | WS_CHILD | SS_LEFT,
                                         20, 240, 200, 25, hwnd, (HMENU)ID_STATUS_LABEL, nullptr, nullptr);

            g_hashrate_label = CreateWindowA("STATIC", "Hashrate: 0.00 H/s (0 threads)",
                                           WS_VISIBLE | WS_CHILD | SS_LEFT,
                                           20, 270, 400, 25, hwnd, (HMENU)ID_HASHRATE_LABEL, nullptr, nullptr);

            g_shares_label = CreateWindowA("STATIC", "Shares: Found=0 | Accepted=0 | Rejected=0",
                                         WS_VISIBLE | WS_CHILD | SS_LEFT,
                                         20, 300, 500, 25, hwnd, (HMENU)ID_SHARES_LABEL, nullptr, nullptr);

            // Log area
            CreateWindowA("STATIC", "Mining Log:",
                         WS_VISIBLE | WS_CHILD | SS_LEFT,
                         20, 340, 100, 20, hwnd, nullptr, nullptr, nullptr);

            g_log_listbox = CreateWindowA("LISTBOX", "",
                                        WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
                                        20, 365, 560, 150, hwnd, (HMENU)ID_LOG_LISTBOX, nullptr, nullptr);

            CreateWindowA("BUTTON", "Clear Log",
                         WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                         490, 340, 90, 20, hwnd, (HMENU)ID_CLEAR_LOG_BUTTON, nullptr, nullptr);

            // Apply fonts
            SendMessage(g_wallet_edit, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
            SendMessage(g_pool_combo, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
            SendMessage(g_threads_spin, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
            SendMessage(g_start_button, WM_SETFONT, (WPARAM)g_font_bold, TRUE);
            SendMessage(g_stop_button, WM_SETFONT, (WPARAM)g_font_bold, TRUE);
            SendMessage(g_status_label, WM_SETFONT, (WPARAM)g_font_bold, TRUE);
            SendMessage(g_hashrate_label, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
            SendMessage(g_shares_label, WM_SETFONT, (WPARAM)g_font_normal, TRUE);

            // Start timer
            SetTimer(hwnd, ID_UPDATE_TIMER, 2000, nullptr);

            break;
        }

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case ID_START_BUTTON: {
                    char wallet[256], threads_str[16];
                    GetWindowTextA(g_wallet_edit, wallet, sizeof(wallet));
                    GetWindowTextA(g_threads_spin, threads_str, sizeof(threads_str));

                    if (!is_valid_monero_address(wallet)) {
                        MessageBoxA(hwnd, "Please enter a valid Monero wallet address (95 characters, starting with '4').",
                                  "Invalid Wallet", MB_OK | MB_ICONERROR);
                        break;
                    }

                    int threads = atoi(threads_str);
                    if (threads < 1 || threads > 64) {
                        MessageBoxA(hwnd, "Thread count must be between 1 and 64.", "Invalid Thread Count", MB_OK | MB_ICONERROR);
                        break;
                    }

                    int pool_idx = SendMessage(g_pool_combo, CB_GETCURSEL, 0, 0);
                    if (pool_idx < 0 || pool_idx >= MINING_POOLS.size()) pool_idx = 0;

                    const auto& pool = MINING_POOLS[pool_idx];

                    if (g_xmrig_manager.start_mining(wallet, pool.host, pool.port, threads)) {
                        EnableWindow(g_start_button, FALSE);
                        EnableWindow(g_stop_button, TRUE);
                        EnableWindow(g_wallet_edit, FALSE);
                        EnableWindow(g_pool_combo, FALSE);
                        EnableWindow(g_threads_spin, FALSE);
                    }
                    break;
                }

                case ID_STOP_BUTTON: {
                    g_xmrig_manager.stop_mining();
                    EnableWindow(g_start_button, TRUE);
                    EnableWindow(g_stop_button, FALSE);
                    EnableWindow(g_wallet_edit, TRUE);
                    EnableWindow(g_pool_combo, TRUE);
                    EnableWindow(g_threads_spin, TRUE);
                    break;
                }

                case ID_CLEAR_LOG_BUTTON: {
                    EnterCriticalSection(&g_log_mutex);
                    g_log_entries.clear();
                    SendMessage(g_log_listbox, LB_RESETCONTENT, 0, 0);
                    LeaveCriticalSection(&g_log_mutex);
                    break;
                }
            }
            break;
        }

        case WM_TIMER: {
            if (wParam == ID_UPDATE_TIMER) {
                // Update status
                std::string status = g_stats.is_mining ? "MINING" : "STOPPED";
                SetWindowTextA(g_status_label, ("Status: " + status).c_str());

                // Update hashrate
                double hashrate = g_stats.current_hashrate;
                std::ostringstream hr_stream;
                hr_stream << "Hashrate: " << std::fixed << std::setprecision(2) << hashrate << " H/s";
                if (g_stats.is_mining) {
                    std::string uptime = " | Uptime: " + format_time(g_stats.start_time);
                    hr_stream << uptime;
                }
                SetWindowTextA(g_hashrate_label, hr_stream.str().c_str());

                // Update shares
                std::ostringstream share_stream;
                share_stream << "Shares: Found=" << g_stats.shares_found.load()
                             << " | Accepted=" << g_stats.shares_accepted.load()
                             << " | Rejected=" << g_stats.shares_rejected.load();
                SetWindowTextA(g_shares_label, share_stream.str().c_str());
            }
            break;
        }

        case WM_USER + 1: {
            // Update log listbox
            EnterCriticalSection(&g_log_mutex);

            // Clear and repopulate
            SendMessage(g_log_listbox, LB_RESETCONTENT, 0, 0);

            for (const auto& entry : g_log_entries) {
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                    entry.timestamp - g_stats.start_time);

                std::ostringstream oss;
                oss << "[" << std::setfill('0') << std::setw(5) << duration.count() << "s] " << entry.message;

                SendMessage(g_log_listbox, LB_ADDSTRING, 0, (LPARAM)oss.str().c_str());
            }

            // Scroll to bottom
            SendMessage(g_log_listbox, LB_SETTOPINDEX,
                       SendMessage(g_log_listbox, LB_GETCOUNT, 0, 0) - 1, 0);

            LeaveCriticalSection(&g_log_mutex);
            break;
        }

        case WM_CLOSE:
            g_xmrig_manager.stop_mining();

            // Clean up extracted XMRig
            DeleteFile("xmrig.exe");

            DestroyWindow(hwnd);
            break;

        case WM_DESTROY:
            KillTimer(hwnd, ID_UPDATE_TIMER);

            if (g_font_normal) DeleteObject(g_font_normal);
            if (g_font_bold) DeleteObject(g_font_bold);

            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
    // Initialize critical section
    InitializeCriticalSection(&g_log_mutex);

    // Initialize common controls
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    // Register window class
    const char CLASS_NAME[] = "XMRDeskIntegrated";

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);

    RegisterClassA(&wc);

    // Create main window
    HWND hwnd = CreateWindowExA(
        0,
        CLASS_NAME,
        "XMRDesk Integrated v5.1.0 - Single File Solution with Embedded XMRig",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 640, 580,
        nullptr, nullptr, hInstance, nullptr
    );

    if (hwnd == nullptr) {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Message loop
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    DeleteCriticalSection(&g_log_mutex);

    return 0;
}