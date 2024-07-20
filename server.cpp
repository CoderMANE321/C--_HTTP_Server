#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <thread>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fstream>
#include <sstream>

void http_request(int client_fd, const std::string& dir) {
    char buf[1024];
    int bytes_received = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (bytes_received < 0) {
        std::cerr << "Failed to receive data\n";
        close(client_fd);
        return;
    }
    buf[bytes_received] = '\0';

    std::string request(buf);
    std::string ok_message = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 2\r\n\r\nOK";
    std::string file_message = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: 14\r\n\r\nHello, World!";
    std::string error_message = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 13\r\n\r\n404 Not Found";
    std::string agent_end = "/1.2.3";

    if (request.find("GET / ") == 0) {
        send(client_fd, ok_message.c_str(), ok_message.length(), 0);            
        std::cout << ok_message;
    } else if (request.find("POST /files/") == 0) {
        std::string fileName = request.substr(11, request.find(" HTTP/") - 11);
        size_t start = request.find("\r\n\r\n") + 4;
        size_t end = request.find(" ", start) + 65;
        std::string content = request.substr(start, end - start);
        std::ofstream ofs(dir + fileName, std::ios::binary);
        if (ofs.good()) {
            ofs << content;
            ofs.close();
            std::string response = "HTTP/1.1 201 Created\r\n\r\n";
            send(client_fd, response.c_str(), response.length(), 0);
        } else {
            send(client_fd, error_message.c_str(), error_message.length(), 0);
        }
    }else if (request.find("GET /user-agent") == 0) {
        size_t start = request.find("User-Agent:") + 12;
        size_t end = request.find("\r\n", start);
        std::string user_agent = request.substr(start, end - start);
        std::string concat = user_agent + agent_end;
        int cLength = (user_agent.length());
        std::cout << user_agent;
        std::cout << agent_end;
        std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(cLength) + "\r\n\r\n" + concat;      
        std::cout << concat;
        send(client_fd, response.c_str(), response.length(), 0);
    } else if (request.find("GET /files/") == 0) {
        std::string fileName = request.substr(11, request.find(" HTTP/") - 11);
        std::ifstream ifs(dir + fileName, std::ios::binary);
        if (ifs.good()) {
            std::ostringstream content;
            content << ifs.rdbuf();
            std::string file_content = content.str();
            std::string response = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " + std::to_string(file_content.length()) + "\r\n\r\n" + file_content;
            send(client_fd, response.c_str(), response.length(), 0);
        } else {
            send(client_fd, error_message.c_str(), error_message.length(), 0);
        }
    } else if (request.find("GET /echo/") == 0) {
        size_t start = request.find("/echo/") + 6;
        size_t end = request.find(" HTTP/1.1");
        std::string str = request.substr(start, end - start);
        size_t startOfG = request.find("Accept-Encoding:") + 17;
        size_t endOfG = request.find("\r\n", startOfG);
        std::string encoding = request.substr(startOfG, endOfG - startOfG);
        if (encoding.find("gzip") != std::string::npos) {
            std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Encoding: gzip\r\nContent-Length: " + std::to_string(str.length()) + "\r\n\r\n" + str;
            send(client_fd, response.c_str(), response.length(), 0);
        } else{
        std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(str.length()) + "\r\n\r\n" + str;
        send(client_fd, response.c_str(), response.length(), 0);
        }
    } else {
        send(client_fd, error_message.c_str(), error_message.length(), 0);
    }

    close(client_fd);
}

int main(int argc, char **argv) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    std::string dir;
    if (argc == 3 && strcmp(argv[1], "--directory") == 0) {
        dir = argv[2];
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create server socket\n";
        return 1;
    }

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        std::cerr << "setsockopt failed\n";
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(4221);
    if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
        std::cerr << "Failed to bind to port 4221\n";
        return 1;
    }

    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0) {
        std::cerr << "listen failed\n";
        return 1;
    }

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    std::cout << "Waiting for a client to connect...\n";

    while (true) {
        int client = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
        if (client < 0) {
            std::cerr << "Failed to accept client connection\n";
            continue;
        }
        std::cout << "Client connected\n";
        std::thread clientThread(http_request, client, dir);
        clientThread.detach();
    }

    close(server_fd);
    return 0;
}
