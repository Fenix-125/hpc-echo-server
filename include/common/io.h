//
// Created by fenix on 05.04.2022.
//

#ifndef ECHO_SERVER_SIMPLE_IO_H
#define ECHO_SERVER_SIMPLE_IO_H

#include <string>
#include <sys/socket.h>
#include <cstddef>

std::string get_socket_addr_str(const struct sockaddr_in *cl_addr, socklen_t cl_addr_len);

ssize_t write_buffer(int fd, const char *buffer, ssize_t size);

ssize_t read_buffer(int fd, char *buffer, ssize_t size);

int read_msg(int fd, std::string &s);

int write_msg(int fd, const std::string &s);

#endif //ECHO_SERVER_SIMPLE_IO_H
