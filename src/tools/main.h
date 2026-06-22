#ifndef __MAIN_H_
#define __MAIN_H_

#include <stdint.h>

int cmd_config(int argc, char *argv[]);
int cmd_tx(int argc, char *argv[]);
int cmd_rx(int argc, char *argv[]);
const char *require_session_file(int argc, char *argv[]);
const char *tx_parse_args(int argc, char *argv[], uint16_t *stdin_buf_size, uint16_t *sender_id);

#endif
