// source link: https://theboostcpplibraries.com/boost.asio-coroutines
#include "echo_server_boost_asio_threaded.h"
#include <boost/asio/io_service.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <exception>
#include <csignal>
#include <string>
#include <sstream>
#include <list>
#include <thread>

#include "common/defines.h"
#include "common/logging.h"

#define ECHO_SERVER_THREADS         (8)

namespace server_boost_asio {
    bool g_running_flag = false;
    int g_rc = STATUS_SUCCESS;

    boost::asio::io_service g_io_service{};
    boost::asio::ip::tcp::endpoint g_endpoint;
    boost::asio::ip::tcp::acceptor g_acceptor{g_io_service};
    std::list<std::thread> g_thread_list{};

    int server_init(uint16_t port);

    void server_terminate_handler(int signum);

    void server_worker();

    namespace client {
        void connect_client();

        void close_client(std::shared_ptr<boost::asio::ip::tcp::socket> client_p);

        void
        get_request_client(std::shared_ptr<boost::asio::ip::tcp::socket> socket_p, std::shared_ptr<char[]> buffer_p);

        void send_response_client(std::shared_ptr<boost::asio::ip::tcp::socket> socket_p,
                                  std::shared_ptr<char[]> buffer_p, size_t length);

        std::string get_client_name(std::shared_ptr<boost::asio::ip::tcp::socket> client_p);
    }
}

int echo_server_boost_asio_threaded_main(uint16_t port) {
    std::string msg_buffer{};

    using namespace server_boost_asio;

    if (STATUS_SUCCESS != server_init(port)) {
        return STATUS_FAIL;
    }
    g_running_flag = true;

    client::connect_client();

    for (int i = 0; i < ECHO_SERVER_THREADS - 1; ++i) {
        g_thread_list.emplace_back(server_worker);
    }
    server_worker();
    g_acceptor.close();
    for (auto &thread: g_thread_list) {
        thread.join();
    }
    LOG(INFO) << "Server stopped";
    return g_rc;
}

int server_boost_asio::server_init(uint16_t port) {
    try {
        g_endpoint = boost::asio::ip::tcp::endpoint{boost::asio::ip::tcp::v4(), port};
        g_acceptor.open(g_endpoint.protocol());
        g_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        g_acceptor.set_option(boost::asio::ip::tcp::acceptor::receive_buffer_size(EXPECTED_MESSAGE_SIZE));
        g_acceptor.set_option(boost::asio::ip::tcp::acceptor::send_buffer_size(EXPECTED_MESSAGE_SIZE));
        g_acceptor.bind(g_endpoint);
        g_acceptor.listen(SERVER_LISTEN_BACKLOG_SIZE);
    } catch (std::exception &e) {
        LOG(ERROR) << e.what();
        return STATUS_FAIL;
    }
    signal(SIGINT, server_terminate_handler);

    LOG(INFO) << "Server started";
    return STATUS_SUCCESS;
}

void server_boost_asio::server_terminate_handler(int signum) {
    if (!g_running_flag) {
        g_acceptor.close();
        LOG(WARNING) << "Server force stop";
        logging_deinit();
        exit(STATUS_FAIL);
    }
    g_running_flag = false;
    g_io_service.stop();
    LOG(INFO) << "Server stop command issued";
}

void server_boost_asio::server_worker() {
    g_io_service.run();
}

void server_boost_asio::client::connect_client() {
    g_acceptor.async_accept([](boost::system::error_code ec, boost::asio::ip::tcp::socket socket_p) {
        if (!ec) {
            auto client_p = std::make_shared<boost::asio::ip::tcp::socket>(std::move(socket_p));
            auto buffer_p = std::shared_ptr<char[]>{new char[EXPECTED_MESSAGE_SIZE]};
            DLOG(INFO) << "New connection from " << get_client_name(client_p);
            get_request_client(client_p, buffer_p);
        } else {
            LOG(ERROR) << ec.message();
            g_rc = STATUS_FAIL;
            g_running_flag = false;
        }
        if (g_running_flag) {
            connect_client();
        } else {
            g_io_service.stop();
        }
    });
}

void server_boost_asio::client::get_request_client(std::shared_ptr<boost::asio::ip::tcp::socket> socket_p,
                                                   std::shared_ptr<char[]> buffer_p) {
    socket_p->async_read_some(
            boost::asio::buffer(buffer_p.get(), EXPECTED_MESSAGE_SIZE),
            [buffer_p, socket_p](boost::system::error_code ec, size_t length) {
                if (!ec) {
                    DLOG(INFO) << "Read from " << get_client_name(socket_p) << " msg:\n"
                               << std::string{buffer_p.get(), length};
                    send_response_client(socket_p, buffer_p, length);
                } else {
                    close_client(socket_p);
                }
            }
    );
}

void server_boost_asio::client::send_response_client(std::shared_ptr<boost::asio::ip::tcp::socket> socket_p,
                                                     std::shared_ptr<char[]> buffer_p, size_t length) {
    socket_p->async_write_some(
            boost::asio::buffer(buffer_p.get(), length),
            [buffer_p, socket_p](boost::system::error_code ec, size_t length) {
                if (!ec) {
                    get_request_client(socket_p, buffer_p);
                } else {
                    close_client(socket_p);
                }
            }
    );
}

void server_boost_asio::client::close_client(std::shared_ptr<boost::asio::ip::tcp::socket> client_p) {
    DLOG(INFO) << "Connection closed for " << get_client_name(client_p);
    client_p->close();
}

std::string server_boost_asio::client::get_client_name(
        std::shared_ptr<boost::asio::ip::tcp::socket> client_p) {
    std::stringstream s{};
    s << client_p->remote_endpoint().address().to_string() << ":"
      << client_p->remote_endpoint().port();
    return s.str();
}
