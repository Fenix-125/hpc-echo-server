// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include <iostream>
#include "common/logging.h"
#include "options_parser/options_parser.h"
#include "echo_server_simple.h"
#include "echo_server_simple_threaded.h"


int main(int argc, char *argv[]) {
    int ret;

//    command_line_options_t command_line_options{argc, argv};
//    std::cout << "A flag value: " << command_line_options.get_A_flag() << std::endl;

    // Initialize Google’s logging library.
    logging_init(&argc, &argv);
    set_log_severity(google::GLOG_INFO);

//    ret = echo_server_simple_main(4025);
    ret = echo_server_simple_threaded_main(4025);

    LOG(INFO) << "Server finished with exit code: " << ret;
    logging_deinit();
    return ret;
}
