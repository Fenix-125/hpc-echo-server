#include <iostream>
#include "common/logging.h"
#include "common/socket.h"
#include "common/defines.h"

#ifdef ECHO_SERVER_SIMPLE
#include "echo_server_simple.h"

#elif ECHO_SERVER_SIMPLE_THREADED
#include "echo_server_simple_threaded.h"

#elif  ECHO_SERVER_CUSTOM_THREAD_POOL
#include "echo_server_custom_thread_pool.h"

#elif  ECHO_SERVER_BOOST_ASIO
#include "echo_server_boost_asio.h"

#elif  ECHO_SERVER_BOOST_ASIO_THREADED
#include "echo_server_boost_asio_threaded.h"

#endif


int main(int argc, char *argv[]) {
    int ret = 0;

    // Initialize Google’s logging library.
    logging_init(&argc, &argv);
    set_log_severity(google::GLOG_INFO);
    sock_num_set_max_limit();

#ifdef ECHO_SERVER_SIMPLE
    ret = echo_server_simple_main(ECHO_SERVER_PORT);

#elif ECHO_SERVER_SIMPLE_THREADED
    ret = echo_server_simple_threaded_main(ECHO_SERVER_PORT);

#elif  ECHO_SERVER_CUSTOM_THREAD_POOL
    ret = echo_server_custom_thread_pool_main(ECHO_SERVER_PORT);

#elif  ECHO_SERVER_BOOST_ASIO
    ret = echo_server_boost_asio_main(ECHO_SERVER_PORT);

#elif  ECHO_SERVER_BOOST_ASIO_THREADED
    ret = echo_server_boost_asio_threaded_main(ECHO_SERVER_PORT);

#else
    LOG(FATAL) << "No valid target specified during compilation!!!";

#endif

    LOG(INFO) << "Server finished with exit code: " << ret;
    logging_deinit();
    return ret;
}
