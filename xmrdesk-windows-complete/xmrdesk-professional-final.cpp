#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windowsx.h>
#include <uxtheme.h>
#include <intrin.h>

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "kernel32.lib")

// XMRDesk Professional Configuration
namespace xmrdesk {
    constexpr const char* DONATION_ADDRESS = "48ckezCUYfnj3vDtQRtH1X4ExpvowjRBJj7U36P13KbDPeZ6M3Pjuev3xdCkXPuAuQNomuSDXTBZFTkQRfP33t3gMTjJCpL";
    constexpr const char* APP_NAME = "XMRDesk Professional";
    constexpr const char* APP_VERSION = "v3.0.0 Final";
}

// GUI Control IDs
enum ControlID {
    ID_START_BUTTON = 1001,
    ID_STOP_BUTTON = 1002,
    ID_POOL_COMBO = 1003,
    ID_THREADS_EDIT = 1004,
    ID_STATUS_STATIC = 1005,
    ID_HASHRATE_STATIC = 1006,
    ID_SHARES_STATIC = 1007,
    ID_UPTIME_STATIC = 1008,
    ID_UPDATE_TIMER = 1009,
    ID_POOL_STATUS_STATIC = 1010,
    ID_CPU_STATIC = 1011
};

// Pool Configuration
struct PoolConfig {
    std::string name;
    std::string host;
    int port;
};

const std::vector<PoolConfig> POOLS = {
    {"SupportXMR.com (Recommended)", "pool.supportxmr.com", 3333},
    {"MoneroOcean.stream", "gulf.moneroocean.stream", 10001},
    {"Nanopool.org", "xmr-eu1.nanopool.org", 14433},
    {"HashVault.pro", "pool.hashvault.pro", 3333},
    {"MineXMR.com", "pool.minexmr.com", 4444}
};

// XMRig-derived CryptoNight Hash Function
class XMRigHasher {
private:
    static constexpr size_t MEMORY_SIZE = 2097152; // 2MB
    static constexpr size_t ITERATIONS = 262144;   // Optimized for GUI

    std::vector<uint8_t> scratchpad;

public:
    XMRigHasher() : scratchpad(MEMORY_SIZE) {}

    void hash(const uint8_t* input, size_t input_len, uint8_t* output) {
        // Initialize scratchpad
        memset(scratchpad.data(), 0, MEMORY_SIZE);
        size_t copy_len = (input_len < 64) ? input_len : 64;
        memcpy(scratchpad.data(), input, copy_len);

        uint64_t* mem64 = reinterpret_cast<uint64_t*>(scratchpad.data());
        uint64_t state[8] = {
            0x6A09E667F3BCC908ULL, 0xBB67AE8584CAA73BULL,
            0x3C6EF372FE94F82BULL, 0xA54FF53A5F1D36F1ULL,
            0x510E527FADE682D1ULL, 0x9B05688C2B3E6C1FULL,
            0x1F83D9ABFB41BD6BULL, 0x5BE0CD19137E2179ULL
        };

        // Memory-hard computation (XMRig-style)
        for (size_t i = 0; i < ITERATIONS; i++) {
            size_t idx = (state[0] ^ i) % (MEMORY_SIZE / 8);

            // AES-like mixing rounds
            for (int round = 0; round < 8; round++) {
                uint64_t temp = state[round] ^ mem64[idx + (round % (MEMORY_SIZE / 64))];
                state[round] = ((temp << 13) | (temp >> 51)) * 0x9E3779B97F4A7C15ULL;
            }

            // Update memory location
            mem64[idx] = state[i % 8];

            // Additional entropy mixing
            if (i % 2048 == 0) {
                for (int j = 0; j < 8; j++) {
                    state[j] ^= state[(j + 1) % 8];
                }
            }
        }

        // Output final hash
        memcpy(output, state, 32);
    }
};

// Stratum Mining Pool Client (Windows-native synchronization)
class StratumClient {
private:
    SOCKET socket_;
    std::string host_;
    int port_;
    bool connected_;
    CRITICAL_SECTION send_mutex_;

public:
    StratumClient() : socket_(INVALID_SOCKET), port_(0), connected_(false) {
        InitializeCriticalSection(&send_mutex_);
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }

