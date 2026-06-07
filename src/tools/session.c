#include "session.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/* ================================================================
 *  Write helpers
 * ================================================================ */

static bool write_full(int fd, const void *buf, size_t len)
{
    const uint8_t *p = (const uint8_t *)buf;
    while (len > 0) {
        ssize_t n = write(fd, p, len);
        if (n < 0) return false;
        p   += n;
        len -= n;
    }
    return true;
}

static bool read_full(int fd, void *buf, size_t len)
{
    uint8_t *p = (uint8_t *)buf;
    while (len > 0) {
        ssize_t n = read(fd, p, len);
        if (n <= 0) return false;
        p   += n;
        len -= n;
    }
    return true;
}

static bool write_zero(int fd, size_t len)
{
    static const uint8_t zero[4096] = {0};
    while (len > 0) {
        size_t chunk = len < sizeof(zero) ? len : sizeof(zero);
        if (!write_full(fd, zero, chunk)) return false;
        len -= chunk;
    }
    return true;
}

/* ================================================================
 *  Send session create
 * ================================================================ */

bool mp_session_send_create(uint16_t mtu, uint8_t buffer_count)
{
    uint32_t magic    = MP_SESSION_SEND_MAGIC;
    uint8_t  head     = 0;
    uint8_t  tail     = 0;
    uint16_t serial   = 0;

    if (!write_full(STDOUT_FILENO, &magic,        4))  return false;
    if (!write_full(STDOUT_FILENO, &buffer_count, 1))  return false;
    if (!write_full(STDOUT_FILENO, &mtu,          2))  return false;
    if (!write_full(STDOUT_FILENO, &head,         1))  return false;
    if (!write_full(STDOUT_FILENO, &tail,         1))  return false;
    if (!write_full(STDOUT_FILENO, &serial,       2))  return false;
    if (!write_zero(STDOUT_FILENO, (size_t)buffer_count * mtu)) return false;

    return true;
}

/* ================================================================
 *  Recv session create
 * ================================================================ */

bool mp_session_recv_create(uint16_t mtu, uint8_t buffer_count)
{
    uint32_t magic            = MP_SESSION_RECV_MAGIC;
    uint8_t  head             = 0;
    uint8_t  tail             = 0;
    uint8_t  slice_base       = 0;
    uint8_t  slice_bitmap[32] = {0};
    uint8_t  slices_received  = 0;
    uint8_t  terminal_serial  = 0xFF;
    uint8_t  pkg_complete     = 0;

    if (!write_full(STDOUT_FILENO, &magic,          4))  return false;
    if (!write_full(STDOUT_FILENO, &buffer_count,   1))  return false;
    if (!write_full(STDOUT_FILENO, &mtu,            2))  return false;
    if (!write_full(STDOUT_FILENO, &head,           1))  return false;
    if (!write_full(STDOUT_FILENO, &tail,           1))  return false;
    if (!write_full(STDOUT_FILENO, &slice_base,     1))  return false;
    if (!write_full(STDOUT_FILENO, slice_bitmap,   32))  return false;
    if (!write_full(STDOUT_FILENO, &slices_received, 1)) return false;
    if (!write_full(STDOUT_FILENO, &terminal_serial, 1)) return false;
    if (!write_full(STDOUT_FILENO, &pkg_complete,   1))  return false;
    if (!write_zero(STDOUT_FILENO, (size_t)buffer_count * mtu)) return false;

    return true;
}

/* ================================================================
 *  Send session load
 * ================================================================ */

bool mp_session_send_load(mp_sender_t *sender, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "mp_session_send_load: fopen(%s) failed: %s\n",
                path, strerror(errno));
        return false;
    }

    uint32_t magic;
    uint8_t  buffer_count;
    uint16_t mtu;
    uint8_t  head, tail;
    uint16_t serial;

    bool ok = true;
    if (!read_full(fileno(f), &magic,        4))  { ok = false; goto out; }
    if (!read_full(fileno(f), &buffer_count, 1))  { ok = false; goto out; }
    if (!read_full(fileno(f), &mtu,          2))  { ok = false; goto out; }
    if (!read_full(fileno(f), &head,         1))  { ok = false; goto out; }
    if (!read_full(fileno(f), &tail,         1))  { ok = false; goto out; }
    if (!read_full(fileno(f), &serial,       2))  { ok = false; goto out; }

    if (magic != MP_SESSION_SEND_MAGIC) {
        fprintf(stderr, "mp_session_send_load: bad magic 0x%08X\n", magic);
        ok = false;
        goto out;
    }

    size_t buf_size = (size_t)buffer_count * mtu;
    uint8_t *buffer = (uint8_t *)malloc(buf_size);
    if (!buffer) {
        fprintf(stderr, "mp_session_send_load: malloc(%zu) failed\n", buf_size);
        ok = false;
        goto out;
    }

    if (!read_full(fileno(f), buffer, buf_size)) {
        free(buffer);
        ok = false;
        goto out;
    }

    {
        mp_config_t cfg = { .buffer = buffer, .buffer_count = buffer_count, .mtu = mtu };
        mp_sender_t s   = { .head = head, .tail = tail, .serial = serial, .config = cfg };
        memcpy(sender, &s, sizeof(*sender));
    }

out:
    fclose(f);
    return ok;
}

/* ================================================================
 *  Send session save
 * ================================================================ */

