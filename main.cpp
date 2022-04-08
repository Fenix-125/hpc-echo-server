// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include <iostream>
#include "options_parser/options_parser.h"
#include "echo_server_simple.h"


int main(int argc, char* argv[]) {
    int ret = 0;

    command_line_options_t command_line_options{argc, argv};
//    std::cout << "A flag value: " << command_line_options.get_A_flag() << std::endl;
    ret = echo_server_simple_main(4025);

    std::cout << "Echo server finished with exit code: " << ret << std::endl;
    return ret;
}
