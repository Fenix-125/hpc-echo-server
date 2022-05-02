//
// Created by fenix on 08.04.2022.
//

#ifndef ECHO_SERVER_SIMPLE_DEFINES_H
#define ECHO_SERVER_SIMPLE_DEFINES_H

#define IP_MAX_STR_SIZE                 (16)

#define EXPECTED_MESSAGE_SIZE           (4086)
#define SERVER_LISTEN_BACKLOG_SIZE      (100)
#define SERVER_EXPECT_CONNECTIONS       (100)

#define SERVER_MSG_PREFIX               ("[server] ")
#define SERVER_DEFAULT_LOG_DIR          ("./logs")

#define STATUS_SUCCESS                  (0)
#define STATUS_FAIL                     (-1)

#endif //ECHO_SERVER_SIMPLE_DEFINES_H
