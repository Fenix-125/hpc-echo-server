// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include <iostream>
#include "common/logging.h"

#ifdef ECHO_SERVER_SIMPLE

#include "echo_server_simple.h"

#elif ECHO_SERVER_SIMPLE_THREADED
#include "echo_server_simple_threaded.h"

#endif


int main(int argc, char *argv[]) {
    int ret = 0;

    // Initialize Google’s logging library.
    logging_init(&argc, &argv);
    set_log_severity(google::GLOG_INFO);

#ifdef ECHO_SERVER_SIMPLE
    ret = echo_server_simple_main(4025);

#elif ECHO_SERVER_SIMPLE_THREADED
    ret = echo_server_simple_threaded_main(4025);

#else
    LOG(FATAL) << "No valid target specified during compilation!!!";

#endif

    LOG(INFO) << "Server finished with exit code: " << ret;
    logging_deinit();
    return ret;
}
