//
// Created by fenix on 08.04.2022.
//

#ifndef ECHO_SERVER_SIMPLE_DEFINES_H
#define ECHO_SERVER_SIMPLE_DEFINES_H

#define IP_MAX_STR_SIZE                 (16)

#define ECHO_SERVER_PORT                (4025)

#define EXPECTED_MESSAGE_SIZE           (128)
#define SERVER_LISTEN_BACKLOG_SIZE      (10000)
#define SERVER_EXPECT_CONNECTIONS       (11000)

#define SERVER_DEFAULT_LOG_DIR          ("./logs")

#define STATUS_SUCCESS                  (0)
#define STATUS_FAIL                     (-1)

#endif //ECHO_SERVER_SIMPLE_DEFINES_H
