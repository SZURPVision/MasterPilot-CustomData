#ifndef __MAIN_H_
#define __MAIN_H_

#include <stdint.h>

int cmd_config(int argc, char *argv[]);
int cmd_send(int argc, char *argv[]);
int cmd_recv(int argc, char *argv[]);
const char *require_session_file(int argc, char *argv[]);
const char *send_parse_args(int argc, char *argv[], uint16_t *stdin_buf_size);

#endif
