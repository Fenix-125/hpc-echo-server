//
// Created by fenix on 29.04.2022.
//

#ifndef ECHO_SERVER_SIMPLE_SOCKET_H
#define ECHO_SERVER_SIMPLE_SOCKET_H

#include <cinttypes>
#include <cstddef>

extern size_t g_socket_num_limit;

int server_socket_init(uint16_t port);

void sock_num_set_max_limit();

#endif //ECHO_SERVER_SIMPLE_SOCKET_H
