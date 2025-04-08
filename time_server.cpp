#include <iostream>
#include <string>
#include <ctime>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <algorithm>
#include <thread>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

// 端口配置
const int PORT = 8080;
const int WS_PORT = 8081; // WebSocket 使用不同端口
const int BUFFER_SIZE = 1024;

// 获取当前时间的函数
std::string get_current_time() {
    time_t now = time(0);
    struct tm time_struct;
    char buf[80];
    time_struct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d %X", &time_struct);
    return buf;
}

// 处理普通 TCP 客户端的函数
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    int valread = read(client_socket, buffer, BUFFER_SIZE);
    
    std::string request(buffer);
    std::string response;
    
    if (request.find("GET_TIME") != std::string::npos) {
        response = "SERVER_TIME: " + get_current_time();
    } else {
        response = "INVALID_REQUEST";
    }
    
    send(client_socket, response.c_str(), response.length(), 0);
    close(client_socket);
}

// WebSocket 握手响应生成函数
std::string get_ws_handshake_response(const std::string& client_key) {
    const std::string kGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string accept_key_input = client_key + kGuid;

    // 计算 SHA-1 哈希
    unsigned char sha1_result[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(accept_key_input.c_str()), accept_key_input.length(), sha1_result);

    // Base64 编码
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, mem);

    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); // 不换行
    BIO_write(b64, sha1_result, SHA_DIGEST_LENGTH);
    BIO_flush(b64);

    BUF_MEM* buffer_ptr;
    BIO_get_mem_ptr(b64, &buffer_ptr);

    std::string accept_key(buffer_ptr->data, buffer_ptr->length);

    BIO_free_all(b64);

    // 返回握手响应头
    return "HTTP/1.1 101 Switching Protocols\r\n"
           "Upgrade: websocket\r\n"
           "Connection: Upgrade\r\n"
           "Sec-WebSocket-Accept: " + accept_key + "\r\n\r\n";
}

// 处理 WebSocket 客户端请求的函数
void handle_ws_client(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    int valread = read(client_socket, buffer, BUFFER_SIZE);
    
    // 检查是否是 WebSocket 握手请求
    std::string request(buffer);
    size_t key_pos = request.find("Sec-WebSocket-Key: ");
    if (key_pos != std::string::npos) {
        size_t start = key_pos + strlen("Sec-WebSocket-Key: ");
        size_t end = request.find("\r\n", start);
        std::string client_key = request.substr(start, end - start);

        std::string handshake_response = get_ws_handshake_response(client_key);
        send(client_socket, handshake_response.c_str(), handshake_response.length(), 0);

        std::cout << "完成握手，等待 WebSocket 消息...\n";
        
        // 后续通信逻辑：接收并回应消息
        while (true) {
            memset(buffer, 0, BUFFER_SIZE);
            valread = read(client_socket, buffer, BUFFER_SIZE);
            if (valread <= 0) break;

            // 解析 WebSocket 帧
        unsigned char* data = (unsigned char*)buffer;
        bool masked = (data[1] & 0x80) != 0;
        uint64_t payload_len = data[1] & 0x7F;
        size_t index = 2;

        if (payload_len == 126) {
            payload_len = (data[2] << 8) | data[3];
            index += 2;
        } else if (payload_len == 127) {
            payload_len = 0;
            for (int i = 0; i < 8; ++i)
                payload_len = (payload_len << 8) | data[index++];
        }

        uint32_t masking_key = 0;
        if (masked) {
            masking_key = (data[index] << 24) | (data[index+1] << 16) | (data[index+2] << 8) | data[index+3];
            index += 4;
        }

        // 提取并解掩码有效载荷
        std::string message;
        for (size_t i = 0; i < payload_len; ++i) {
            char c = data[index + i] ^ ((masking_key >> (8 * (3 - i % 4))) & 0xFF);
            message += c;
        }

        std::cout << "Received: " << message << std::endl;

            std::string response_msg;
            if (message == "GET_TIME") {
                response_msg = "SERVER_TIME: " + get_current_time();
            } else {
                response_msg = "INVALID_REQUEST";
            }

            // 创建 WebSocket 帧并发送
            std::vector<unsigned char> frame;
            frame.push_back(0x81); // FIN + text frame opcode
            frame.push_back(response_msg.size());
            frame.insert(frame.end(), response_msg.begin(), response_msg.end());
            std::string ws_frame(frame.begin(), frame.end());

            send(client_socket, ws_frame.c_str(), ws_frame.length(), 0);
        }
    } else {
        std::cerr << "未能提取 Sec-WebSocket-Key，握手失败\n";
        close(client_socket);
        return;
    }
    
    close(client_socket);
}

// WebSocket 服务器运行函数
void run_websocket_server() {
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
    address.sin_port = htons(WS_PORT);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    std::cout << "WebSocket server running at port " << WS_PORT << std::endl;
    
    while (true) {
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }
        
        handle_ws_client(client_socket);
    }
}

// HTTP 服务器运行函数
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
    address.sin_port = htons(8000);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    std::cout << "HTTP server is running on port 8000" << std::endl;
    
    while (true) {
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }
        
        char buffer[BUFFER_SIZE] = {0};
        read(client_socket, buffer, BUFFER_SIZE);
        
        // 读取 HTML 文件内容
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

int main() {
    // 启动 HTTP 服务器
    std::thread http_thread(run_http_server);
    http_thread.detach();
    
    // 启动 WebSocket 服务器
    std::thread ws_thread(run_websocket_server);
    ws_thread.detach();
    
    std::cout << "主线程等待连接，可扩展更多服务..." << std::endl;
    
    while (true) {
        // 主线程可以处理更多的任务，或保持主线程活跃
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
    
    return 0;
}

