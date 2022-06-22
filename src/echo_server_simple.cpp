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

#include "common/io.h"
#include "common/defines.h"
#include "common/socket.h"
#include "common/logging.h"


#define GC_THRESHOLD                    (300)
#define SERVER_FD_INDEX                 (0)
#define INFTIM                          (-1)
#define FD_POOL_DUMMY_FD                (-1)
#define FD_POOL_EMPTY_ADDRESS_STR       ("")


namespace server_simple {
    bool g_running_flag = false;
    size_t g_garbage_count = 0;
    int g_server_fd = -1;
    std::vector<pollfd> g_db_fd_pool{};
    std::vector<std::string> g_db_addr_str{};

    int server_init(uint16_t port);

    void server_terminate_handler(int signum);

    // Garbage Collector
    int gc_routine();

    namespace client {
        int connect_client();

        void close_client(size_t client_pool_index);

        int get_request_client(size_t client_pool_index, std::string &msg_buffer);

        void send_response_client(size_t client_pool_index, const std::string &msg_buffer);
    }
}

int echo_server_simple_main(uint16_t port) {
    int trig_fds_count;
    std::string msg_buffer{};

    using namespace server_simple;

    if (STATUS_SUCCESS != server_init(port)) {
        return STATUS_FAIL;
    }

    g_running_flag = true;
    while (g_running_flag) {
        trig_fds_count = poll(g_db_fd_pool.data(), g_db_fd_pool.size(), INFTIM);
        if (trig_fds_count < 0) {
            if (EINTR == errno) {
                continue; // while (g_running_flag)
            }
            if (g_running_flag) {
                PLOG(ERROR) << "Error calling poll";
            } else {
                // user stop signal issued
            }
            break; // while (g_running_flag)
        } else if (0 == trig_fds_count) {
            continue; // while (g_running_flag)
        }

        // Server socket fd event
        if (g_db_fd_pool[SERVER_FD_INDEX].revents) {
            trig_fds_count--;
            // Error handling
            if (POLLIN != g_db_fd_pool[SERVER_FD_INDEX].revents) {
                if (g_running_flag) {
                    LOG(ERROR) << "Error server socket fail";
                }
                LOG(INFO) << "Server socket closed";
                break; // while (g_running_flag)
            }
            if (STATUS_SUCCESS != client::connect_client()) {
                break;  // while (g_running_flag)
            }
        }

        // Client requests handling
        for (size_t index = 1; index < g_db_fd_pool.size() && trig_fds_count > 0; ++index) {
            if (0 == g_db_fd_pool[index].revents) {
                continue; // for
            }
            trig_fds_count--;

            // Error handling
            if (POLLIN != g_db_fd_pool[index].revents) {
                client::close_client(index);
                continue; // for
            }

            if (STATUS_SUCCESS != client::get_request_client(index, msg_buffer)) {
                continue; // for
            }
            DLOG(INFO) << "Read from " << g_db_addr_str[index] << " msg:\n" << msg_buffer;
            client::send_response_client(index, msg_buffer);
        } // for (size_t index = 1; index < g_db_fd_pool.size() && trig_fds_count > 0; ++index)

        // Cleanup garbage in DB
        if (STATUS_SUCCESS != gc_routine()) {
            LOG(ERROR) << "Error critical bug during garbage collector routine";
            break; // while (g_running_flag)
        }
    } // while (g_running_flag)

    close(g_server_fd);
    LOG(INFO) << "Server stopped";
    return STATUS_SUCCESS;
}

int server_simple::server_init(uint16_t port) {
    g_server_fd = server_socket_init(port);
    if (g_server_fd < 0) {
        return STATUS_FAIL;
    }

    g_db_fd_pool.reserve(SERVER_EXPECT_CONNECTIONS);
    g_db_addr_str.reserve(SERVER_EXPECT_CONNECTIONS);

    g_db_fd_pool.emplace_back(pollfd{g_server_fd, POLLIN | POLLERR | POLLHUP | POLLNVAL, 0});
    // g_db_addr_str expects not empty string
    g_db_addr_str.emplace_back("server_addr");

    signal(SIGINT, server_terminate_handler);

    return STATUS_SUCCESS;
}

