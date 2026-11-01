#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>

#include "../server/http_server.h"

using namespace workbench;

static std::atomic<bool> g_running{true};

void open_browser(const std::string& url) {
    ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

// WinMain entry point ensures no ugly command prompt popup if launched from explorer
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    // Determine directory containing Launch.exe
    char exe_path[MAX_PATH];
    GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    std::string exe_dir(exe_path);
    size_t last_slash = exe_dir.find_last_of("\\/");
    if (last_slash != std::string::npos) {
        exe_dir = exe_dir.substr(0, last_slash);
    }
    SetCurrentDirectoryA(exe_dir.c_str());

    // Try starting server on port 8080, or 8081..8090 if already bound
    int port = 8080;
    std::unique_ptr<server::HttpServer> srv;
    for (int p = 8080; p <= 8095; ++p) {
        srv = std::make_unique<server::HttpServer>("127.0.0.1", p, "src/frontend");
        if (srv->start()) {
            port = p;
            break;
        }
    }

    if (!srv || !srv->is_running()) {
        MessageBoxA(NULL, 
            "Failed to bind HTTP server to local ports (8080-8095).\nPlease check if another application is using the ports.", 
            "Unified Engineering Workbench - Launch Error", 
            MB_ICONERROR | MB_OK);
        return 1;
    }

    std::string app_url = "http://127.0.0.1:" + std::to_string(port) + "/index.html";
    
    // Launch default web browser to the workbench UI
    open_browser(app_url);

    // Keep server process running while browser is open or until user exits
    while (g_running.load() && srv->is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    srv->stop();
    return 0;
}
