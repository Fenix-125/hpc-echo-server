// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "echo_server_simple.h"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <csignal>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

//for server 1
#include "common/io.h"
#include "common/defines.h"

#define SERVER_MSG_PREFIX               "[server] "
#define SERVER_EXPECT_CONNECTIONS       (100)
#define GC_THRESHOLD                    (10)
#define SERVER_LISTEN_BACKLOG_SIZE      (100)
#define SERVER_FD_INDEX                 (0)
#define INFTIM                          (-1)
#define FD_POOL_DUMMY_FD                (-1)
#define FD_POOL_EMPTY_ADDRESS_STR       ("")


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
    } else {
        std::cout << SERVER_MSG_PREFIX << "Server socket is not set" << std::endl;
    }
}

static void close_client(size_t client_pool_index, std::vector<pollfd> &fd_pool,
                         std::vector<std::string> &fd_address_str, size_t &garbage_to_clean) {
    // Disconnect
    close(fd_pool[client_pool_index].fd);
    std::cout << SERVER_MSG_PREFIX << "Connection closed for " << fd_address_str[client_pool_index] << std::endl;
    fd_pool[client_pool_index].fd = FD_POOL_DUMMY_FD;
    fd_address_str[client_pool_index] = FD_POOL_EMPTY_ADDRESS_STR;
    garbage_to_clean++;
}

// Garbage collector
static int
gc_routine(size_t &garbage_to_clean, std::vector<pollfd> &fd_pool, std::vector<std::string> &fd_address_str) {
    int rc = STATUS_SUCCESS;
    size_t del_num_fd = 0;
    size_t del_num_str = 0;

    if (garbage_to_clean < GC_THRESHOLD) {
        return STATUS_SUCCESS;
    }

    fd_pool.erase(std::remove_if(fd_pool.begin(), fd_pool.end(),
                                 [&del_num_fd](const pollfd &elem) {
                                     if (elem.fd == FD_POOL_DUMMY_FD) {
                                         del_num_fd++;
                                     }
                                     return elem.fd == FD_POOL_DUMMY_FD;
                                 }), fd_pool.end());
    fd_address_str.erase(std::remove_if(fd_address_str.begin(), fd_address_str.end(),
                                        [&del_num_str](const std::string &elem) {
                                            if (elem.empty()) {
                                                del_num_str++;
                                            }
                                            return elem.empty();
                                        }), fd_address_str.end());

    if (del_num_fd == garbage_to_clean && del_num_str == garbage_to_clean) {
        std::cout << SERVER_MSG_PREFIX << "GC clean up " << garbage_to_clean << " elements" << std::endl;
        rc = STATUS_SUCCESS;
    } else {
        std::cerr << SERVER_MSG_PREFIX << "GC encountered a critical BUG" << garbage_to_clean << std::endl;
        rc = STATUS_FAIL;
    }
    garbage_to_clean = 0;
    return rc;
}

