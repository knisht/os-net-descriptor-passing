#include "rwutils.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <string.h>
#include <string>
#include <sys/epoll.h>
#include <sys/un.h>
#include <unistd.h>
#include <unordered_map>

struct fd_wrapper {
    int fd;
    explicit fd_wrapper(int fd) : fd(fd) {}

    operator int() { return fd; }

    fd_wrapper(fd_wrapper const &) = delete;

    ~fd_wrapper()
    {
        if (fd != -1 && close(fd) == -1 && errno != EBADF) {
            std::cerr << "close failed: " << strerror(errno) << std::endl;
        }
    }
};

const char address[] = "hw7_echo_unix";

[[noreturn]] void run()
{
    fd_wrapper server_socket(socket(AF_UNIX, SOCK_SEQPACKET, 0));
    if (server_socket == -1) {
        throw std::runtime_error("Cannot create socket");
    }
    struct sockaddr_un server_address;
    server_address.sun_family = AF_UNIX;
    server_address.sun_path[0] = '\0';
    memset(server_address.sun_path, 0, 108);
    memcpy(server_address.sun_path + 1, address, sizeof(address));
    if (bind(server_socket, reinterpret_cast<sockaddr *>(&server_address),
             sizeof(sockaddr_un)) == -1) {
        throw std::runtime_error("Cannot bind");
    }

    if (listen(server_socket, 20) == -1) {
        throw std::runtime_error("Cannot listen");
    }
    while (true) {
        fd_wrapper client_socket{accept(server_socket, nullptr, nullptr)};
        if (client_socket == -1) {
            std::cerr << "Could not connect to client: " << strerror(errno)
                      << std::endl;
            continue;
        }
        int readpipefd[2];
        int writepipefd[2];
        if (pipe(readpipefd) == -1) {
            std::cerr << "Failed to create pipes: " << strerror(errno)
                      << std::endl;
            continue;
        }
        fd_wrapper read_fd{readpipefd[0]};
        fd_wrapper temporary{readpipefd[1]};
        if (pipe(writepipefd) == -1) {
            std::cerr << "Failed to create pipes: " << strerror(errno)
                      << std::endl;
            continue;
        }
        fd_wrapper write_fd{writepipefd[1]};
        fd_wrapper temporary2{writepipefd[0]};
        struct msghdr msg;
        struct iovec iov[1];
        char buf[CMSG_SPACE(sizeof(int[2]))], c;
        c = '*';
        iov[0].iov_base = &c;
        iov[0].iov_len = sizeof(c);
        memset(buf, 0x0b, sizeof(buf));
        struct cmsghdr *cmsghdr = reinterpret_cast<struct cmsghdr *>(buf);
        cmsghdr->cmsg_len = CMSG_LEN(sizeof(int[2]));
        cmsghdr->cmsg_level = SOL_SOCKET;
        cmsghdr->cmsg_type = SCM_RIGHTS;
        msg.msg_name = nullptr;
        msg.msg_namelen = 0;
        msg.msg_iov = iov;
        msg.msg_iovlen = sizeof(iov) / sizeof(iov[0]);
        msg.msg_control = cmsghdr;
        msg.msg_controllen = CMSG_LEN(sizeof(int[2]));
        msg.msg_flags = 0;
        int *data = reinterpret_cast<int *>(CMSG_DATA(cmsghdr));
        data[0] = readpipefd[1];
        data[1] = writepipefd[0];
        if (sendmsg(client_socket, &msg, 0) == -1) {
            std::cerr << "Faild to send descriptors" << strerror(errno)
                      << std::endl;
        }
        close(temporary);
        close(temporary2);
        try {
            std::string received = read_message(read_fd);
            std::cout << "[INFO] Received: " << received << std::endl;
            write_message(write_fd, received);
        } catch (std::runtime_error &e) {
            std::cout << e.what() << std::endl;
        }
    }
}

static const std::string greeting =
    R"BLOCK(
Echo server based on unix sockets.
Just resends the message back.
Uses abstract socket at address "hw7_echo_unix"

)BLOCK";

int main(int argc, char *argv[])
{
    std::cout << greeting << std::endl;
    try {
        run();
    } catch (const std::exception &e) {
        std::cerr << e.what();
        if (errno != 0) {
            std::cerr << ": " << strerror(errno);
        }
        std::cerr << std::endl;
    }
}
