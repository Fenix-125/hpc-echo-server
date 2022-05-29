#include "echo_server_simple_threaded.h"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <csignal>
#include <string>
#include <vector>
#include <thread>

#include "common/io.h"
#include "common/defines.h"
#include "common/socket.h"
#include "common/logging.h"


namespace server_simple_threaded {
    bool g_running_flag = false;
    int g_server_fd = -1;

    int server_init(uint16_t port);

    void server_terminate_handler(int signum);

    namespace client {
        void client_handler(int fd, std::string name);

        int connect_client(int &fd, std::string &name);

        void close_client(int fd, const std::string &name);

        int get_request_client(int fd, const std::string &name, std::string &msg_buffer);

        int send_response_client(int fd, const std::string &name, const std::string &msg_buffer);
    }
}

int echo_server_simple_threaded_main(uint16_t port) {
    int client_fd;
    std::string client_name{};
    std::string msg_buffer{};
    std::vector<std::thread> thread_db{};

    using namespace server_simple_threaded;

    thread_db.reserve(SERVER_EXPECT_CONNECTIONS);

    if (STATUS_SUCCESS != server_init(port)) {
        return STATUS_FAIL;
    }

    g_running_flag = true;
    while (g_running_flag) {
        if (STATUS_SUCCESS != client::connect_client(client_fd, client_name)) {
            break;  // while (g_running_flag)
        }
        thread_db.emplace_back(client::client_handler, client_fd, client_name);
    } // while (g_running_flag)

    close(g_server_fd);
    LOG(INFO) << "Server stopped";
    return STATUS_SUCCESS;
}

int server_simple_threaded::server_init(uint16_t port) {
    g_server_fd = server_socket_init(port);
    if (g_server_fd < 0) {
        return STATUS_FAIL;
    }

    signal(SIGINT, server_terminate_handler);
    return STATUS_SUCCESS;
}

void server_simple_threaded::server_terminate_handler(int signum) {
    if (!g_running_flag) {
        close(g_server_fd);
        LOG(WARNING) << "Server force stop";
        logging_deinit();
        exit(STATUS_FAIL);
    }
    g_running_flag = false;
    LOG(WARNING) << "Server stop command issued";
    if (-1 != g_server_fd) {
        shutdown(g_server_fd, SHUT_RDWR);
    } else {
        LOG(WARNING) << "Server socket is not set";
    }
}

void server_simple_threaded::client::client_handler(int fd, std::string name) {
    std::string msg_buffer{};

    while (true) {
        if (STATUS_SUCCESS != client::get_request_client(fd, name, msg_buffer)) {
            break;
        }
        DLOG(INFO) << "Read from " << name << " msg:\n" << msg_buffer;
        if (STATUS_SUCCESS != client::send_response_client(fd, name, msg_buffer)) {
            break;
        }
    }
}

int server_simple_threaded::client::connect_client(int &fd, std::string &name) {
    struct sockaddr_in client_addr{};
    socklen_t client_addr_len = sizeof(client_addr);
    int client_sock_fd;

    client_sock_fd = accept(g_server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
    if (client_sock_fd < 0) {
        if (g_running_flag) {
            PLOG(ERROR) << "Error calling accept()";
        }
        // EXIT PROGRAM
        return STATUS_FAIL;
    }
    fd = client_sock_fd;
    name = get_socket_addr_str(&client_addr, client_addr_len);
    DLOG(INFO) << "New connection from " << name;
    return STATUS_SUCCESS;
}

void server_simple_threaded::client::close_client(int fd, const std::string &name) {
    // Disconnect
    close(fd);
    DLOG(INFO) << "Connection closed for " << name;
}

int server_simple_threaded::client::get_request_client(int fd, const std::string &name, std::string &msg_buffer) {
    int io_status = read_msg(fd, msg_buffer);
    if (STATUS_SUCCESS != io_status) {
        LOG(WARNING) << "failed to read from " << name;
        client::close_client(fd, name);
        return STATUS_FAIL;
    }
    // Handling EOF message
    if (msg_buffer.empty()) {
        client::close_client(fd, name);
        return STATUS_FAIL;
    }
    return STATUS_SUCCESS;
}

int server_simple_threaded::client::send_response_client(int fd,
                                                         const std::string &name, const std::string &msg_buffer) {
    int io_status = write_msg(fd, msg_buffer);
    if (STATUS_SUCCESS != io_status) {
        LOG(WARNING) << "failed to write to " << name;
        client::close_client(fd, name);
        return STATUS_FAIL;
    }
    return STATUS_SUCCESS;
}
