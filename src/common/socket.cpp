#include "common/socket.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <unistd.h>

#include "common/defines.h"

int server_socket_init(uint16_t port) {
    int rc;
    int server_sock_fd;
    struct sockaddr_in serv_addr{};

    std::cout << SERVER_MSG_PREFIX << "Initializing..." << std::endl;

    server_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock_fd < 0) {
        std::cerr << SERVER_MSG_PREFIX << "Error: creating listening socket." << std::endl;
        return STATUS_FAIL;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    /* setsockopt: reuse port for the server; server restart flow */
    int sock_optval = 1;
    setsockopt(server_sock_fd, SOL_SOCKET, SO_REUSEADDR, static_cast<const void *>(&sock_optval), sizeof(sock_optval));

    rc = bind(server_sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if (rc < 0) {
        std::cerr << SERVER_MSG_PREFIX << "Error calling bind()" << std::endl;
        close(server_sock_fd);
        return STATUS_FAIL;
    }

    rc = listen(server_sock_fd, SERVER_LISTEN_BACKLOG_SIZE);
    if (rc < 0) {
        std::cerr << SERVER_MSG_PREFIX << "Error calling listen()" << std::endl;
        close(server_sock_fd);
        return STATUS_FAIL;
    }

    std::cout << SERVER_MSG_PREFIX << "Initialization finished" << std::endl;
    return server_sock_fd;
}
