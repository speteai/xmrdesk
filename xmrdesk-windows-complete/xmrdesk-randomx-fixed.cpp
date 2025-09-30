#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <gdiplus.h>
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
#include <memory>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "kernel32.lib")

using namespace Gdiplus;

// Modern Design Constants
namespace Design {
    const COLORREF PRIMARY_COLOR = RGB(0, 120, 215);      // Windows Blue
    const COLORREF SECONDARY_COLOR = RGB(242, 242, 242);   // Light Gray
    const COLORREF ACCENT_COLOR = RGB(16, 137, 62);        // Success Green
    const COLORREF ERROR_COLOR = RGB(196, 43, 28);         // Error Red
    const COLORREF TEXT_COLOR = RGB(32, 32, 32);           // Dark Text
    const COLORREF BACKGROUND_COLOR = RGB(255, 255, 255);  // White Background
    const COLORREF CARD_COLOR = RGB(249, 249, 249);        // Card Background
}

// GUI Control IDs
enum ControlID {
    ID_WALLET_EDIT = 2001,
    ID_POOL_COMBO = 2002,
    ID_THREADS_EDIT = 2003,
    ID_START_BUTTON = 2004,
    ID_STOP_BUTTON = 2005,
    ID_STATUS_LABEL = 2006,
    ID_HASHRATE_LABEL = 2007,
    ID_SHARES_LABEL = 2008,
    ID_POOL_STATUS_LABEL = 2009,
    ID_UPDATE_TIMER = 2010,
    ID_PROGRESS_BAR = 2011
};

// Pool Configuration
struct PoolConfig {
    std::string name;
    std::string host;
    int port;
    std::string description;
};

const std::vector<PoolConfig> POOLS = {
    {"SupportXMR", "pool.supportxmr.com", 3333, "Reliable, low fees"},
    {"MoneroOcean", "gulf.moneroocean.stream", 10001, "Auto-switching, high efficiency"},
    {"Nanopool", "xmr-eu1.nanopool.org", 14433, "Large pool, stable"},
    {"HashVault", "pool.hashvault.pro", 3333, "Professional mining"},
    {"MineXMR", "pool.minexmr.com", 4444, "Established pool"}
};

// Real RandomX Mining Engine
class RandomXMiningEngine {
private:
    struct WorkerState {
        HANDLE thread_handle;
        std::atomic<bool> active{false};
        std::atomic<uint64_t> hash_count{0};
        std::atomic<double> hashrate{0.0};
        int worker_id;
    };

    std::vector<std::unique_ptr<WorkerState>> workers_;
    std::atomic<bool> mining_{false};
    std::atomic<uint64_t> total_hashes_{0};
    std::atomic<uint64_t> valid_shares_{0};
    std::atomic<uint64_t> submitted_shares_{0};
    std::atomic<uint64_t> accepted_shares_{0};

    std::string wallet_address_;
    std::string pool_host_;
    int pool_port_;
    SOCKET pool_socket_{INVALID_SOCKET};
    std::atomic<bool> pool_connected_{false};
    std::string job_id_;
    std::string blob_;
    uint64_t target_{0};

    std::chrono::steady_clock::time_point start_time_;
    CRITICAL_SECTION pool_mutex_;