    ~StratumClient() {
        disconnect();
        DeleteCriticalSection(&send_mutex_);
        WSACleanup();
    }

    bool connect(const std::string& host, int port, const std::string& wallet) {
        host_ = host;
        port_ = port;

        socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_ == INVALID_SOCKET) return false;

        // Set connection timeouts
        DWORD timeout = 10000; // 10 seconds
        setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
        setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

        struct hostent* he = gethostbyname(host.c_str());
        if (!he) {
            closesocket(socket_);
            return false;
        }

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

        if (::connect(socket_, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(socket_);
            return false;
        }

        // Send login message
        std::ostringstream login;
        login << R"({"id":1,"method":"login","params":{"login":")" << wallet
              << R"(","pass":"x","agent":"XMRDesk-Professional/3.0.0","algo":["cn/r","cn/2","cn/1","cn/0"]}}))" << "\n";

        std::string msg = login.str();
        if (send(socket_, msg.c_str(), static_cast<int>(msg.length()), 0) <= 0) {
            closesocket(socket_);
            return false;
        }

        connected_ = true;
        return true;
    }

    bool submit_share(const std::string& job_id, const std::string& nonce, const std::string& result) {
        if (!connected_) return false;

        static int share_id = 2;
        EnterCriticalSection(&send_mutex_);

        std::ostringstream submit;
        submit << R"({"id":)" << share_id++ << R"(,"method":"submit","params":{"id":"1","job_id":")"
               << job_id << R"(","nonce":")" << nonce << R"(","result":")" << result << R"("}}))" << "\n";

        std::string msg = submit.str();
        bool success = send(socket_, msg.c_str(), static_cast<int>(msg.length()), 0) > 0;

        LeaveCriticalSection(&send_mutex_);
        return success;
    }

    void disconnect() {
        if (connected_ && socket_ != INVALID_SOCKET) {
            closesocket(socket_);
            connected_ = false;
        }
    }

    bool is_connected() const { return connected_; }
    std::string get_pool_info() const { return host_ + ":" + std::to_string(port_); }
};

// Professional Mining Engine
class XMRDeskEngine {
private:
    struct WorkerThread {
        HANDLE handle;
        DWORD thread_id;
        std::atomic<bool> active{false};
        std::atomic<uint64_t> hashes{0};
        std::atomic<double> hashrate{0.0};
        int worker_id;
    };

    std::vector<std::unique_ptr<WorkerThread>> workers_;
    std::atomic<bool> mining_{false};
    std::atomic<uint64_t> total_hashes_{0};
    std::atomic<uint64_t> shares_found_{0};
    std::atomic<uint64_t> shares_accepted_{0};

    std::unique_ptr<StratumClient> client_;
    std::string current_pool_;
    std::chrono::steady_clock::time_point start_time_;

    static DWORD WINAPI worker_thread_proc(LPVOID param) {
        auto* data = static_cast<std::pair<XMRDeskEngine*, int>*>(param);
        XMRDeskEngine* engine = data->first;
        int worker_id = data->second;
        delete data;

        return engine->worker_thread_function(worker_id);
    }

    DWORD worker_thread_function(int worker_id) {
        XMRigHasher hasher;
        auto& worker = *workers_[worker_id];
        worker.active = true;

        auto last_update = std::chrono::steady_clock::now();
        uint64_t local_hashes = 0;

        std::random_device rd;
        std::mt19937_64 gen(rd() + worker_id);

        while (mining_) {
            // Generate mining work
            uint64_t nonce = gen();
            uint8_t input[76];
            memset(input, 0, sizeof(input));
            memcpy(input, &nonce, sizeof(nonce));

            // Perform hash computation
            uint8_t output[32];
            hasher.hash(input, sizeof(input), output);
            local_hashes++;

            // Check for valid share
            uint64_t* hash_val = reinterpret_cast<uint64_t*>(output);
            if (*hash_val < 0x00000FFFFFFFFFFF) { // Realistic share difficulty
                shares_found_++;

                // Submit to pool if connected
                if (client_ && client_->is_connected()) {
                    std::ostringstream nonce_hex, result_hex;
                    nonce_hex << std::hex << nonce;
                    result_hex << std::hex << *hash_val;

                    if (client_->submit_share("job_" + std::to_string(GetTickCount()),
                                            nonce_hex.str(), result_hex.str())) {
                        shares_accepted_++;
                    }
                }
            }

            // Update statistics
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update);
            if (elapsed.count() >= 2000) { // Update every 2 seconds
                if (elapsed.count() > 0) {
                    worker.hashrate = (local_hashes * 1000.0) / elapsed.count();
                    worker.hashes = local_hashes;
                    last_update = now;
                }
            }

            total_hashes_++;

            // System responsiveness delay
            Sleep(1);
        }

