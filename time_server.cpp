#include <iostream>
#include <string>
#include <ctime>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include "spark_api.h"

// 端口配置
const int HTTP_PORT = 8080;
const int WS_PORT = 8081;
const int BUFFER_SIZE = 1024;

typedef websocketpp::server<websocketpp::config::asio> wsserver;
wsserver ws_server;

// 获取当前时间
std::string get_current_time() {
    time_t now = time(0);
    struct tm time_struct;
    char buf[80];
    time_struct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d %X", &time_struct);
    return buf;
}

// HTTP服务器实现
void run_http_server() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(HTTP_PORT);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    std::cout << "HTTP server running on port " << HTTP_PORT << std::endl;
    
    while (true) {
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }
        
        char buffer[BUFFER_SIZE] = {0};
        read(client_socket, buffer, BUFFER_SIZE);
        
        FILE* html_file = fopen("index.html", "r");
        if (!html_file) {
            perror("fopen");
            const char* not_found = "HTTP/1.1 404 Not Found\r\n\r\nFile not found";
            send(client_socket, not_found, strlen(not_found), 0);
            close(client_socket);
            continue;
        }
        
        fseek(html_file, 0, SEEK_END);
        long file_size = ftell(html_file);
        rewind(html_file);
        
        char* html_content = new char[file_size + 1];
        fread(html_content, 1, file_size, html_file);
        html_content[file_size] = '\0';
        fclose(html_file);
        
        std::string response = "HTTP/1.1 200 OK\r\n"
                             "Content-Type: text/html\r\n"
                             "Connection: close\r\n"
                             "\r\n" + std::string(html_content);
        
        send(client_socket, response.c_str(), response.length(), 0);
        delete[] html_content;
        close(client_socket);
    }
}

// WebSocket消息处理
void on_message(wsserver* s, websocketpp::connection_hdl hdl, wsserver::message_ptr msg) {
    std::string payload = msg->get_payload();
    std::string response;

    if (payload == "GET_TIME") {
        response = "SERVER_TIME: " + get_current_time();
    } else {
        response = ask_spark_api(payload);
    }

    try {
        s->send(hdl, response, msg->get_opcode());
    } catch (const websocketpp::exception& e) {
        std::cerr << "Send failed: " << e.what() << std::endl;
    }
}

// WebSocket服务器实现
void run_websocket_server() {
    try {
        ws_server.set_reuse_addr(true);
        ws_server.init_asio();
        
        ws_server.set_message_handler(std::bind(&on_message, &ws_server, 
            std::placeholders::_1, std::placeholders::_2));
        
        // 可选：关闭详细日志
        ws_server.clear_access_channels(websocketpp::log::alevel::all);
        
        ws_server.listen(WS_PORT);
        ws_server.start_accept();
        
        std::cout << "WebSocket server running on port " << WS_PORT << std::endl;
        ws_server.run();
    } catch (const websocketpp::exception& e) {
        std::cerr << "WebSocket server error: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown error" << std::endl;
    }
}

int main() {
    // 启动HTTP服务器
    std::thread http_thread(run_http_server);
    http_thread.detach();

    // 启动WebSocket服务器
    std::thread ws_thread(run_websocket_server);
    ws_thread.detach();

    std::cout << "All services started..." << std::endl;
    while (true) {
        std::this_thread::sleep_for(std::chrono::hours(1));
    }
    return 0;
}