int echo_server_simple_main(short int port) {
    int rc;
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

    rc = bind(server_sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if (rc < 0) {
        std::cerr << SERVER_MSG_PREFIX << "Error calling bind()" << std::endl;
        close(server_sock_fd);
        return EXIT_FAILURE;
    }

    rc = listen(server_sock_fd, SERVER_LISTEN_BACKLOG_SIZE);
    if (rc < 0) {
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
    std::string msg_buffer{};
    int io_status;

    std::vector<pollfd> fd_pool{};
    std::vector<std::string> fd_address_str{};
    int trig_fds_count;
    size_t garbage_to_clean = 0;

    fd_pool.reserve(SERVER_EXPECT_CONNECTIONS);
    fd_address_str.reserve(SERVER_EXPECT_CONNECTIONS);

    fd_pool.emplace_back(pollfd{server_sock_fd, POLLIN | POLLERR | POLLHUP | POLLNVAL, 0});
    // fd_address_str expects not empty string
    fd_address_str.emplace_back(get_socket_addr_str(&serv_addr, sizeof(serv_addr)));

    while (server_running_flag) {
        trig_fds_count = poll(fd_pool.data(), fd_pool.size(), INFTIM);
        if (trig_fds_count < 0) {
            if (EINTR != errno) {
                continue; // while (server_running_flag)
            }
            if (server_running_flag) {
                std::cerr << SERVER_MSG_PREFIX << "Error: calling poll errno: " << strerror(errno) << std::endl;
            } else {
                // user stop signal issued
            }
            break; // while (server_running_flag)
        } else if (0 == trig_fds_count) {
            continue; // while (server_running_flag)
        }

        // Server socket fd event
        if (fd_pool[SERVER_FD_INDEX].revents) {
            trig_fds_count--;
            // Error handling
            if (POLLIN != fd_pool[SERVER_FD_INDEX].revents) {
                if (server_running_flag) {
                    std::cerr << SERVER_MSG_PREFIX << "Error: server socket fail" << std::endl;
                }
                std::cout << SERVER_MSG_PREFIX << "Server socket closed" << std::endl;
                break; // while (server_running_flag)
            }

            // Connect new client
            client_sock_fd = accept(fd_pool[SERVER_FD_INDEX].fd, (struct sockaddr *) &client_addr, &client_addr_len);
            if (client_sock_fd < 0) {
                std::cerr << SERVER_MSG_PREFIX << "Error: calling accept() errno: " << strerror(errno) << std::endl;
                break; // while (server_running_flag)
            }
            fd_pool.emplace_back(pollfd{client_sock_fd, POLLIN | POLLERR | POLLHUP | POLLNVAL, 0});
            // fd_address_str expects not empty string
            fd_address_str.emplace_back(get_socket_addr_str(&client_addr, client_addr_len));
            std::cout << SERVER_MSG_PREFIX << "New connection from " << fd_address_str.back() << std::endl;
        }

        // Client requests handling
        for (size_t index = 1; index < fd_pool.size() && trig_fds_count > 0; ++index) {
            if (0 == fd_pool[index].revents) {
                continue; // for
            }
            trig_fds_count--;

            // Error handling
            if (POLLIN != fd_pool[index].revents) {
                close_client(index, fd_pool, fd_address_str, garbage_to_clean);
                continue; // for
            }
            // Read
            io_status = read_msg(fd_pool[index].fd, msg_buffer);
            if (STATUS_SUCCESS != io_status) {
                std::cerr << SERVER_MSG_PREFIX << "Error: failed to read from " << fd_address_str[index]
                          << std::endl;
                close_client(index, fd_pool, fd_address_str, garbage_to_clean);
                continue; // for
            }
            // Handling EOF message
            if (msg_buffer.empty()) {
                close_client(index, fd_pool, fd_address_str, garbage_to_clean);
                continue; // for
            }

            std::cout << SERVER_MSG_PREFIX << "Read from " << fd_address_str[index] << " msg:\n" << msg_buffer
                      << std::endl;

            // Write
            io_status = write_msg(fd_pool[index].fd, msg_buffer);
            if (STATUS_SUCCESS != io_status) {
                std::cerr << SERVER_MSG_PREFIX << "Error: failed to write from " << fd_address_str[index]
                          << std::endl;
                close_client(index, fd_pool, fd_address_str, garbage_to_clean);
            }
        } // for (size_t index = 1; index < fd_pool.size() && trig_fds_count > 0; ++index)


        // Cleanup garbage in DB
        if (STATUS_SUCCESS != gc_routine(garbage_to_clean, fd_pool, fd_address_str)) {
            std::cerr << SERVER_MSG_PREFIX << "Error: critical bug during garbage collector routine" << std::endl;
            break; // while (server_running_flag)
        }

    } // while (server_running_flag)

    close(server_sock_fd);
    std::cout << SERVER_MSG_PREFIX << "Server stopped" << std::endl;
    return STATUS_SUCCESS;
}