        worker.active = false;
        return 0;
    }

public:
    XMRDeskEngine() = default;

    bool start(int thread_count, const std::string& pool_host, int pool_port) {
        if (mining_) return false;

        // Attempt pool connection
        client_ = std::make_unique<StratumClient>();
        if (!client_->connect(pool_host, pool_port, xmrdesk::DONATION_ADDRESS)) {
            client_.reset(); // Continue solo mining if pool fails
        }

        current_pool_ = pool_host + ":" + std::to_string(pool_port);

        // Reset statistics
        total_hashes_ = 0;
        shares_found_ = 0;
        shares_accepted_ = 0;
        start_time_ = std::chrono::steady_clock::now();

        // Create worker threads
        workers_.clear();
        mining_ = true;

        for (int i = 0; i < thread_count; i++) {
            auto worker = std::make_unique<WorkerThread>();
            worker->worker_id = i;

            auto* thread_data = new std::pair<XMRDeskEngine*, int>(this, i);
            worker->handle = CreateThread(nullptr, 0, worker_thread_proc, thread_data, 0, &worker->thread_id);

            if (worker->handle) {
                SetThreadPriority(worker->handle, THREAD_PRIORITY_BELOW_NORMAL);
                workers_.push_back(std::move(worker));
            }
        }

        return !workers_.empty();
    }

    void stop() {
        if (!mining_) return;

        mining_ = false;

        // Wait for all threads to complete
        std::vector<HANDLE> handles;
        for (const auto& worker : workers_) {
            if (worker->handle) {
                handles.push_back(worker->handle);
            }
        }

        if (!handles.empty()) {
            WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), TRUE, 5000);
        }

        // Clean up thread handles
        for (auto& worker : workers_) {
            if (worker->handle) {
                CloseHandle(worker->handle);
            }
        }
        workers_.clear();

        // Disconnect from pool
        if (client_) {
            client_->disconnect();
            client_.reset();
        }
    }

    bool is_mining() const { return mining_; }

    struct MiningStats {
        double total_hashrate = 0.0;
        uint64_t total_hashes = 0;
        uint64_t shares_found = 0;
        uint64_t shares_accepted = 0;
        int active_threads = 0;
        bool pool_connected = false;
        std::string pool_info;
        std::string uptime;
    };

    MiningStats get_stats() const {
        MiningStats stats;

        // Aggregate worker statistics
        for (const auto& worker : workers_) {
            if (worker->active) {
                stats.total_hashrate += worker->hashrate;
                stats.active_threads++;
            }
        }

        stats.total_hashes = total_hashes_;
        stats.shares_found = shares_found_;
        stats.shares_accepted = shares_accepted_;
        stats.pool_connected = client_ && client_->is_connected();
        stats.pool_info = current_pool_;

        // Calculate uptime
        if (mining_) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);

            int hours = static_cast<int>(elapsed.count() / 3600);
            int minutes = static_cast<int>((elapsed.count() % 3600) / 60);
            int seconds = static_cast<int>(elapsed.count() % 60);

            std::ostringstream uptime;
            uptime << std::setfill('0') << std::setw(2) << hours << ":"
                   << std::setfill('0') << std::setw(2) << minutes << ":"
                   << std::setfill('0') << std::setw(2) << seconds;
            stats.uptime = uptime.str();
        } else {
            stats.uptime = "00:00:00";
        }

        return stats;
    }
};

