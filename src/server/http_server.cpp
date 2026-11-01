#include "http_server.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <iomanip>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
typedef int SOCKET;
#endif

#include "../core/fea/thermal_fea.h"
#include "../core/parsers/stl_parser.h"
#include "../core/parsers/ansys_csv_parser.h"
#include "../core/multicore/batch_solver.h"
#include "../core/embedded/mock_adapters.h"
#include "../core/backprop/backprop_engine.h"

namespace workbench::server {

// Global embedded device manager instance
static embedded::EmbeddedWorkbenchManager g_embedded_manager;

// Helper: Trim string
static inline std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// Simple JSON field extractor for basic types without external heavy libraries
static std::string extract_json_string(const std::string& json, const std::string& key, const std::string& fallback = "") {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return fallback;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return fallback;
    pos = json.find('"', pos);
    if (pos == std::string::npos) return fallback;
    size_t end = json.find('"', pos + 1);
    if (end == std::string::npos) return fallback;
    return json.substr(pos + 1, end - (pos + 1));
}

static double extract_json_double(const std::string& json, const std::string& key, double fallback = 0.0) {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return fallback;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return fallback;
    pos = json.find_first_not_of(" \t\r\n", pos + 1);
    if (pos == std::string::npos) return fallback;
    size_t end = json.find_first_of(",}\r\n \t", pos);
    if (end == std::string::npos) end = json.length();
    try {
        return std::stod(json.substr(pos, end - pos));
    } catch (...) {
        return fallback;
    }
}

static int extract_json_int(const std::string& json, const std::string& key, int fallback = 0) {
    return static_cast<int>(extract_json_double(json, key, static_cast<double>(fallback)));
}

// HttpResponse constructors
HttpResponse HttpResponse::json(const std::string& content, int code) {
    HttpResponse res;
    res.status_code = code;
    res.status_text = (code == 200) ? "OK" : "Error";
    res.content_type = "application/json; charset=utf-8";
    res.body = content;
    return res;
}

HttpResponse HttpResponse::html(const std::string& content, int code) {
    HttpResponse res;
    res.status_code = code;
    res.status_text = "OK";
    res.content_type = "text/html; charset=utf-8";
    res.body = content;
    return res;
}

HttpResponse HttpResponse::css(const std::string& content, int code) {
    HttpResponse res;
    res.status_code = code;
    res.status_text = "OK";
    res.content_type = "text/css; charset=utf-8";
    res.body = content;
    return res;
}

HttpResponse HttpResponse::js(const std::string& content, int code) {
    HttpResponse res;
    res.status_code = code;
    res.status_text = "OK";
    res.content_type = "application/javascript; charset=utf-8";
    res.body = content;
    return res;
}

HttpResponse HttpResponse::text(const std::string& content, int code) {
    HttpResponse res;
    res.status_code = code;
    res.status_text = "OK";
    res.content_type = "text/plain; charset=utf-8";
    res.body = content;
    return res;
}

HttpResponse HttpResponse::error(int code, const std::string& message) {
    HttpResponse res;
    res.status_code = code;
    res.status_text = (code == 404) ? "Not Found" : ((code == 400) ? "Bad Request" : "Internal Server Error");
    res.content_type = "application/json; charset=utf-8";
    std::ostringstream oss;
    oss << "{\"error\":true,\"status\":" << code << ",\"message\":\"" << message << "\"}";
    res.body = oss.str();
    return res;
}

HttpServer::HttpServer(const std::string& host, int port, const std::string& static_dir)
    : host_(host), port_(port), static_dir_(static_dir) {
#ifdef _WIN32
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif
    register_workbench_routes();
}

HttpServer::~HttpServer() {
    stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

void HttpServer::route(const std::string& method, const std::string& path, HttpHandler handler) {
    routes_[method + " " + path] = handler;
}

bool HttpServer::start() {
    if (running_.load()) return true;

    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock == INVALID_SOCKET) {
        std::cerr << "[-] Error creating socket\n";
        return false;
    }

    // Reuse address option
    int opt = 1;
#ifdef _WIN32
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(static_cast<u_short>(port_));
    inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr);