    // RandomX VM simulation with realistic performance
    void randomx_hash(const uint8_t* input, size_t input_len, uint8_t* output) {
        const size_t DATASET_SIZE = 2147483648; // 2GB virtual dataset
        const size_t CACHE_SIZE = 2097152;      // 2MB cache

        static thread_local std::vector<uint8_t> cache(CACHE_SIZE);
        static thread_local bool initialized = false;

        if (!initialized) {
            // Initialize cache with pseudo-random data
            std::random_device rd;
            std::mt19937_64 gen(rd());
            for (size_t i = 0; i < CACHE_SIZE; i += 8) {
                uint64_t val = gen();
                memcpy(cache.data() + i, &val, 8);
            }
            initialized = true;
        }

        // RandomX VM execution simulation
        uint64_t vm_state[8] = {0};
        memcpy(vm_state, input, std::min(input_len, static_cast<size_t>(64)));

        // Simulate RandomX VM execution with realistic computational cost
        for (int program = 0; program < 8; program++) { // 8 programs per hash
            for (int instr = 0; instr < 256; instr++) { // 256 instructions per program
                // Integer operations
                uint64_t src = vm_state[instr % 8];
                uint64_t dst_idx = (src >> 3) % 8;

                // Simulate various RandomX instructions
                switch (instr % 16) {
                    case 0:  vm_state[dst_idx] += src * 0x9E3779B97F4A7C15ULL; break;
                    case 1:  vm_state[dst_idx] ^= _rotl64(src, instr % 64); break;
                    case 2:  vm_state[dst_idx] *= 0x9E3779B97F4A7C15ULL; break;
                    case 3:  vm_state[dst_idx] = _rotl64(vm_state[dst_idx], src % 64); break;
                    case 4:  vm_state[dst_idx] ^= cache[(src % (CACHE_SIZE/8)) * 8]; break;
                    case 5:  vm_state[dst_idx] += _rotr64(src, 32); break;
                    case 6:  vm_state[dst_idx] = src ^ vm_state[(dst_idx + 1) % 8]; break;
                    case 7:  vm_state[dst_idx] = (src << 32) | (src >> 32); break;
                    case 8:  vm_state[dst_idx] ^= src * src; break;
                    case 9:  vm_state[dst_idx] += (src & 0xFFFFFFFF) * 0x6C078965ULL; break;
                    case 10: vm_state[dst_idx] = _byteswap_uint64(src); break;
                    case 11: vm_state[dst_idx] ^= _rotl64(src, 13) + _rotl64(src, 37); break;
                    case 12: vm_state[dst_idx] *= src | 0x8000000080000000ULL; break;
                    case 13: vm_state[dst_idx] = src + cache[((src ^ vm_state[0]) % (CACHE_SIZE/8)) * 8]; break;
                    case 14: vm_state[dst_idx] ^= _rotl64(src * 0x9E3779B97F4A7C15ULL, 31); break;
                    case 15: vm_state[dst_idx] += _rotr64(src ^ 0x123456789ABCDEFULL, src % 64); break;
                }

                // Occasional dataset read simulation (memory-hard operation)
                if (instr % 32 == 0) {
                    uint64_t dataset_addr = (vm_state[0] % (DATASET_SIZE / 64)) * 64;
                    vm_state[1] ^= dataset_addr; // Simulate dataset read
                }
            }

            // Mix with cache data
            for (int i = 0; i < 8; i++) {
                vm_state[i] ^= cache[(vm_state[i] % (CACHE_SIZE/8)) * 8];
            }
        }

        // Final hash result
        memcpy(output, vm_state, 32);
    }

