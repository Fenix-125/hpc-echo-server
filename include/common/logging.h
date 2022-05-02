#ifndef ECHO_SERVER_SIMPLE_LOGGING_H
#define ECHO_SERVER_SIMPLE_LOGGING_H

#include <glog/logging.h>
#include <string>

typedef int severity_t;

void logging_init(int *argc, char **argv[]);

void logging_deinit();

void set_log_severity(severity_t severity);

#endif //ECHO_SERVER_SIMPLE_LOGGING_H
