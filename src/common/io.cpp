#include "common/io.h"
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>

#include "common/defines.h"

std::string get_socket_addr_str(const struct sockaddr_in *cl_addr, socklen_t cl_addr_len) {
    char user_ip_str[IP_MAX_STR_SIZE];
    std::stringstream user_addr_s{};

    if (cl_addr->sin_family == AF_INET) {
        inet_ntop(AF_INET, &cl_addr->sin_addr, user_ip_str, cl_addr_len);
    } else {
        return "Unknown AF";
    }
    user_addr_s << user_ip_str << ':' << ntohs(cl_addr->sin_port);
    return user_addr_s.str();
}

// size param should include place for terminating '\0' character
ssize_t read_buffer(int fd, char *buffer, ssize_t size) {
    ssize_t read_bytes;

    if (size > 1) {
        // reserve one byte for '\0' terminating character
        --size;
    } else {
        return STATUS_FAIL;
    }

    do {
        read_bytes = read(fd, static_cast<void *>(buffer), size);
        if (STATUS_FAIL == read_bytes && EINTR != errno) {
            return STATUS_FAIL;
        }
    } while (STATUS_FAIL == read_bytes);

    if (read_bytes >= 0) {
        buffer[read_bytes] = '\0';
    } else {
        read_bytes = STATUS_FAIL;
    }
    return read_bytes;
}

ssize_t write_buffer(int fd, const char *buffer, ssize_t size) {
    ssize_t written_bytes = 0;
    ssize_t written_now;

    while (written_bytes < size) {
        written_now = write(fd, buffer + written_bytes, size - written_bytes);
        if (STATUS_FAIL == written_now) {
            if (EINTR == errno)
                continue;
            else {
                return STATUS_FAIL;
            }
        } else
            written_bytes += written_now;
    }
    return written_bytes;
}

int read_msg(int fd, std::string &s) {
    char buffer[EXPECTED_MESSAGE_SIZE + 1];
    ssize_t read_bytes;

    s.reserve(EXPECTED_MESSAGE_SIZE);

    read_bytes = read_buffer(fd, buffer, EXPECTED_MESSAGE_SIZE + 1);
    if (read_bytes < 0) {
        return STATUS_FAIL;
    }

    s.assign(buffer, read_bytes);
    return STATUS_SUCCESS;
}

int write_msg(int fd, const std::string &s) {
    auto s_size = static_cast<ssize_t>(s.size());
    // check for overflow
    if (s_size < 0) {
        return STATUS_FAIL;
    }

    if (STATUS_FAIL == write_buffer(fd, s.c_str(), s_size)) {
        return STATUS_FAIL;
    } else {
        return STATUS_SUCCESS;
    }
}