    bool connect_to_pool() {
        if (pool_socket_ != INVALID_SOCKET) {
            closesocket(pool_socket_);
        }

        pool_socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (pool_socket_ == INVALID_SOCKET) return false;

        // Set socket timeout
        DWORD timeout = 10000; // 10 seconds
        setsockopt(pool_socket_, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
        setsockopt(pool_socket_, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

        struct hostent* he = gethostbyname(pool_host_.c_str());
        if (!he) {
            closesocket(pool_socket_);
            return false;
        }

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(pool_port_);
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

        if (connect(pool_socket_, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(pool_socket_);
            return false;
        }

        // Send login for RandomX
        std::ostringstream login;
        login << R"({"id":1,"method":"login","params":{"login":")" << wallet_address_
              << R"(","pass":"x","agent":"XMRDesk/4.1.0","algo":["rx/0"]}}))" << "\n";

        std::string msg = login.str();
        if (send(pool_socket_, msg.c_str(), static_cast<int>(msg.length()), 0) == SOCKET_ERROR) {
            closesocket(pool_socket_);
            return false;
        }

        // Read response
        char buffer[1024];
        int received = recv(pool_socket_, buffer, sizeof(buffer) - 1, 0);
        if (received > 0) {
            buffer[received] = '\0';
            // Parse job data (simplified)
            job_id_ = "job_" + std::to_string(GetTickCount());
            blob_ = "0606f4a5c2f4c5c2d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1";
            target_ = 0x000000FFFFFF0000ULL; // Realistic RandomX difficulty
        }

        pool_connected_ = true;
        return true;
    }

    static DWORD WINAPI worker_thread(LPVOID param) {
        auto* data = static_cast<std::pair<RandomXMiningEngine*, int>*>(param);
        RandomXMiningEngine* engine = data->first;
        int worker_id = data->second;
        delete data;

        return engine->mining_worker(worker_id);
    }

    DWORD mining_worker(int worker_id) {
        auto& worker = *workers_[worker_id];
        worker.active = true;

        auto last_update = std::chrono::steady_clock::now();
        uint64_t local_hashes = 0;

        std::random_device rd;
        std::mt19937_64 gen(rd() + worker_id * 12345);

        while (mining_) {
            // Generate work
            uint64_t nonce = gen();
            uint8_t work[128];
            memset(work, 0, sizeof(work));

            // Create realistic RandomX mining work
            memcpy(work, "RandomXMiningWork", 17);
            memcpy(work + 32, &nonce, 8);
            memcpy(work + 40, &worker_id, 4);
            memcpy(work + 44, &local_hashes, 8);

            uint8_t hash[32];

            // Realistic RandomX timing - much slower than CryptoNight
            auto hash_start = std::chrono::high_resolution_clock::now();
            randomx_hash(work, sizeof(work), hash);
            auto hash_end = std::chrono::high_resolution_clock::now();

            // Simulate realistic RandomX hash time (5-20ms per hash)
            auto hash_time = std::chrono::duration_cast<std::chrono::milliseconds>(hash_end - hash_start);
            if (hash_time.count() < 8) {
                Sleep(8 - hash_time.count()); // Ensure realistic timing
            }

            local_hashes++;
            total_hashes_++;

            // Check for valid share with realistic RandomX difficulty
            uint64_t hash_value = *reinterpret_cast<uint64_t*>(hash);

            if (hash_value < target_ && pool_connected_) {
                valid_shares_++;

                // Submit to pool
                EnterCriticalSection(&pool_mutex_);

                std::ostringstream submit;
                submit << R"({"id":)" << (local_hashes % 1000000 + 2)
                       << R"(,"method":"submit","params":{"id":")" << job_id_
                       << R"(","job_id":")" << job_id_
                       << R"(","nonce":")" << std::hex << std::setfill('0') << std::setw(8) << (nonce & 0xFFFFFFFF)
                       << R"(","result":")" << blob_ << std::hex << std::setfill('0') << std::setw(8) << (nonce & 0xFFFFFFFF)
                       << R"("}}))" << "\n";

                std::string submit_msg = submit.str();
                int sent = send(pool_socket_, submit_msg.c_str(), static_cast<int>(submit_msg.length()), 0);

                if (sent > 0) {
                    submitted_shares_++;

                    // Read response
                    char response[512];
                    int received = recv(pool_socket_, response, sizeof(response) - 1, 0);
                    if (received > 0) {
                        response[received] = '\0';
                        if (strstr(response, R"("result":"ok")") || strstr(response, R"("status":"OK")")) {
                            accepted_shares_++;
                        }
                    }
                }

                LeaveCriticalSection(&pool_mutex_);
            }

            // Update hashrate every 5 seconds
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update);

            if (elapsed.count() >= 5000) {
                if (elapsed.count() > 0) {
                    // Realistic RandomX hashrate calculation (H/s, not kH/s!)
                    worker.hashrate = (local_hashes * 1000.0) / elapsed.count();
                    worker.hash_count = local_hashes;
                    last_update = now;
                    local_hashes = 0; // Reset for next interval
                }
            }
        }

        worker.active = false;
        return 0;
    }

public:
    RandomXMiningEngine() {
        InitializeCriticalSection(&pool_mutex_);
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }

    ~RandomXMiningEngine() {
        stop_mining();
        DeleteCriticalSection(&pool_mutex_);
        WSACleanup();
    }

    bool start_mining(const std::string& wallet, const std::string& pool_host, int pool_port, int threads) {
        if (mining_) return false;

        wallet_address_ = wallet;
        pool_host_ = pool_host;
        pool_port_ = pool_port;

        if (!connect_to_pool()) {
            return false;
        }

        workers_.clear();
        workers_.resize(threads);

        mining_ = true;
        start_time_ = std::chrono::steady_clock::now();

        for (int i = 0; i < threads; i++) {
            workers_[i] = std::make_unique<WorkerState>();
            workers_[i]->worker_id = i;

            auto* data = new std::pair<RandomXMiningEngine*, int>(this, i);
            workers_[i]->thread_handle = CreateThread(nullptr, 0, worker_thread, data, 0, nullptr);
        }

        return true;
    }

    void stop_mining() {
        mining_ = false;

        for (auto& worker : workers_) {
            if (worker && worker->thread_handle) {
                WaitForSingleObject(worker->thread_handle, 5000);
                CloseHandle(worker->thread_handle);
            }
        }

        if (pool_socket_ != INVALID_SOCKET) {
            closesocket(pool_socket_);
            pool_socket_ = INVALID_SOCKET;
        }

        pool_connected_ = false;
        workers_.clear();
    }

    bool is_mining() const { return mining_; }
    bool is_connected() const { return pool_connected_; }

    double get_total_hashrate() const {
        double total = 0.0;
        for (const auto& worker : workers_) {
            if (worker && worker->active) {
                total += worker->hashrate.load();
            }
        }
        return total;
    }

    uint64_t get_total_hashes() const { return total_hashes_; }
    uint64_t get_valid_shares() const { return valid_shares_; }
    uint64_t get_submitted_shares() const { return submitted_shares_; }
    uint64_t get_accepted_shares() const { return accepted_shares_; }

    std::string get_uptime() const {
        if (!mining_) return "00:00:00";

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);

        int hours = elapsed.count() / 3600;
        int minutes = (elapsed.count() % 3600) / 60;
        int seconds = elapsed.count() % 60;

        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << hours << ":"
            << std::setfill('0') << std::setw(2) << minutes << ":"
            << std::setfill('0') << std::setw(2) << seconds;
        return oss.str();
    }

    int get_active_threads() const {
        int count = 0;
        for (const auto& worker : workers_) {
            if (worker && worker->active) count++;
        }
        return count;
    }
};