    if (bind(listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "[-] Error binding to " << host_ << ":" << port_ << "\n";
        closesocket(listen_sock);
        return false;
    }

    if (listen(listen_sock, 32) == SOCKET_ERROR) {
        std::cerr << "[-] Error listening on socket\n";
        closesocket(listen_sock);
        return false;
    }

    listen_socket_ = static_cast<uintptr_t>(listen_sock);
    running_.store(true);

    listener_thread_ = std::make_unique<std::thread>(&HttpServer::listen_loop, this);
    std::cout << "[+] HTTP Server started on http://" << host_ << ":" << port_ << "\n";
    return true;
}

void HttpServer::stop() {
    if (!running_.load()) return;
    running_.store(false);

    if (listen_socket_ != 0) {
        closesocket(static_cast<SOCKET>(listen_socket_));
        listen_socket_ = 0;
    }

    if (listener_thread_ && listener_thread_->joinable()) {
        listener_thread_->join();
    }
}

void HttpServer::listen_loop() {
    SOCKET l_sock = static_cast<SOCKET>(listen_socket_);

    while (running_.load()) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(l_sock, &read_fds);

        timeval tv{1, 0}; // 1-second timeout for graceful exit
        int sel = select(static_cast<int>(l_sock + 1), &read_fds, nullptr, nullptr, &tv);

        if (sel > 0 && FD_ISSET(l_sock, &read_fds)) {
            sockaddr_in client_addr{};
            int addr_len = sizeof(client_addr);
            SOCKET client_sock = accept(l_sock, (struct sockaddr*)&client_addr, &addr_len);

            if (client_sock != INVALID_SOCKET) {
                std::thread client_thread(&HttpServer::handle_client, this, static_cast<uintptr_t>(client_sock));
                client_thread.detach();
            }
        }
    }
}