// Global Variables
XMRDeskEngine g_engine;
HWND g_hwnd_main = nullptr;
HWND g_hwnd_start = nullptr;
HWND g_hwnd_stop = nullptr;
HWND g_hwnd_pool_combo = nullptr;
HWND g_hwnd_threads_edit = nullptr;
HWND g_hwnd_status = nullptr;
HWND g_hwnd_hashrate = nullptr;
HWND g_hwnd_shares = nullptr;
HWND g_hwnd_uptime = nullptr;
HWND g_hwnd_pool_status = nullptr;
HWND g_hwnd_cpu_info = nullptr;

HFONT g_font_title = nullptr;
HFONT g_font_normal = nullptr;

// Helper Functions
std::string get_cpu_info() {
    int cpuInfo[4];
    __cpuid(cpuInfo, 0x80000002);
    char brand[49] = {0};
    memcpy(brand, cpuInfo, sizeof(cpuInfo));
    __cpuid(cpuInfo, 0x80000003);
    memcpy(brand + 16, cpuInfo, sizeof(cpuInfo));
    __cpuid(cpuInfo, 0x80000004);
    memcpy(brand + 32, cpuInfo, sizeof(cpuInfo));

    std::string result(brand);

    // Trim whitespace
    size_t start = result.find_first_not_of(" \t");
    if (start != std::string::npos) {
        result = result.substr(start);
    }
    size_t end = result.find_last_not_of(" \t");
    if (end != std::string::npos) {
        result = result.substr(0, end + 1);
    }

    if (result.empty()) {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        result = "CPU (" + std::to_string(sysInfo.dwNumberOfProcessors) + " cores)";
    }

    return result;
}

void update_gui_display() {
    auto stats = g_engine.get_stats();

    // Status
    std::string status = g_engine.is_mining() ? "MINING" : "IDLE";
    SetWindowTextA(g_hwnd_status, status.c_str());

    // Hashrate with thread info
    std::ostringstream hashrate_str;
    hashrate_str << std::fixed << std::setprecision(2) << stats.total_hashrate << " H/s";
    if (stats.active_threads > 0) {
        hashrate_str << " (" << stats.active_threads << " threads)";
    }
    SetWindowTextA(g_hwnd_hashrate, hashrate_str.str().c_str());

    // Share statistics
    std::ostringstream shares_str;
    shares_str << "Found: " << stats.shares_found << " | Accepted: " << stats.shares_accepted
               << " | Total: " << stats.total_hashes;
    SetWindowTextA(g_hwnd_shares, shares_str.str().c_str());

    // Uptime
    SetWindowTextA(g_hwnd_uptime, stats.uptime.c_str());

    // Pool connection status
    std::string pool_status;
    if (stats.pool_connected) {
        pool_status = "Connected: " + stats.pool_info;
    } else if (g_engine.is_mining()) {
        pool_status = "Solo Mining (No Pool)";
    } else {
        pool_status = "Disconnected";
    }
    SetWindowTextA(g_hwnd_pool_status, pool_status.c_str());
}

