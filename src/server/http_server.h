#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include <functional>
#include <map>
#include <memory>
#include <atomic>
#include <thread>
#include <vector>

namespace workbench::server {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string query_string;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int status_code = 200;
    std::string status_text = "OK";
    std::string content_type = "application/json";
    std::map<std::string, std::string> extra_headers;
    std::string body;

    static HttpResponse json(const std::string& json_content, int code = 200);
    static HttpResponse html(const std::string& html_content, int code = 200);
    static HttpResponse css(const std::string& css_content, int code = 200);
    static HttpResponse js(const std::string& js_content, int code = 200);
    static HttpResponse text(const std::string& text_content, int code = 200);
    static HttpResponse error(int code, const std::string& message);
};

using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

class HttpServer {
public:
    HttpServer(const std::string& host = "127.0.0.1", int port = 8080, const std::string& static_dir = "src/frontend");
    ~HttpServer();

    // Route registration
    void route(const std::string& method, const std::string& path, HttpHandler handler);

    // Starts listening and serving requests asynchronously
    bool start();

    // Stops server and releases socket resources
    void stop();

    int get_port() const { return port_; }
    std::string get_host() const { return host_; }
    bool is_running() const { return running_.load(); }

    // Binds default workbench API routes connecting C++ Module 1 core to endpoints
    void register_workbench_routes();

private:
    void listen_loop();
    void handle_client(uintptr_t client_socket);
    HttpResponse serve_static_file(const std::string& req_path);

    std::string host_;
    int port_;
    std::string static_dir_;
    std::atomic<bool> running_{false};
    uintptr_t listen_socket_ = 0; // SOCKET representation
    std::unique_ptr<std::thread> listener_thread_;
    std::map<std::string, HttpHandler> routes_; // key: "METHOD /path"
};

} // namespace workbench::server

#endif // HTTP_SERVER_H
