#include "common/socket.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <unistd.h>
#include <sys/resource.h>

#include "common/defines.h"
#include "common/logging.h"

size_t g_socket_num_limit = 0;


int server_socket_init(uint16_t port) {
    int rc;
    int server_sock_fd;
    struct sockaddr_in serv_addr{};

    LOG(INFO) << "Initializing...";

    server_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock_fd < 0) {
        PLOG(FATAL) << "Error creating listening socket.";
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
        PLOG(FATAL) << "Error calling bind()";
        close(server_sock_fd);
        return STATUS_FAIL;
    }

    rc = listen(server_sock_fd, SERVER_LISTEN_BACKLOG_SIZE);
    if (rc < 0) {
        PLOG(FATAL) << "Error calling listen()";
        close(server_sock_fd);
        return STATUS_FAIL;
    }

    LOG(INFO) << "Initialization finished";
    return server_sock_fd;
}

void sock_num_set_max_limit() {
    rlimit limit{};

    if (STATUS_SUCCESS != getrlimit(RLIMIT_NOFILE, &limit)) {
        PLOG(ERROR) << "syscall getrlimit failed";
        return;
    }
    g_socket_num_limit = static_cast<size_t>(limit.rlim_cur);
    limit.rlim_cur = limit.rlim_max;

    if (STATUS_SUCCESS != setrlimit(RLIMIT_NOFILE, &limit)) {
        LOG(INFO) << "Set server max connections num: " << g_socket_num_limit;
        PLOG(ERROR) << "syscall getrlimit failed";
        return;
    }
    g_socket_num_limit = static_cast<size_t>(limit.rlim_cur);
    LOG(INFO) << "Set server max connections num: " << g_socket_num_limit;
}