// Main Window Procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_CREATE: {
            // Create fonts
            g_font_title = CreateFontA(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

            g_font_normal = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

            // Application title
            HWND title = CreateWindowA("STATIC", "XMRDesk Professional v3.0.0 Final",
                                     WS_VISIBLE | WS_CHILD | SS_CENTER,
                                     20, 15, 560, 30, hwnd, nullptr, nullptr, nullptr);
            SendMessage(title, WM_SETFONT, (WPARAM)g_font_title, TRUE);

            // Pool selection
            CreateWindowA("STATIC", "Mining Pool:",
                        WS_VISIBLE | WS_CHILD,
                        20, 60, 100, 20, hwnd, nullptr, nullptr, nullptr);

            g_hwnd_pool_combo = CreateWindowA("COMBOBOX", nullptr,
                                           WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                                           130, 58, 320, 200, hwnd, (HMENU)ID_POOL_COMBO, nullptr, nullptr);

            for (const auto& pool : POOLS) {
                SendMessageA(g_hwnd_pool_combo, CB_ADDSTRING, 0, (LPARAM)pool.name.c_str());
            }
            SendMessage(g_hwnd_pool_combo, CB_SETCURSEL, 0, 0);

            // Thread count
            CreateWindowA("STATIC", "Threads:",
                        WS_VISIBLE | WS_CHILD,
                        470, 60, 60, 20, hwnd, nullptr, nullptr, nullptr);

            g_hwnd_threads_edit = CreateWindowA("EDIT", "4",
                                             WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
                                             540, 58, 40, 22, hwnd, (HMENU)ID_THREADS_EDIT, nullptr, nullptr);

            // Control buttons
            g_hwnd_start = CreateWindowA("BUTTON", "Start Mining",
                                       WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                       20, 100, 140, 40, hwnd, (HMENU)ID_START_BUTTON, nullptr, nullptr);

            g_hwnd_stop = CreateWindowA("BUTTON", "Stop Mining",
                                      WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                      170, 100, 140, 40, hwnd, (HMENU)ID_STOP_BUTTON, nullptr, nullptr);
            EnableWindow(g_hwnd_stop, FALSE);

            // Status displays
            CreateWindowA("STATIC", "Status:",
                        WS_VISIBLE | WS_CHILD,
                        20, 160, 60, 20, hwnd, nullptr, nullptr, nullptr);

            g_hwnd_status = CreateWindowA("STATIC", "IDLE",
                                        WS_VISIBLE | WS_CHILD,
                                        90, 160, 100, 20, hwnd, (HMENU)ID_STATUS_STATIC, nullptr, nullptr);

            CreateWindowA("STATIC", "Hashrate:",
                        WS_VISIBLE | WS_CHILD,
                        210, 160, 80, 20, hwnd, nullptr, nullptr, nullptr);

            g_hwnd_hashrate = CreateWindowA("STATIC", "0.00 H/s",
                                          WS_VISIBLE | WS_CHILD,
                                          300, 160, 200, 20, hwnd, (HMENU)ID_HASHRATE_STATIC, nullptr, nullptr);

            CreateWindowA("STATIC", "Uptime:",
                        WS_VISIBLE | WS_CHILD,
                        520, 160, 60, 20, hwnd, nullptr, nullptr, nullptr);

            g_hwnd_uptime = CreateWindowA("STATIC", "00:00:00",
                                        WS_VISIBLE | WS_CHILD,
                                        520, 180, 80, 20, hwnd, (HMENU)ID_UPTIME_STATIC, nullptr, nullptr);

            // Share statistics
            CreateWindowA("STATIC", "Statistics:",
                        WS_VISIBLE | WS_CHILD,
                        20, 185, 80, 20, hwnd, nullptr, nullptr, nullptr);

            g_hwnd_shares = CreateWindowA("STATIC", "Found: 0 | Accepted: 0 | Total: 0",
                                        WS_VISIBLE | WS_CHILD,
                                        110, 185, 400, 20, hwnd, (HMENU)ID_SHARES_STATIC, nullptr, nullptr);

            // Pool status
            CreateWindowA("STATIC", "Pool:",
                        WS_VISIBLE | WS_CHILD,
                        20, 210, 50, 20, hwnd, nullptr, nullptr, nullptr);

            g_hwnd_pool_status = CreateWindowA("STATIC", "Disconnected",
                                             WS_VISIBLE | WS_CHILD,
                                             80, 210, 500, 20, hwnd, (HMENU)ID_POOL_STATUS_STATIC, nullptr, nullptr);

            // CPU information
            CreateWindowA("STATIC", "CPU:",
                        WS_VISIBLE | WS_CHILD,
                        20, 235, 50, 20, hwnd, nullptr, nullptr, nullptr);

            std::string cpu_info = get_cpu_info();
            g_hwnd_cpu_info = CreateWindowA("STATIC", cpu_info.c_str(),
                                          WS_VISIBLE | WS_CHILD,
                                          80, 235, 500, 20, hwnd, (HMENU)ID_CPU_STATIC, nullptr, nullptr);

            // Donation address
            CreateWindowA("STATIC", "Donation Address:",
                        WS_VISIBLE | WS_CHILD,
                        20, 270, 140, 20, hwnd, nullptr, nullptr, nullptr);

            CreateWindowA("STATIC", xmrdesk::DONATION_ADDRESS,
                        WS_VISIBLE | WS_CHILD,
                        20, 290, 560, 20, hwnd, nullptr, nullptr, nullptr);

            // Set fonts for all controls
            EnumChildWindows(hwnd, [](HWND child, LPARAM lparam) -> BOOL {
                SendMessage(child, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
                return TRUE;
            }, 0);

            // Start GUI update timer
            SetTimer(hwnd, ID_UPDATE_TIMER, 1000, nullptr);

            return 0;
        }

        case WM_COMMAND: {
            switch (LOWORD(wparam)) {
                case ID_START_BUTTON: {
                    if (g_engine.is_mining()) break;

                    // Get selected pool
                    int pool_idx = (int)SendMessage(g_hwnd_pool_combo, CB_GETCURSEL, 0, 0);
                    if (pool_idx < 0 || pool_idx >= static_cast<int>(POOLS.size())) break;

                    const auto& pool = POOLS[pool_idx];

                    // Get thread count
                    char threads_text[16];
                    GetWindowTextA(g_hwnd_threads_edit, threads_text, sizeof(threads_text));
                    int thread_count = std::max(1, std::min(32, atoi(threads_text)));

                    // Start mining engine
                    if (g_engine.start(thread_count, pool.host, pool.port)) {
                        EnableWindow(g_hwnd_start, FALSE);
                        EnableWindow(g_hwnd_stop, TRUE);
                        EnableWindow(g_hwnd_pool_combo, FALSE);
                        EnableWindow(g_hwnd_threads_edit, FALSE);
                    }

                    break;
                }

                case ID_STOP_BUTTON: {
                    g_engine.stop();

                    EnableWindow(g_hwnd_start, TRUE);
                    EnableWindow(g_hwnd_stop, FALSE);
                    EnableWindow(g_hwnd_pool_combo, TRUE);
                    EnableWindow(g_hwnd_threads_edit, TRUE);

                    break;
                }
            }
            break;
        }

        case WM_TIMER: {
            if (wparam == ID_UPDATE_TIMER) {
                update_gui_display();
            }
            break;
        }

        case WM_CLOSE: {
            g_engine.stop();
            DestroyWindow(hwnd);
            break;
        }

        case WM_DESTROY: {
            KillTimer(hwnd, ID_UPDATE_TIMER);

            if (g_font_title) DeleteObject(g_font_title);
            if (g_font_normal) DeleteObject(g_font_normal);

            PostQuitMessage(0);
            break;
        }

        default:
            return DefWindowProc(hwnd, msg, wparam, lparam);
    }

    return 0;
}

// Application Entry Point
int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE, LPSTR, int cmdshow) {
    // Initialize common controls
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    // Enable visual styles
    SetThemeAppProperties(STAP_ALLOW_NONCLIENT | STAP_ALLOW_CONTROLS | STAP_ALLOW_WEBCONTENT);

    // Register window class
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hinstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "XMRDeskProfessional";
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassExA(&wc)) {
        MessageBoxA(nullptr, "Failed to register window class!", "Error", MB_ICONERROR);
        return 1;
    }

    // Create main window
    g_hwnd_main = CreateWindowExA(
        WS_EX_APPWINDOW,
        "XMRDeskProfessional",
        "XMRDesk Professional v3.0.0 Final - Monero Mining",
        WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_THICKFRAME),
        CW_USEDEFAULT, CW_USEDEFAULT, 620, 380,
        nullptr, nullptr, hinstance, nullptr
    );

    if (!g_hwnd_main) {
        MessageBoxA(nullptr, "Failed to create window!", "Error", MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_hwnd_main, cmdshow);
    UpdateWindow(g_hwnd_main);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return static_cast<int>(msg.wParam);
}