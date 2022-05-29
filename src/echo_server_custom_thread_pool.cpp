#include "echo_server_custom_thread_pool.h"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <csignal>
#include <cstring>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <list>

#include "common/io.h"
#include "common/defines.h"
#include "common/socket.h"
#include "common/logging.h"
#include "common/thread_safe_radio_queue.h"


#define WORKER_NUM                      (8)
#define WORK_QUEUE_MAX_SIZE             (0)
#define GC_THRESHOLD                    (10)
#define FD_POOL_TIMEOUT_MS              (1)

#define SERVER_FD_INDEX                 (0)
#define FD_POOL_DUMMY_FD                (-1)
#define INFTIM                          (-1)


// Type declarations
namespace server_custom_thread_pool {
    namespace client {
        enum class client_state {
            IDLE,
            READ,
            WRITE,
            CLOSED
        };

        struct client_data {
            const int fd;
            const std::string name;
            size_t pool_index;
            client_state state;
            std::mutex mutex{};

            client_data(const int fd, std::string &&name, size_t pool_index, client_state state)
                    : fd(fd), name(std::move(name)), pool_index(pool_index), state(state) {};

            ~client_data() = default;
        };
    }
    namespace worker {
        struct job_data {
            client::client_data *client;
            std::string message{};

            job_data() : client(nullptr) {}

            explicit job_data(client::client_data *client) : client(client) {}

            job_data(client::client_data *client, std::string &&message) : client(client),
                                                                           message(std::move(message)) {}
        };
    }
}


namespace server_custom_thread_pool {
    bool g_running_flag = false;
    std::atomic<size_t> g_garbage_count = 0;
    int g_server_fd = -1;
    std::vector<pollfd> g_fd_pool_db{};
    std::list<client::client_data> g_client_db{};
    std::mutex g_db_mutex{};
    std::vector<std::thread> g_worker_pool{};
    t_queue_radio<worker::job_data> g_job_pool{WORK_QUEUE_MAX_SIZE};

    int server_init(uint16_t port);

    void server_deinit();

    void server_terminate_handler(int signum);

    // Garbage Collector
    void gc_routine(bool force = false);

    namespace client {
        int connect_client();

        void close_client(client_data &client);

        int get_request_client(client_data &client, std::string &msg_buffer);

        int send_response_client(client_data &client, const std::string &msg_buffer);
    }

    namespace worker {
        void worker_routine();

        void schedule_read_job(client::client_data &client);

        void schedule_write_job(client::client_data &client, std::string &&msg_buffer);

        void schedule_idle_job(client::client_data &client);
    }
}


