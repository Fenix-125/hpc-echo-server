// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "echo_server_simple.h"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <csignal>

//for server 1
#include "common/io.h"
#include "common/defines.h"

#define SERVER_MSG_PREFIX "[server] "

static bool server_running_flag = false;
static int g_server_fd = -1;

static void server_terminate_handler(int signum) {
    if (!server_running_flag) {
        std::cout << SERVER_MSG_PREFIX << "Server force stop" << std::endl;
        exit(STATUS_FAIL);
    }
    server_running_flag = false;
    std::cout << SERVER_MSG_PREFIX << "Server stop command issued" << std::endl;
    if (-1 != g_server_fd) {
        shutdown(g_server_fd, SHUT_RDWR);
        close(g_server_fd);
    } else {
        std::cout << SERVER_MSG_PREFIX << "Server socket is not set" << std::endl;
    }
}


int echo_server_simple_main(short int port) {
    int server_sock_fd;
    int client_sock_fd;
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

    if (bind(server_sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << SERVER_MSG_PREFIX << "Error calling bind()" << std::endl;
        close(server_sock_fd);
        return EXIT_FAILURE;
    }

    if (listen(server_sock_fd, 1000) < 0) {
        std::cerr << SERVER_MSG_PREFIX << "Error calling listen()" << std::endl;
        close(server_sock_fd);
        return EXIT_FAILURE;
    }

    // setup signal for cleanup on termination
    g_server_fd = server_sock_fd;
    signal(SIGINT, server_terminate_handler);

    std::cout << SERVER_MSG_PREFIX << "Initialization finished" << std::endl;
    server_running_flag = true;

    // Client handle structures
    struct sockaddr_in client_addr{};
    socklen_t client_addr_len = sizeof(client_addr);
    std::string client_addr_str{};
    std::string msg_buffer{};
    int io_status;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    while (server_running_flag) {
        // Connect
        client_sock_fd = accept(server_sock_fd, (struct sockaddr *) &client_addr, &client_addr_len);
        if (client_sock_fd < 0) {
            if (server_running_flag) {
                std::cerr << SERVER_MSG_PREFIX << "Error: calling accept()" << std::endl;
            }
            continue;
        }
        client_addr_str = get_socket_addr_str(&client_addr, client_addr_len);
        std::cout << SERVER_MSG_PREFIX << "New connection from " << client_addr_str << std::endl;

        // Read
        io_status = read_msg(client_sock_fd, msg_buffer);
        if (STATUS_SUCCESS != io_status) {
            std::cerr << SERVER_MSG_PREFIX << "Error: failed to read from client[" << client_addr_str << ']'
                      << std::endl;
            close(client_sock_fd);
            std::cout << SERVER_MSG_PREFIX << "Connection closed for " << client_addr_str << std::endl;
            continue;
        }
        std::cout << SERVER_MSG_PREFIX << "Read from " << client_addr_str << "msg:" << msg_buffer << std::endl;

        // Write
        io_status = write_msg(client_sock_fd, msg_buffer);
        if (STATUS_SUCCESS != io_status) {
            std::cerr << SERVER_MSG_PREFIX << "Error: failed to write from client[" << client_addr_str << ']'
                      << std::endl;
            std::cout << SERVER_MSG_PREFIX << "Connection closed for " << client_addr_str << std::endl;
            close(client_sock_fd);
            continue;
        }

        // Disconnect
        close(client_sock_fd);
        std::cout << SERVER_MSG_PREFIX << "Connection closed for " << client_addr_str << std::endl;
    }
#pragma clang diagnostic pop

    close(server_sock_fd);
    std::cout << SERVER_MSG_PREFIX << "Server stopped" << std::endl;
    return STATUS_SUCCESS;
}
