#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace {

constexpr int kBacklog = 10;
constexpr size_t kBufferSize = 1024;

class FileDescriptor {
public:
    explicit FileDescriptor(int fd = -1) : fd_(fd) {}
    ~FileDescriptor() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

    FileDescriptor(FileDescriptor&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

    FileDescriptor& operator=(FileDescriptor&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0) {
                close(fd_);
            }
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    int get() const { return fd_; }

private:
    int fd_;
};

bool parse_port(const char* raw_port, int* port_out) {
    if (raw_port == nullptr || port_out == nullptr) {
        return false;
    }

    char* end_ptr = nullptr;
    errno = 0;
    const long parsed = std::strtol(raw_port, &end_ptr, 10);

    if (errno != 0 || end_ptr == raw_port || *end_ptr != '\0') {
        return false;
    }
    if (parsed <= 0 || parsed > 65535) {
        return false;
    }

    *port_out = static_cast<int>(parsed);
    return true;
}

bool create_listening_socket(int port, FileDescriptor* server_socket_out) {
    if (server_socket_out == nullptr) {
        return false;
    }

    FileDescriptor server_fd(socket(AF_INET, SOCK_STREAM, 0));
    if (server_fd.get() < 0) {
        std::cerr << "Socket creation failed: " << std::strerror(errno) << '\n';
        return false;
    }

    const int opt = 1;
    if (setsockopt(server_fd.get(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "setsockopt(SO_REUSEADDR) failed: " << std::strerror(errno) << '\n';
        return false;
    }

    sockaddr_in server_addr {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(server_fd.get(), reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        std::cerr << "Bind failed: " << std::strerror(errno) << '\n';
        return false;
    }

    if (listen(server_fd.get(), kBacklog) < 0) {
        std::cerr << "Listen failed: " << std::strerror(errno) << '\n';
        return false;
    }

    *server_socket_out = std::move(server_fd);
    return true;
}

void handle_client(int client_fd, const std::string& client_ip, uint16_t client_port) {
    std::cout << "Client connected: " << client_ip << ':' << client_port << '\n';

    char buffer[kBufferSize];

    while (true) {
        const ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received < 0) {
            std::cerr << "Receive failed: " << std::strerror(errno) << '\n';
            break;
        }
        if (bytes_received == 0) {
            std::cout << "Client disconnected: " << client_ip << ':' << client_port << '\n';
            break;
        }

        buffer[bytes_received] = '\0';
        std::string message(buffer);
        std::cout << "Client says: " << message;

        if (message == "exit\n") {
            const std::string goodbye = "Goodbye.\n";
            if (send(client_fd, goodbye.c_str(), goodbye.size(), 0) < 0) {
                std::cerr << "Send failed: " << std::strerror(errno) << '\n';
            }
            break;
        }

        const std::string response = "Echo: " + message;
        if (send(client_fd, response.c_str(), response.size(), 0) < 0) {
            std::cerr << "Send failed: " << std::strerror(errno) << '\n';
            break;
        }
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    std::signal(SIGPIPE, SIG_IGN);

    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }

    int port = 0;
    if (!parse_port(argv[1], &port)) {
        std::cerr << "Invalid port: " << argv[1] << '\n';
        return 1;
    }

    FileDescriptor server_socket;
    if (!create_listening_socket(port, &server_socket)) {
        return 1;
    }

    std::cout << "Server listening on port " << port << '\n';

    while (true) {
        sockaddr_in client_addr {};
        socklen_t client_len = sizeof(client_addr);

        const int raw_client_fd =
            accept(server_socket.get(), reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (raw_client_fd < 0) {
            std::cerr << "Accept failed: " << std::strerror(errno) << '\n';
            continue;
        }

        FileDescriptor client_fd(raw_client_fd);
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        const uint16_t client_port = ntohs(client_addr.sin_port);

        handle_client(client_fd.get(), client_ip, client_port);
    }
}