int echo_server_custom_thread_pool_main(uint16_t port) {
    int trig_fds_count;

    using namespace server_custom_thread_pool;

    if (STATUS_SUCCESS != server_init(port)) {
        return STATUS_FAIL;
    }

    g_running_flag = true;
    while (g_running_flag) {
        trig_fds_count = poll(g_fd_pool_db.data(), g_fd_pool_db.size(), FD_POOL_TIMEOUT_MS);
        if (trig_fds_count < 0) {
            if (EINTR == errno) {
                continue; // while (g_running_flag)
            } else if (EINVAL == errno) {
                // enforce GC to free up space if possible
                if (g_garbage_count > 0) {
                    gc_routine(true);
                } else {
                    // run out of FD; should not happen if we check fd num limit before accept
                    PLOG(ERROR) << "poll fail; run out of file descriptors";
                    break; // while
                }
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
        if (g_fd_pool_db[SERVER_FD_INDEX].revents) {
            trig_fds_count--;
            // Error handling
            if (POLLIN != g_fd_pool_db[SERVER_FD_INDEX].revents) {
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
        for (auto &elem: g_client_db) {
            if (0 == trig_fds_count) {
                break; // for
            } else if (0 == g_fd_pool_db[elem.pool_index].revents) {
                continue; // for
            }
            trig_fds_count--;

            // Error handling
            if (POLLIN != g_fd_pool_db[elem.pool_index].revents) {
                client::close_client(elem);
                continue; // for
            }
            worker::schedule_read_job(elem);
        } // for

        // Cleanup garbage in DB
        gc_routine();
    } // while (g_running_flag)

    server_deinit();
    return STATUS_SUCCESS;
}

int server_custom_thread_pool::server_init(uint16_t port) {
    g_server_fd = server_socket_init(port);
    if (g_server_fd < 0) {
        return STATUS_FAIL;
    }

    g_fd_pool_db.reserve(SERVER_EXPECT_CONNECTIONS);
    g_worker_pool.reserve(WORKER_NUM);

    g_fd_pool_db.emplace_back(pollfd{g_server_fd, POLLIN | POLLERR | POLLHUP | POLLNVAL, 0});
    g_job_pool.publish();

    DLOG(INFO) << "Starting workers...";
    // Start workers
    for (uint16_t i = 0; i < WORKER_NUM; ++i) {
        g_worker_pool.emplace_back(worker::worker_routine);
    }
    DLOG(INFO) << "All workers started";

    signal(SIGINT, server_terminate_handler);

    return STATUS_SUCCESS;
}

void server_custom_thread_pool::server_deinit() {
    DLOG(INFO) << "Stopping workers...";
    // Stop workers
    g_job_pool.unpublish(); // should send poison pill
    for (uint16_t i = 0; i < WORKER_NUM; ++i) {
        g_worker_pool[i].join();
    }
    DLOG(INFO) << "All workers stopped";

    close(g_server_fd);
    LOG(INFO) << "Server stopped";
}

void server_custom_thread_pool::server_terminate_handler(int signum) {
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

void server_custom_thread_pool::gc_routine(bool force) {
    std::lock_guard<std::mutex> db_lg{g_db_mutex};
    size_t index;
    size_t garbage_collected = 0;
    if (g_garbage_count < GC_THRESHOLD && !force) {
        return;
    }

    g_client_db.remove_if([&garbage_collected](client::client_data &elem) {
        std::lock_guard<std::mutex> lg{elem.mutex};
        bool rc;
        rc = client::client_state::CLOSED == elem.state;
        if (rc) {
            garbage_collected++;
        }
        return rc;
    });

    g_fd_pool_db.clear();
    g_fd_pool_db.reserve(g_client_db.size() + 1 /* for server fd */ + 1 /* for server fd */);
    g_fd_pool_db.emplace_back(pollfd{g_server_fd, POLLIN | POLLERR | POLLHUP | POLLNVAL, 0});
    index = 1;
    for (auto &elem: g_client_db) {
        {
            std::lock_guard<std::mutex> lg{elem.mutex};
            elem.pool_index = index;
            index++;
            if (client::client_state::IDLE == elem.state) {
                g_fd_pool_db.emplace_back(pollfd{elem.fd, POLLIN | POLLERR | POLLHUP | POLLNVAL, 0});
            } else {
                g_fd_pool_db.emplace_back(pollfd{FD_POOL_DUMMY_FD, POLLIN | POLLERR | POLLHUP | POLLNVAL, 0});
            }
        }
    }

    LOG(INFO) << "GC clean up " << g_garbage_count << " elements";
    g_garbage_count -= garbage_collected;
}

int server_custom_thread_pool::client::connect_client() {
    struct sockaddr_in client_addr{};
    socklen_t client_addr_len = sizeof(client_addr);
    int client_sock_fd;

    client_sock_fd = accept(g_server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
    if (client_sock_fd < 0) {
        PLOG(ERROR) << "Error calling accept()";
        return STATUS_FAIL;
    }
    {
        std::lock_guard<std::mutex> db_lg{g_db_mutex};
        g_fd_pool_db.emplace_back(pollfd{client_sock_fd, POLLIN | POLLERR | POLLHUP | POLLNVAL, 0});
        g_client_db.emplace_back(client_sock_fd, get_socket_addr_str(&client_addr, client_addr_len),
                                 g_fd_pool_db.size() - 1, client::client_state::IDLE);
    }
    DLOG(INFO) << "New connection from " << g_client_db.back().name;
    return STATUS_SUCCESS;
}

void server_custom_thread_pool::client::close_client(client_data &client) {
    {
        std::lock_guard<std::mutex> lg{client.mutex};
        if (client::client_state::CLOSED == client.state) {
            LOG(WARNING) << "Try to close closed client " << client.name;
            return;
        }
        client.state = client::client_state::CLOSED;
        close(client.fd);
        g_garbage_count++;
    }
    DLOG(INFO) << "Connection closed for " << client.name;
}

int server_custom_thread_pool::client::get_request_client(client_data &client, std::string &msg_buffer) {
    {
        std::lock_guard<std::mutex> lg{client.mutex};
        if (client::client_state::READ != client.state) {
            LOG(ERROR) << "Try to read from client[" << client.name << "] in invalid state["
                       << static_cast<int>(client.state) << "]";
            return STATUS_FAIL;
        }
    }
    int io_status = read_msg(client.fd, msg_buffer);
    if (STATUS_SUCCESS != io_status) {
        LOG(WARNING) << "Failed to read from " << client.name;
        return STATUS_FAIL;
    }
    // Handling EOF message
    if (msg_buffer.empty()) {
        client::close_client(client);
        return STATUS_FAIL;
    }
    return STATUS_SUCCESS;
}

int server_custom_thread_pool::client::send_response_client(client_data &client, const std::string &msg_buffer) {
    {
        std::lock_guard<std::mutex> lg{client.mutex};
        if (client::client_state::WRITE != client.state) {
            LOG(ERROR) << "Try to write to client[" << client.name << "] in invalid state["
                       << static_cast<int>(client.state) << "]";
            return STATUS_FAIL;
        }
    }
    int io_status = write_msg(client.fd, msg_buffer);
    if (STATUS_SUCCESS != io_status) {
        LOG(WARNING) << "Error failed to write from " << client.name;
        client::close_client(client);
    }
    return io_status;
}

void server_custom_thread_pool::worker::worker_routine() {
    worker::job_data job;
    int rc;

    g_job_pool.subscribe();
    DLOG(INFO) << "Worker started";
    while (true) {
        job = g_job_pool.pop_back();

        // Handle poison pill
        if (nullptr == job.client) {
            DLOG(INFO) << "Worker received poison pill";
            g_job_pool.unsubscribe();
            g_job_pool.emplace_front_force(std::move(job));
            break;
        }

        switch (job.client->state) {
            case client::client_state::READ:
                rc = client::get_request_client(*job.client, job.message);
                if (STATUS_SUCCESS == rc) {
                    DLOG(INFO) << "Read from " << job.client->name << " msg:\n" << job.message;
                    worker::schedule_write_job(*job.client, std::move(job.message));
                }
                break;

            case client::client_state::WRITE:
                rc = client::send_response_client(*job.client, job.message);
                if (STATUS_SUCCESS == rc) {
                    DLOG(INFO) << "Send to " << job.client->name << " msg:\n" << job.message;
                    worker::schedule_idle_job(*job.client);
                }
                break;

            case client::client_state::IDLE:
                LOG(WARNING) << "Worker encountered client[" << job.client->name << "] in invalid state[IDLE]";
                break;

            case client::client_state::CLOSED:
                LOG(WARNING) << "Worker encountered client[" << job.client->name << "] in invalid state[CLOSED]";
                break;

            default:
                LOG(ERROR) << "Worker encountered client[" << job.client->name << "] in invalid state["
                           << static_cast<int>(job.client->state) << ']';
        }
    }
    g_job_pool.unsubscribe();
    DLOG(INFO) << "Worker stopped";
}

// call only from main thread
void server_custom_thread_pool::worker::schedule_read_job(client::client_data &client) {
    {
        std::lock_guard<std::mutex> lg{client.mutex};
        if (client::client_state::IDLE != client.state) {
            LOG(WARNING) << "Client " << client.name << " tried to READ in invalid state["
                         << static_cast<int>(client.state) << ']';
            return;
        }
        client.state = client::client_state::READ;
    }
    // No need for mutex as write is performed in main thread which owns g_fd_pool_db exclusively
    g_fd_pool_db[client.pool_index].fd = FD_POOL_DUMMY_FD;
    g_job_pool.emplace_back(worker::job_data{&client});
}

void server_custom_thread_pool::worker::schedule_write_job(client::client_data &client,
                                                           std::string &&msg_buffer) {
    {
        std::lock_guard<std::mutex> lg{client.mutex};
        if (client::client_state::READ != client.state) {
            LOG(WARNING) << "Client " << client.name << " tried to WRITE in invalid state["
                         << static_cast<int>(client.state) << ']';
            return;
        }
        client.state = client::client_state::WRITE;
    }
    g_job_pool.emplace_back_force(worker::job_data{&client, std::move(msg_buffer)});
}

void server_custom_thread_pool::worker::schedule_idle_job(client::client_data &client) {
    {
        std::lock_guard<std::mutex> lg{client.mutex};
        if (client::client_state::WRITE != client.state) {
            LOG(WARNING) << "Client " << client.name << " tried to IDLE while not in WRITE state";
            return;
        }
        client.state = client::client_state::IDLE;
    }
    {
        std::lock_guard<std::mutex> db_lg{g_db_mutex};
        g_fd_pool_db[client.pool_index].fd = client.fd;
    }
}