void HttpServer::handle_client(uintptr_t sock_ptr) {
    SOCKET client_sock = static_cast<SOCKET>(sock_ptr);

    char buffer[16384];
    std::string raw_request;
    int bytes_read = 0;

    // Read initial HTTP header block
    while ((bytes_read = recv(client_sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        raw_request.append(buffer, bytes_read);
        if (raw_request.find("\r\n\r\n") != std::string::npos) {
            break;
        }
    }

    if (raw_request.empty()) {
        closesocket(client_sock);
        return;
    }

    // Parse Request Line
    size_t header_end = raw_request.find("\r\n\r\n");
    std::string header_part = raw_request.substr(0, header_end);
    std::string body_part = (header_end != std::string::npos && header_end + 4 < raw_request.length()) 
                            ? raw_request.substr(header_end + 4) : "";

    std::istringstream stream(header_part);
    std::string req_line;
    std::getline(stream, req_line);

    std::istringstream req_iss(req_line);
    HttpRequest req;
    req_iss >> req.method >> req.path;

    // Extract query string if any
    size_t q_pos = req.path.find('?');
    if (q_pos != std::string::npos) {
        req.query_string = req.path.substr(q_pos + 1);
        req.path = req.path.substr(0, q_pos);
    }

    // Parse Headers
    std::string h_line;
    while (std::getline(stream, h_line)) {
        size_t colon = h_line.find(':');
        if (colon != std::string::npos) {
            std::string key = trim(h_line.substr(0, colon));
            std::string val = trim(h_line.substr(colon + 1));
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            req.headers[key] = val;
        }
    }

    // Check Content-Length for large body
    auto cl_it = req.headers.find("content-length");
    if (cl_it != req.headers.end()) {
        size_t content_len = 0;
        try { content_len = std::stoul(cl_it->second); } catch (...) {}
        while (body_part.length() < content_len) {
            bytes_read = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (bytes_read <= 0) break;
            body_part.append(buffer, bytes_read);
        }
    }
    req.body = body_part;

    HttpResponse response;

    // Handle OPTIONS Preflight
    if (req.method == "OPTIONS") {
        response.status_code = 204;
        response.status_text = "No Content";
        response.content_type = "text/plain";
    } else {
        // Dispatch Route
        std::string route_key = req.method + " " + req.path;
        auto it = routes_.find(route_key);
        if (it != routes_.end()) {
            try {
                response = it->second(req);
            } catch (const std::exception& e) {
                response = HttpResponse::error(500, std::string("Internal Exception: ") + e.what());
            } catch (...) {
                response = HttpResponse::error(500, "Unknown fatal error processing request.");
            }
        } else {
            // Fallback to static files
            if (req.method == "GET" || req.method == "HEAD") {
                response = serve_static_file(req.path);
                if (req.method == "HEAD") response.body.clear();
            } else {
                response = HttpResponse::error(404, "Endpoint not found: " + req.path);
            }
        }
    }

    // Construct HTTP response payload with CORS
    std::ostringstream oss;
    oss << "HTTP/1.1 " << response.status_code << " " << response.status_text << "\r\n";
    oss << "Content-Type: " << response.content_type << "\r\n";
    oss << "Content-Length: " << response.body.length() << "\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "Access-Control-Allow-Methods: GET, POST, OPTIONS, PUT, DELETE\r\n";
    oss << "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With\r\n";
    oss << "Connection: close\r\n";
    for (const auto& [hk, hv] : response.extra_headers) {
        oss << hk << ": " << hv << "\r\n";
    }
    oss << "\r\n";
    oss << response.body;

    std::string resp_str = oss.str();
    send(client_sock, resp_str.c_str(), static_cast<int>(resp_str.length()), 0);
    closesocket(client_sock);
}

HttpResponse HttpServer::serve_static_file(const std::string& req_path) {
    std::string path = req_path;
    if (path == "/" || path.empty()) path = "/index.html";

    // Prevent directory traversal attacks
    if (path.find("..") != std::string::npos) {
        return HttpResponse::error(403, "Access Denied: Path traversal detected.");
    }

    std::vector<std::string> candidate_roots = {
        static_dir_,
        "../" + static_dir_,
        "src/frontend",
        "../src/frontend",
        "./frontend",
        "."
    };

    std::ifstream file;
    std::string resolved_path;
    for (const auto& root : candidate_roots) {
        std::string p = root + path;
        file.open(p, std::ios::in | std::ios::binary);
        if (file.is_open()) {
            resolved_path = p;
            break;
        }
        file.clear();
    }

    if (!file.is_open()) {
        // Fallback: check if looking for data/ file
        if (req_path.find("/data/") == 0) {
            std::vector<std::string> data_candidates = {
                req_path.substr(1),
                "../" + req_path.substr(1),
                "data/" + req_path.substr(6),
                "../data/" + req_path.substr(6)
            };
            for (const auto& dp : data_candidates) {
                std::ifstream data_file(dp, std::ios::in | std::ios::binary);
                if (data_file.is_open()) {
                    std::string content((std::istreambuf_iterator<char>(data_file)),
                                         std::istreambuf_iterator<char>());
                    return HttpResponse::text(content);
                }
            }
        }
        return HttpResponse::error(404, "File not found: " + path);
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    if (path.rfind(".html") != std::string::npos) return HttpResponse::html(content);
    if (path.rfind(".css") != std::string::npos) return HttpResponse::css(content);
    if (path.rfind(".js") != std::string::npos) return HttpResponse::js(content);
    if (path.rfind(".json") != std::string::npos) return HttpResponse::json(content);
    return HttpResponse::text(content);
}

void HttpServer::register_workbench_routes() {
    // 1. Health & System Specs API
    route("GET", "/api/status", [](const HttpRequest&) {
        auto specs = multicore::BatchSolver::detect_system();
        std::ostringstream oss;
        oss << "{"
            << "\"status\":\"ONLINE\","
            << "\"app_name\":\"Unified Engineering Workbench\","
            << "\"version\":\"1.0.0\","
            << "\"cpu_model\":\"" << specs.cpu_model << "\","
            << "\"logical_cores\":" << specs.logical_cores << ","
            << "\"ram_info\":\"" << specs.ram_info << "\","
            << "\"compiler\":\"" << specs.compiler << "\","
            << "\"opt_flags\":\"" << specs.optimization_flags << "\""
            << "}";
        return HttpResponse::json(oss.str());
    });

    // 2. 1D Thermal FEA Solve API
    route("POST", "/api/fea/solve", [](const HttpRequest& req) {
        fea::ThermalParams p;
        if (!req.body.empty()) {
            p.L = extract_json_double(req.body, "L", 1.0);
            p.A = extract_json_double(req.body, "A", 1e-4);
            p.k = extract_json_double(req.body, "k", 10.0);
            p.q = extract_json_double(req.body, "q", 10000.0);
            p.T0 = extract_json_double(req.body, "T0", 100.0);
            p.TL = extract_json_double(req.body, "TL", 20.0);
            p.num_elements = extract_json_int(req.body, "num_elements", 10);
        }

        try {
            fea::FEAResult res = fea::ThermalSolver1D::solve(p);
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(6);
            oss << "{"
                << "\"success\":true,"
                << "\"elements\":" << res.num_elements << ","
                << "\"nodes\":" << res.num_nodes << ","
                << "\"h\":" << res.element_length_h << ","
                << "\"compute_time_ms\":" << res.compute_time_ms << ","
                << "\"max_midpoint_error\":" << res.max_midpoint_error << ","
                << "\"l2_midpoint_error\":" << res.l2_midpoint_error << ",";

            // Node positions and temperatures
            oss << "\"nodes_x\":[";
            for (size_t i = 0; i < res.node_x.size(); ++i) {
                if (i > 0) oss << ",";
                oss << res.node_x[i];
            }
            oss << "],\"nodes_T_fe\":[";
            for (size_t i = 0; i < res.node_T_fe.size(); ++i) {
                if (i > 0) oss << ",";
                oss << res.node_T_fe[i];
            }
            oss << "],\"nodes_T_exact\":[";
            for (size_t i = 0; i < res.node_T_exact.size(); ++i) {
                if (i > 0) oss << ",";
                oss << res.node_T_exact[i];
            }
            oss << "],\"midpoints\":[";
            for (size_t i = 0; i < res.midpoints.size(); ++i) {
                const auto& m = res.midpoints[i];
                if (i > 0) oss << ",";
                oss << "{\"elem\":" << m.element_id << ",\"x\":" << m.x_mid 
                    << ",\"T_fe\":" << m.T_fe_interp << ",\"T_exact\":" << m.T_exact 
                    << ",\"error\":" << m.abs_error << "}";
            }
            oss << "]}";
            return HttpResponse::json(oss.str());
        } catch (const std::exception& e) {
            return HttpResponse::error(400, e.what());
        }
    });

    // 3. Thermal FEA Convergence Study API
    route("GET", "/api/fea/convergence", [](const HttpRequest&) {
        fea::ThermalParams p;
        auto cases = fea::ThermalSolver1D::run_convergence_study(p);
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6);
        oss << "{\"success\":true,\"cases\":[";
        for (size_t i = 0; i < cases.size(); ++i) {
            const auto& c = cases[i];
            if (i > 0) oss << ",";
            oss << "{\"elements\":" << c.num_elements 
                << ",\"h\":" << c.h 
                << ",\"max_midpoint_error\":" << c.max_midpoint_error 
                << ",\"l2_error\":" << c.l2_error 
                << ",\"convergence_rate\":" << c.convergence_rate << "}";
        }
        oss << "]}";
        return HttpResponse::json(oss.str());
    });

    // 4. CAD STL Parser API
    route("POST", "/api/parsers/stl", [](const HttpRequest& req) {
        parsers::STLModel model;
        if (req.body.find("solid") != std::string::npos) {
            model = parsers::STLParser::parse_string(req.body);
        } else {
            // Load default sample STL
            model = parsers::STLParser::parse_file("data/sample_geometry.stl");
        }

        if (!model.is_valid) {
            return HttpResponse::error(400, model.error_message);
        }
        return HttpResponse::json(parsers::STLParser::to_json(model));
    });

    // 5. ANSYS CSV Parser API
    route("POST", "/api/parsers/ansys", [](const HttpRequest& req) {
        parsers::ANSYSModel model;
        if (req.body.find("temperature") != std::string::npos || req.body.find("node_id") != std::string::npos) {
            model = parsers::ANSYSCSVParser::parse_string(req.body);
        } else {
            // Load default sample dataset
            model = parsers::ANSYSCSVParser::parse_file("data/ansys_thermal_results.csv");
        }

        if (!model.is_valid) {
            return HttpResponse::error(400, model.error_message);
        }
        return HttpResponse::json(parsers::ANSYSCSVParser::to_json(model));
    });

    // 6. Multicore Execution Benchmark API
    route("POST", "/api/benchmark/run", [](const HttpRequest& req) {
        multicore::BatchSolver::reset_cancellation();
        int cases = 500;
        int elements = 40;
        if (!req.body.empty()) {
            cases = extract_json_int(req.body, "cases", 500);
            elements = extract_json_int(req.body, "elements", 40);
        }
        auto res = multicore::BatchSolver::run_benchmarks(cases, elements);
        return HttpResponse::json(multicore::BatchSolver::to_json(res));
    });

    // 6b. Safe Non-blocking Cancellation API
    route("POST", "/api/cancel", [](const HttpRequest&) {
        multicore::BatchSolver::request_cancellation();
        return HttpResponse::json("{\"cancelled\":true,\"message\":\"Compute cancellation requested successfully.\"}");
    });

    // 7. Embedded Devices Telemetry & Capabilities API
    route("GET", "/api/embedded/devices", [](const HttpRequest&) {
        return HttpResponse::json(g_embedded_manager.get_all_devices_json());
    });

    // 8. Embedded Device Action API (Connect, Disconnect, Inject Fault, Recover)
    route("POST", "/api/embedded/action", [](const HttpRequest& req) {
        std::string device_id = extract_json_string(req.body, "device_id");
        std::string action = extract_json_string(req.body, "action");

        auto* dev = g_embedded_manager.get_adapter(device_id);
        if (!dev) {
            return HttpResponse::error(404, "Device ID not recognized: " + device_id);
        }

        bool success = false;
        std::string msg;
        if (action == "connect") {
            success = dev->connect();
            msg = "Device connected successfully.";
        } else if (action == "disconnect") {
            success = dev->disconnect();
            msg = "Device disconnected.";
        } else if (action == "inject_fault") {
            dev->inject_fault(embedded::FaultType::COMMUNICATION_TIMEOUT, "Simulated communication timeout (500 ms limit exceeded).");
            success = true;
            msg = "Timeout fault injected successfully.";
        } else if (action == "recover") {
            success = dev->recover();
            msg = success ? "Link recovered successfully." : "Recovery failed.";
        } else {
            return HttpResponse::error(400, "Unknown action: " + action);
        }

        std::ostringstream oss;
        oss << "{\"success\":" << (success ? "true" : "false") << ",\"message\":\"" << msg << "\"}";
        return HttpResponse::json(oss.str());
    });

    // 9. Embedded Architecture & Pinout Specifications API
    route("GET", "/api/embedded/specs", [](const HttpRequest&) {
        std::ostringstream oss;
        // Escape newlines and quotes for JSON payload
        auto escape_json = [](const std::string& input) {
            std::string out;
            for (char c : input) {
                if (c == '"') out += "\\\"";
                else if (c == '\\') out += "\\\\";
                else if (c == '\n') out += "\\n";
                else if (c == '\r') out += "\\r";
                else if (c == '\t') out += "\\t";
                else out += c;
            }
            return out;
        };

        oss << "{"
            << "\"mermaid\":\"" << escape_json(embedded::EmbeddedWorkbenchManager::get_block_diagram_mermaid()) << "\","
            << "\"table_markdown\":\"" << escape_json(embedded::EmbeddedWorkbenchManager::get_pin_voltage_table_markdown()) << "\","
            << "\"jtag_rationale\":\"" << escape_json(embedded::EmbeddedWorkbenchManager::get_jtag_rationale_explanation()) << "\""
            << "}";
        return HttpResponse::json(oss.str());
    });

    // 10. Backpropagation Training API
    route("POST", "/api/backprop/train", [](const HttpRequest& req) {
        double lr = 0.1;
        int steps = 100;
        if (!req.body.empty()) {
            lr = extract_json_double(req.body, "learning_rate", 0.1);
            steps = extract_json_int(req.body, "steps", 100);
        }
        auto res = backprop::BackpropEngine::run_training(1.0, 2.0, lr, steps);
        return HttpResponse::json(backprop::BackpropEngine::to_json(res));
    });
}

} // namespace workbench::server
