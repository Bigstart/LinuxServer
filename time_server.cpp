#include <iostream>
#include <string>
#include <ctime>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <vector>
#include <mutex>
#include <map>
#include <sstream>
#include "spark_api.h"

const int HTTP_PORT = 8080;
const int BUFFER_SIZE = 1024;

std::mutex msg_mutex;
std::map<long, std::string> message_store;
long last_timestamp = 0;


// 转义字符串中的换行符、回车符和双引号
std::string escape_json_string(const std::string &input) {
    std::string escaped = input;

    // 转义双引号
    size_t pos = 0;
    while ((pos = escaped.find("\"", pos)) != std::string::npos) {
        escaped.replace(pos, 1, "\\\"");
        pos += 2;  // 跳过已转义的字符
    }

    // 转义换行符
    size_t pos_n = 0;
    while ((pos_n = escaped.find("\n", pos_n)) != std::string::npos) {
        escaped.replace(pos_n, 1, "\\n");
        pos_n += 2;
    }

    // 转义回车符
    size_t pos_r = 0;
    while ((pos_r = escaped.find("\r", pos_r)) != std::string::npos) {
        escaped.replace(pos_r, 1, "\\r");
        pos_r += 2;
    }

    return escaped;
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
    
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(HTTP_PORT);
    
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 3);
    
    std::cout << "HTTP server running on port " << HTTP_PORT << std::endl;
    
    while (true) {
        client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        
        char buffer[BUFFER_SIZE] = {0};
        read(client_socket, buffer, BUFFER_SIZE);
        
        // 处理请求
        std::string request(buffer);
        std::string response;
        
        if (request.find("GET / ") != std::string::npos) {
            // 返回HTML页面
            FILE* html_file = fopen("index.html", "r");
            if (html_file) {
                fseek(html_file, 0, SEEK_END);
                long file_size = ftell(html_file);
                rewind(html_file);
                char* html_content = new char[file_size + 1];
                fread(html_content, 1, file_size, html_file);
                fclose(html_file);
                
                response = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html\r\n"
                           "Connection: close\r\n\r\n" + 
                           std::string(html_content);
                delete[] html_content;
            } else {
                response = "HTTP/1.1 404 Not Found\r\n\r\nFile not found";
            }
        }
        else if (request.find("POST /send") != std::string::npos) {
            // 处理消息发送
            std::string msg(request.begin() + request.find("\r\n\r\n") + 4, request.end());
            
            std::cout << "msg: " << msg << std::endl;
            
            std::string api_response = ask_spark_api(msg);
            
            std::cout << "API Response: " << api_response << std::endl;
            
            std::lock_guard<std::mutex> lock(msg_mutex);
            message_store[++last_timestamp] = api_response;
            
            response = "HTTP/1.1 200 OK\r\n\r\n" + api_response;
        }else {
            response = "HTTP/1.1 404 Not Found\r\n\r\nInvalid request";
        }
        
        send(client_socket, response.c_str(), response.length(), 0);
        close(client_socket);
    }
}

int main() {
    std::thread http_thread(run_http_server);
    http_thread.detach();
    
    std::cout << "All services started..." << std::endl;
    while (true) {
        std::this_thread::sleep_for(std::chrono::hours(1));
    }
    return 0;
}
