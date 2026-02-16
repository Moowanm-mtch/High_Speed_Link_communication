#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <server_ip> <port>\n";
        return 1;
    }

    const std::string server_ip = argv[1];
    const int port = std::stoi(argv[2]);

    if (port <= 0 || port > 65535) {
        std::cerr << "Invalid port: " << port << '\n';
        return 1;
    }

    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
        std::cerr << "Socket creation failed: " << std::strerror(errno) << '\n';
        return 1;
    }

    sockaddr_in server_addr {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(static_cast<uint16_t>(port));

    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid server IP address: " << server_ip << '\n';
        close(client_fd);
        return 1;
    }

    std::cout << "Connecting to " << server_ip << ":" << port << " ...\n";
    if (connect(client_fd, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        std::cerr << "Connect failed: " << std::strerror(errno) << '\n';
        close(client_fd);
        return 1;
    }
    std::cout << "Connected. Type a message and press Enter.\n";
    std::cout << "Type 'exit' to close the client.\n";

    std::string input;
    char buffer[1024];

    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, input)) {
            break;
        }

        if (input == "exit") {
            break;
        }

        input.push_back('\n');

        const ssize_t sent = send(client_fd, input.c_str(), input.size(), 0);
        if (sent < 0) {
            std::cerr << "Send failed: " << std::strerror(errno) << '\n';
            break;
        }

        const ssize_t received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (received < 0) {
            std::cerr << "Receive failed: " << std::strerror(errno) << '\n';
            break;
        }
        if (received == 0) {
            std::cout << "Server closed the connection.\n";
            break;
        }

        buffer[received] = '\0';
        std::cout << "Server: " << buffer;
    }

    close(client_fd);
    std::cout << "Client disconnected.\n";
    return 0;
}