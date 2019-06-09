#include "rwutils.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

struct fd_wrapper {
    int fd;
    explicit fd_wrapper(int fd) : fd(fd) {}

    operator int() { return fd; }

    fd_wrapper(fd_wrapper const &) = delete;

    ~fd_wrapper()
    {
        if (fd != -1 && close(fd) == -1) {
            std::cerr << "close failed: " << strerror(errno) << std::endl;
        }
    }
};

const char address[] = "hw7_echo_unix";

void run()
{
    struct sockaddr_un server;
    fd_wrapper socket_fd(socket(AF_UNIX, SOCK_SEQPACKET, 0));
    if (socket_fd == -1) {
        throw std::runtime_error("Cannot create socket");
    }

    server.sun_family = AF_UNIX;
    server.sun_path[0] = '\0';
    memset(server.sun_path, 0, 108);
    memcpy(server.sun_path + 1, address, sizeof(address));
    if (connect(socket_fd, reinterpret_cast<sockaddr *>(&server),
                sizeof(server)) == -1) {
        throw std::runtime_error("Cannot connect to server");
    }

    struct msghdr msg;
    struct iovec iov[1];
    char cmsg_buffer[CMSG_SPACE(sizeof(int[2]))], c;
    iov[0].iov_base = &c;
    iov[0].iov_len = sizeof(c);
    memset(cmsg_buffer, 0x0d, sizeof(cmsg_buffer));
    struct cmsghdr *cmsghdr = reinterpret_cast<struct cmsghdr *>(cmsg_buffer);
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

    if (recvmsg(socket_fd, &msg, 0) == -1) {
        throw std::runtime_error("Cannot retrieve file descriptors");
    }

    int *fds = reinterpret_cast<int *>(CMSG_DATA(cmsghdr));
    fd_wrapper write_fd{fds[0]};
    fd_wrapper read_fd{fds[1]};

    std::string query;
    getline(std::cin, query);
    write_message(write_fd, query);
    std::string received = read_message(read_fd);
    std::cout << "Server replied: " << received << std::endl;
}

static const std::string greeting =
    R"BLOCK(
Echo client based on unix sockets.
Expects server listening an abstarct socket at address "hw7_echo_unix"
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