bool mp_session_send_save(const mp_sender_t *sender, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "mp_session_send_save: fopen(%s) failed: %s\n",
                path, strerror(errno));
        return false;
    }

    uint32_t magic    = MP_SESSION_SEND_MAGIC;
    uint8_t  bc       = sender->config.buffer_count;
    uint16_t mtu      = sender->config.mtu;
    uint8_t  head     = sender->head;
    uint8_t  tail     = sender->tail;
    uint16_t serial   = sender->serial;
    size_t   buf_size = (size_t)bc * mtu;

    bool ok = true;
    if (!write_full(fileno(f), &magic, 4))       { ok = false; goto out; }
    if (!write_full(fileno(f), &bc,    1))       { ok = false; goto out; }
    if (!write_full(fileno(f), &mtu,   2))       { ok = false; goto out; }
    if (!write_full(fileno(f), &head,  1))       { ok = false; goto out; }
    if (!write_full(fileno(f), &tail,  1))       { ok = false; goto out; }
    if (!write_full(fileno(f), &serial, 2))      { ok = false; goto out; }
    if (!write_full(fileno(f), sender->config.buffer, buf_size)) { ok = false; goto out; }

out:
    fclose(f);
    return ok;
}

/* ================================================================
 *  Recv session load
 * ================================================================ */

bool mp_session_recv_load(mp_receiver_t *receiver, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "mp_session_recv_load: fopen(%s) failed: %s\n",
                path, strerror(errno));
        return false;
    }

    uint32_t magic;
    uint8_t  buffer_count;
    uint16_t mtu;
    uint8_t  head, tail, slice_base;
    uint8_t  slice_bitmap[32];
    uint8_t  slices_received, terminal_serial, pkg_complete;

    bool ok = true;
    if (!read_full(fileno(f), &magic,           4))  { ok = false; goto out; }
    if (!read_full(fileno(f), &buffer_count,    1))  { ok = false; goto out; }
    if (!read_full(fileno(f), &mtu,             2))  { ok = false; goto out; }
    if (!read_full(fileno(f), &head,            1))  { ok = false; goto out; }
    if (!read_full(fileno(f), &tail,            1))  { ok = false; goto out; }
    if (!read_full(fileno(f), &slice_base,      1))  { ok = false; goto out; }
    if (!read_full(fileno(f), slice_bitmap,    32))  { ok = false; goto out; }
    if (!read_full(fileno(f), &slices_received, 1))  { ok = false; goto out; }
    if (!read_full(fileno(f), &terminal_serial, 1))  { ok = false; goto out; }
    if (!read_full(fileno(f), &pkg_complete,    1))  { ok = false; goto out; }

    if (magic != MP_SESSION_RECV_MAGIC) {
        fprintf(stderr, "mp_session_recv_load: bad magic 0x%08X\n", magic);
        ok = false;
        goto out;
    }

    {
        size_t buf_size = (size_t)buffer_count * mtu;
        uint8_t *buffer = (uint8_t *)malloc(buf_size);
        if (!buffer) {
            fprintf(stderr, "mp_session_recv_load: malloc(%zu) failed\n", buf_size);
            ok = false;
            goto out;
        }

        if (!read_full(fileno(f), buffer, buf_size)) {
            free(buffer);
            ok = false;
            goto out;
        }

        mp_config_t   cfg = { .buffer = buffer, .buffer_count = buffer_count, .mtu = mtu };
        mp_receiver_t r   = {
            .tail              = tail,
            .head              = head,
            .slice_base        = slice_base,
            .slices_received   = slices_received,
            .terminal_serial   = terminal_serial,
            .package_complete  = (bool)pkg_complete,
            .config            = cfg
        };
        memcpy((void *)r.slice_bitmap, slice_bitmap, 32);
        memcpy((void *)receiver, &r, sizeof(*receiver));
    }

out:
    fclose(f);
    return ok;
}

/* ================================================================
 *  Recv session save
 * ================================================================ */

bool mp_session_recv_save(const mp_receiver_t *receiver, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "mp_session_recv_save: fopen(%s) failed: %s\n",
                path, strerror(errno));
        return false;
    }

    uint32_t magic           = MP_SESSION_RECV_MAGIC;
    uint8_t  bc              = receiver->config.buffer_count;
    uint16_t mtu             = receiver->config.mtu;
    uint8_t  head            = receiver->head;
    uint8_t  tail            = receiver->tail;
    uint8_t  slice_base      = receiver->slice_base;
    uint8_t  slices_received = receiver->slices_received;
    uint8_t  terminal_serial = receiver->terminal_serial;
    uint8_t  pkg_complete    = (uint8_t)receiver->package_complete;
    size_t   buf_size        = (size_t)bc * mtu;

    bool ok = true;
    if (!write_full(fileno(f), &magic,           4))  { ok = false; goto out; }
    if (!write_full(fileno(f), &bc,              1))  { ok = false; goto out; }
    if (!write_full(fileno(f), &mtu,             2))  { ok = false; goto out; }
    if (!write_full(fileno(f), &head,            1))  { ok = false; goto out; }
    if (!write_full(fileno(f), &tail,            1))  { ok = false; goto out; }
    if (!write_full(fileno(f), &slice_base,      1))  { ok = false; goto out; }
    if (!write_full(fileno(f), (const void *)receiver->slice_bitmap, 32)) { ok = false; goto out; }
    if (!write_full(fileno(f), &slices_received, 1))  { ok = false; goto out; }
    if (!write_full(fileno(f), &terminal_serial, 1))  { ok = false; goto out; }
    if (!write_full(fileno(f), &pkg_complete,    1))  { ok = false; goto out; }
    if (!write_full(fileno(f), receiver->config.buffer, buf_size)) { ok = false; goto out; }

out:
    fclose(f);
    return ok;
}

/* ================================================================
 *  Free
 * ================================================================ */

void mp_session_free(mp_config_t cfg)
{
    free(cfg.buffer);
}