void server_simple::server_terminate_handler(int signum) {
    if (!g_running_flag) {
        close(g_server_fd);
        LOG(WARNING) << "Server force stop";
        logging_deinit();
        exit(STATUS_FAIL);
    }
    g_running_flag = false;
    LOG(INFO) << "Server stop command issued";
    if (-1 != g_server_fd) {
        shutdown(g_server_fd, SHUT_RDWR);
    } else {
        LOG(WARNING) << "Server socket is not set";
    }
}

int server_simple::gc_routine() {
    int rc = STATUS_SUCCESS;
    size_t del_num_fd = 0;
    size_t del_num_str = 0;

    if (g_garbage_count < GC_THRESHOLD) {
        return STATUS_SUCCESS;
    }

    g_db_fd_pool.erase(std::remove_if(g_db_fd_pool.begin(), g_db_fd_pool.end(),
                                      [&del_num_fd](const pollfd &elem) {
                                          if (elem.fd == FD_POOL_DUMMY_FD) {
                                              del_num_fd++;
                                          }
                                          return elem.fd == FD_POOL_DUMMY_FD;
                                      }), g_db_fd_pool.end());
    g_db_addr_str.erase(std::remove_if(g_db_addr_str.begin(), g_db_addr_str.end(),
                                       [&del_num_str](const std::string &elem) {
                                           if (elem.empty()) {
                                               del_num_str++;
                                           }
                                           return elem.empty();
                                       }), g_db_addr_str.end());

    if (del_num_fd == g_garbage_count && del_num_str == g_garbage_count) {
        LOG(INFO) << "GC clean up " << g_garbage_count << " elements";
        rc = STATUS_SUCCESS;
    } else {
        LOG(ERROR) << "GC encountered a critical BUG" << g_garbage_count;
        rc = STATUS_FAIL;
    }
    g_garbage_count = 0;
    return rc;
}

int server_simple::client::connect_client() {
    struct sockaddr_in client_addr{};
    socklen_t client_addr_len = sizeof(client_addr);
    int client_sock_fd;

    client_sock_fd = accept(g_server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
    if (client_sock_fd < 0) {
        PLOG(ERROR) << "Error calling accept()";
        return STATUS_FAIL;
    }
    g_db_fd_pool.emplace_back(pollfd{client_sock_fd, POLLIN | POLLERR | POLLHUP | POLLNVAL, 0});
    // g_db_addr_str expects not empty string
    g_db_addr_str.emplace_back(get_socket_addr_str(&client_addr, client_addr_len));
    DLOG(INFO) << "New connection from " << g_db_addr_str.back();
    return STATUS_SUCCESS;
}

void server_simple::client::close_client(size_t client_pool_index) {
    // Disconnect
    close(g_db_fd_pool[client_pool_index].fd);
    DLOG(INFO) << "Connection closed for " << g_db_addr_str[client_pool_index];
    g_db_fd_pool[client_pool_index].fd = FD_POOL_DUMMY_FD;
    g_db_addr_str[client_pool_index] = FD_POOL_EMPTY_ADDRESS_STR;
    g_garbage_count++;
}

int server_simple::client::get_request_client(size_t client_pool_index, std::string &msg_buffer) {
    int io_status = read_msg(g_db_fd_pool[client_pool_index].fd, msg_buffer);
    if (STATUS_SUCCESS != io_status) {
        LOG(WARNING) << "Error failed to read from " << g_db_addr_str[client_pool_index];
        client::close_client(client_pool_index);
        return STATUS_FAIL;
    }
    // Handling EOF message
    if (msg_buffer.empty()) {
        client::close_client(client_pool_index);
        return STATUS_FAIL;
    }
    return STATUS_SUCCESS;
}

void server_simple::client::send_response_client(size_t client_pool_index, const std::string &msg_buffer) {
    int io_status = write_msg(g_db_fd_pool[client_pool_index].fd, msg_buffer);
    if (STATUS_SUCCESS != io_status) {
        LOG(WARNING) << "Error failed to write from " << g_db_addr_str[client_pool_index];
        client::close_client(client_pool_index);
    }
}
