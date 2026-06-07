#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include "main.h"
#include "session.h"

#define DEFAULT_MTU          300
#define DEFAULT_BUFFER_COUNT 16

static void print_usage(const char *prog_name)
{
    fprintf(stderr, "Usage: %s <subcommand> [options]\n\n", prog_name);
    fprintf(stderr, "Subcommands:\n");
    fprintf(stderr, "  config   Create a new session file (output to stdout)\n");
    fprintf(stderr, "  send     Encode one packet from stdin using session\n");
    fprintf(stderr, "  recv     Decode packets from stdin using session\n\n");
    fprintf(stderr, "Options for 'config':\n");
    fprintf(stderr, "  -m, --mtu <bytes>      Set MTU size (default: %d)\n", DEFAULT_MTU);
    fprintf(stderr, "  -c, --count <num>      Set buffer count (default: %d)\n", DEFAULT_BUFFER_COUNT);
    fprintf(stderr, "  -h, --help             Show this help message\n\n");
    fprintf(stderr, "Options for 'send'/'recv':\n");
    fprintf(stderr, "  -s, --session <path>   Session file path (required)\n");
    fprintf(stderr, "  -h, --help             Show this help message\n");
}

int cmd_config(int argc, char *argv[])
{
    int mtu = DEFAULT_MTU;
    int buffer_count = DEFAULT_BUFFER_COUNT;
    int type = 0; /* 0=send, 1=recv */

    static struct option long_options[] = {
        {"mtu",   required_argument, 0, 'm'},
        {"count", required_argument, 0, 'c'},
        {"type",  required_argument, 0, 't'},
        {"help",  no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "m:c:t:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'm': mtu = atoi(optarg); break;
            case 'c': buffer_count = atoi(optarg); break;
            case 't':
                if (strcmp(optarg, "recv") == 0) type = 1;
                else if (strcmp(optarg, "send") == 0) type = 0;
                else {
                    fprintf(stderr, "Error: -t must be 'send' or 'recv'\n");
                    return 1;
                }
                break;
            case 'h':
                fprintf(stderr, "Usage: %s config [-m MTU] [-c COUNT] [-t send|recv]\n", argv[0]);
                fprintf(stderr, "Create a new session file, output to stdout.\n");
                return 0;
            default:
                return 1;
        }
    }

    if (mtu < (int)sizeof(mp_header_t) + 1) {
        fprintf(stderr, "Error: MTU must be > header size (%zu)\n", sizeof(mp_header_t));
        return 1;
    }
    if (buffer_count < 3) {
        fprintf(stderr, "Error: Buffer count must be >= 3\n");
        return 1;
    }
    if (mtu > 65535 || buffer_count > 255) {
        fprintf(stderr, "Error: MTU must be <= 65535, buffer_count <= 255\n");
        return 1;
    }

    if (type == 1) {
        return mp_session_recv_create((uint16_t)mtu, (uint8_t)buffer_count) ? 0 : 1;
    } else {
        return mp_session_send_create((uint16_t)mtu, (uint8_t)buffer_count) ? 0 : 1;
    }
}

#define DEFAULT_STDIN_BUF_SIZE 64

const char *send_parse_args(int argc, char *argv[], uint16_t *stdin_buf_size)
{
    static struct option long_options[] = {
        {"session",   required_argument, 0, 's'},
        {"stdin-buf", required_argument, 0, 'b'},
        {"help",      no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    const char *session_path = NULL;
    *stdin_buf_size = DEFAULT_STDIN_BUF_SIZE;

    int opt;
    while ((opt = getopt_long(argc, argv, "s:b:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 's': session_path = optarg; break;
            case 'b': *stdin_buf_size = (uint16_t)atoi(optarg); break;
            case 'h': return NULL;
            default:  return NULL;
        }
    }

    if (*stdin_buf_size == 0) {
        fprintf(stderr, "Error: stdin buffer size must be > 0\n");
        return NULL;
    }

    if (!session_path) {
        fprintf(stderr, "Error: -s/--session <path> is required\n");
        return NULL;
    }

    return session_path;
}

const char *require_session_file(int argc, char *argv[])
{
    static struct option long_options[] = {
        {"session", required_argument, 0, 's'},
        {"help",    no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    const char *session_path = NULL;

    int opt;
    while ((opt = getopt_long(argc, argv, "s:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 's': session_path = optarg; break;
            case 'h': return NULL;
            default:  return NULL;
        }
    }

    if (!session_path) {
        fprintf(stderr, "Error: -s/--session <path> is required\n");
        return NULL;
    }

    return session_path;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *subcommand = argv[1];

    /* Shift argv so subcommand functions see their own args starting at [0] */
    if (strcmp(subcommand, "config") == 0) {
        return cmd_config(argc - 1, argv + 1);
    } else if (strcmp(subcommand, "send") == 0) {
        return cmd_send(argc - 1, argv + 1);
    } else if (strcmp(subcommand, "recv") == 0) {
        return cmd_recv(argc - 1, argv + 1);
    } else if (strcmp(subcommand, "-h") == 0 || strcmp(subcommand, "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    } else {
        fprintf(stderr, "Error: Unknown subcommand '%s'\n\n", subcommand);
        print_usage(argv[0]);
        return 1;
    }
}