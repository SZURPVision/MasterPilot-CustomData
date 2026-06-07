#ifndef __MAIN_H_
#define __MAIN_H_

int cmd_config(int argc, char *argv[]);
int cmd_send(int argc, char *argv[]);
int cmd_recv(int argc, char *argv[]);
const char *require_session_file(int argc, char *argv[]);

#endif
