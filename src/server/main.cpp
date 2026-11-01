#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include "http_server.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

using namespace workbench;

static std::atomic<bool> g_keep_running{true};

void signal_handler(int) {
    g_keep_running.store(false);
}

void open_browser(const std::string& url) {
#ifdef _WIN32
    ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
#else
    std::string cmd = "xdg-open " + url;
    system(cmd.c_str());
#endif
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    int port = 8080;
    bool auto_open = true;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--no-browser") {
            auto_open = false;
        } else if (arg == "--help") {
            std::cout << "Unified Engineering Workbench - Native Server\n";
            std::cout << "Usage: workbench_server [--port <number>] [--no-browser]\n";
            return 0;
        }
    }

    std::cout << "========================================================\n";
    std::cout << "       UNIFIED ENGINEERING WORKBENCH SERVER CORE        \n";
    std::cout << "========================================================\n";

    server::HttpServer server("127.0.0.1", port, "src/frontend");
    if (!server.start()) {
        std::cerr << "[-] Failed to start HTTP server on port " << port << ".\n";
        return 1;
    }

    std::string url = "http://127.0.0.1:" + std::to_string(port);
    std::cout << "[+] Server listening at: " << url << "\n";
    std::cout << "[+] Static assets directory: src/frontend\n";
    std::cout << "[+] C++ backend ready. Press Ctrl+C to terminate.\n";

    if (auto_open) {
        std::cout << "[*] Launching user interface in default web browser...\n";
        open_browser(url);
    }

    while (g_keep_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "\n[*] Shutting down server gracefully...\n";
    server.stop();
    std::cout << "[+] Server stopped.\n";
    return 0;
}