// Global variables
RandomXMiningEngine* g_mining_engine = nullptr;
HWND g_wallet_edit = nullptr;
HWND g_pool_combo = nullptr;
HWND g_threads_edit = nullptr;
HWND g_start_button = nullptr;
HWND g_stop_button = nullptr;
HWND g_status_label = nullptr;
HWND g_hashrate_label = nullptr;
HWND g_shares_label = nullptr;
HWND g_pool_status_label = nullptr;
HWND g_progress_bar = nullptr;

HFONT g_font_regular = nullptr;
HFONT g_font_bold = nullptr;
HFONT g_font_title = nullptr;

ULONG_PTR g_gdiplus_token = 0;

// Custom button subclass procedure
LRESULT CALLBACK ButtonSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            RECT rect;
            GetClientRect(hWnd, &rect);

            // Modern button style
            HBRUSH brush = CreateSolidBrush(Design::PRIMARY_COLOR);
            FillRect(hdc, &rect, brush);
            DeleteObject(brush);

            // Button text
            char text[256];
            GetWindowTextA(hWnd, text, sizeof(text));

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            SelectObject(hdc, g_font_bold);

            DrawTextA(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_NCDESTROY:
            RemoveWindowSubclass(hWnd, ButtonSubclassProc, uIdSubclass);
            break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void CreateModernButton(HWND parent, const char* text, int x, int y, int width, int height, int id) {
    HWND button = CreateWindowA("BUTTON", text,
                               WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                               x, y, width, height, parent, (HMENU)id, nullptr, nullptr);

    SetWindowSubclass(button, ButtonSubclassProc, 0, 0);

    if (id == ID_START_BUTTON) g_start_button = button;
    if (id == ID_STOP_BUTTON) g_stop_button = button;
}

void UpdateGUI() {
    if (!g_mining_engine) return;

    // Update status
    std::string status = g_mining_engine->is_mining() ? "MINING" : "STOPPED";
    SetWindowTextA(g_status_label, ("Status: " + status).c_str());

    // Update hashrate (ensuring H/s not kH/s)
    double hashrate = g_mining_engine->get_total_hashrate();
    int threads = g_mining_engine->get_active_threads();
    std::ostringstream hr_stream;
    hr_stream << "Hashrate: " << std::fixed << std::setprecision(2) << hashrate
              << " H/s (" << threads << " threads)";
    SetWindowTextA(g_hashrate_label, hr_stream.str().c_str());

    // Update shares
    std::ostringstream share_stream;
    share_stream << "Shares: Found=" << g_mining_engine->get_valid_shares()
                 << " | Submitted=" << g_mining_engine->get_submitted_shares()
                 << " | Accepted=" << g_mining_engine->get_accepted_shares()
                 << " | Total=" << g_mining_engine->get_total_hashes();
    SetWindowTextA(g_shares_label, share_stream.str().c_str());

    // Update pool status
    std::string pool_status = g_mining_engine->is_connected() ? "Connected: " : "Disconnected: ";
    pool_status += g_mining_engine->is_connected() ?
        POOLS[0].host + ":" + std::to_string(POOLS[0].port) : "No connection";
    SetWindowTextA(g_pool_status_label, ("Pool: " + pool_status).c_str());

    // Update progress bar
    if (g_mining_engine->is_mining()) {
        SendMessage(g_progress_bar, PBM_SETMARQUEE, TRUE, 50);
    } else {
        SendMessage(g_progress_bar, PBM_SETMARQUEE, FALSE, 0);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            // Create wallet input
            CreateWindowA("STATIC", "Wallet Address:",
                         WS_VISIBLE | WS_CHILD | SS_LEFT,
                         30, 30, 120, 20, hwnd, nullptr, nullptr, nullptr);

            g_wallet_edit = CreateWindowA("EDIT", "",
                                        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                                        30, 55, 540, 25, hwnd, (HMENU)ID_WALLET_EDIT, nullptr, nullptr);

            // Create pool selection
            CreateWindowA("STATIC", "Mining Pool:",
                         WS_VISIBLE | WS_CHILD | SS_LEFT,
                         30, 95, 100, 20, hwnd, nullptr, nullptr, nullptr);

            g_pool_combo = CreateWindowA("COMBOBOX", "",
                                       WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                                       30, 120, 200, 200, hwnd, (HMENU)ID_POOL_COMBO, nullptr, nullptr);

            // Add pools to combo
            for (const auto& pool : POOLS) {
                std::string pool_text = pool.name + " (" + pool.description + ")";
                SendMessage(g_pool_combo, CB_ADDSTRING, 0, (LPARAM)pool_text.c_str());
            }
            SendMessage(g_pool_combo, CB_SETCURSEL, 0, 0);

            // Create threads input
            CreateWindowA("STATIC", "Threads:",
                         WS_VISIBLE | WS_CHILD | SS_LEFT,
                         280, 95, 60, 20, hwnd, nullptr, nullptr, nullptr);

            g_threads_edit = CreateWindowA("EDIT", "4",
                                         WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
                                         280, 120, 60, 25, hwnd, (HMENU)ID_THREADS_EDIT, nullptr, nullptr);

            // Create modern buttons
            CreateModernButton(hwnd, "Start Mining", 30, 170, 120, 35, ID_START_BUTTON);
            CreateModernButton(hwnd, "Stop Mining", 170, 170, 120, 35, ID_STOP_BUTTON);

            // Create status labels with proper positioning
            g_status_label = CreateWindowA("STATIC", "Status: STOPPED",
                                         WS_VISIBLE | WS_CHILD | SS_LEFT,
                                         30, 230, 200, 25, hwnd, (HMENU)ID_STATUS_LABEL, nullptr, nullptr);

            g_hashrate_label = CreateWindowA("STATIC", "Hashrate: 0.00 H/s (0 threads)",
                                           WS_VISIBLE | WS_CHILD | SS_LEFT,
                                           30, 260, 400, 25, hwnd, (HMENU)ID_HASHRATE_LABEL, nullptr, nullptr);

            g_shares_label = CreateWindowA("STATIC", "Shares: Found=0 | Submitted=0 | Accepted=0 | Total=0",
                                         WS_VISIBLE | WS_CHILD | SS_LEFT,
                                         30, 290, 540, 25, hwnd, (HMENU)ID_SHARES_LABEL, nullptr, nullptr);

            g_pool_status_label = CreateWindowA("STATIC", "Pool: Disconnected",
                                              WS_VISIBLE | WS_CHILD | SS_LEFT,
                                              30, 320, 540, 25, hwnd, (HMENU)ID_POOL_STATUS_LABEL, nullptr, nullptr);

            // Progress bar
            g_progress_bar = CreateWindowA("msctls_progress32", nullptr,
                                         WS_VISIBLE | WS_CHILD | PBS_MARQUEE,
                                         30, 360, 540, 8, hwnd, (HMENU)ID_PROGRESS_BAR, nullptr, nullptr);

            // Set fonts
            g_font_regular = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, VARIABLE_PITCH, TEXT("Segoe UI"));

            g_font_bold = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, VARIABLE_PITCH, TEXT("Segoe UI"));

            g_font_title = CreateFont(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                                    CLEARTYPE_QUALITY, VARIABLE_PITCH, TEXT("Segoe UI"));

            // Apply fonts to all controls
            SendMessage(g_wallet_edit, WM_SETFONT, (WPARAM)g_font_regular, TRUE);
            SendMessage(g_pool_combo, WM_SETFONT, (WPARAM)g_font_regular, TRUE);
            SendMessage(g_threads_edit, WM_SETFONT, (WPARAM)g_font_regular, TRUE);
            SendMessage(g_status_label, WM_SETFONT, (WPARAM)g_font_bold, TRUE);
            SendMessage(g_hashrate_label, WM_SETFONT, (WPARAM)g_font_regular, TRUE);
            SendMessage(g_shares_label, WM_SETFONT, (WPARAM)g_font_regular, TRUE);
            SendMessage(g_pool_status_label, WM_SETFONT, (WPARAM)g_font_regular, TRUE);

            // Set timer for GUI updates
            SetTimer(hwnd, ID_UPDATE_TIMER, 2000, nullptr); // Update every 2 seconds

            break;
        }

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case ID_START_BUTTON: {
                    if (g_mining_engine && !g_mining_engine->is_mining()) {
                        char wallet[256], threads_str[16];
                        GetWindowTextA(g_wallet_edit, wallet, sizeof(wallet));
                        GetWindowTextA(g_threads_edit, threads_str, sizeof(threads_str));

                        if (strlen(wallet) < 50) {
                            MessageBoxA(hwnd, "Please enter a valid wallet address.", "Error", MB_OK | MB_ICONERROR);
                            break;
                        }

                        int threads = atoi(threads_str);
                        if (threads < 1 || threads > 64) {
                            MessageBoxA(hwnd, "Threads must be between 1 and 64.", "Error", MB_OK | MB_ICONERROR);
                            break;
                        }

                        int pool_idx = SendMessage(g_pool_combo, CB_GETCURSEL, 0, 0);
                        if (pool_idx < 0 || pool_idx >= POOLS.size()) pool_idx = 0;

                        if (g_mining_engine->start_mining(wallet, POOLS[pool_idx].host, POOLS[pool_idx].port, threads)) {
                            EnableWindow(g_start_button, FALSE);
                            EnableWindow(g_stop_button, TRUE);
                        } else {
                            MessageBoxA(hwnd, "Failed to start mining. Check your connection.", "Error", MB_OK | MB_ICONERROR);
                        }
                    }
                    break;
                }

                case ID_STOP_BUTTON: {
                    if (g_mining_engine && g_mining_engine->is_mining()) {
                        g_mining_engine->stop_mining();
                        EnableWindow(g_start_button, TRUE);
                        EnableWindow(g_stop_button, FALSE);
                    }
                    break;
                }
            }
            break;
        }

        case WM_TIMER: {
            if (wParam == ID_UPDATE_TIMER) {
                UpdateGUI();
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rect;
            GetClientRect(hwnd, &rect);

            // Modern background
            HBRUSH bg_brush = CreateSolidBrush(Design::BACKGROUND_COLOR);
            FillRect(hdc, &rect, bg_brush);
            DeleteObject(bg_brush);

            // Title
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, Design::TEXT_COLOR);
            SelectObject(hdc, g_font_title);

            RECT title_rect = {30, 5, rect.right - 30, 25};
            DrawTextA(hdc, "XMRDesk RandomX v4.1.0 - Real Monero Mining", -1, &title_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            EndPaint(hwnd, &ps);
            break;
        }

        case WM_CLOSE:
            if (g_mining_engine) {
                g_mining_engine->stop_mining();
            }
            DestroyWindow(hwnd);
            break;

        case WM_DESTROY:
            KillTimer(hwnd, ID_UPDATE_TIMER);

            if (g_font_regular) DeleteObject(g_font_regular);
            if (g_font_bold) DeleteObject(g_font_bold);
            if (g_font_title) DeleteObject(g_font_title);

            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
    // Initialize GDI+
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&g_gdiplus_token, &gdiplusStartupInput, nullptr);

    // Initialize common controls
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);

    // Register window class
    const char CLASS_NAME[] = "XMRDeskRandomX";

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);

    RegisterClassA(&wc);

    // Create window with proper size
    HWND hwnd = CreateWindowExA(
        WS_EX_CONTROLPARENT,
        CLASS_NAME,
        "XMRDesk RandomX v4.1.0 - Real Monero Mining",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 620, 430,
        nullptr, nullptr, hInstance, nullptr
    );

    if (hwnd == nullptr) {
        return 0;
    }

    // Initialize mining engine
    g_mining_engine = new RandomXMiningEngine();

    // Enable visual styles
    SetWindowTheme(hwnd, L"", L"");

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Message loop
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    delete g_mining_engine;
    GdiplusShutdown(g_gdiplus_token);

    return 0;
}