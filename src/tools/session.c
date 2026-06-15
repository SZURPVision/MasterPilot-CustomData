#include "session.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

/* ── helper ── */
static size_t rx_total_size(uint16_t transmission_unit)
{
    uint16_t max_p    = transmission_unit - sizeof(mp_header_meta_t);
    size_t   buf_size = (size_t)256 * max_p;
    return sizeof(mp_rx_session_t) + buf_size;
}

/* ================================================================
 *  TX session — stdout create, mmap load/save
 * ================================================================ */

bool mp_session_tx_create(uint16_t transmission_unit)
{
    mp_tx_session_t s;
    memset(&s, 0, sizeof(s));
    s.magic             = MP_SESSION_SEND_MAGIC;
    s.transmission_unit = transmission_unit;
    /* coord is already zeroed */

    const uint8_t *p = (const uint8_t *)&s;
    size_t len = sizeof(s);
    while (len > 0) {
        ssize_t n = write(STDOUT_FILENO, p, len);
        if (n < 0) return false;
        p   += (size_t)n;
        len -= (size_t)n;
    }
    return true;
}

mp_tx_session_t *mp_session_tx_load(const char *path)
{
    int fd = open(path, O_RDWR);
    if (fd < 0) return NULL;

    struct stat st;
    if (fstat(fd, &st) < 0 || (size_t)st.st_size != sizeof(mp_tx_session_t)) {
        close(fd);
        return NULL;
    }

    void *p = mmap(NULL, sizeof(mp_tx_session_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (p == MAP_FAILED) return NULL;

    mp_tx_session_t *s = (mp_tx_session_t *)p;
    if (s->magic != MP_SESSION_SEND_MAGIC) {
        munmap(p, sizeof(mp_tx_session_t));
        return NULL;
    }
    return s;
}

void mp_session_tx_save(mp_tx_session_t *session)
{
    if (!session) return;
    msync(session, sizeof(mp_tx_session_t), MS_SYNC);
    munmap(session, sizeof(mp_tx_session_t));
}

/* ================================================================
 *  RX session — stdout create, mmap load/save
 * ================================================================ */

bool mp_session_rx_create(uint16_t transmission_unit)
{
    mp_rx_session_t s;
    memset(&s, 0, sizeof(s));
    s.magic             = MP_SESSION_RECV_MAGIC;
    s.transmission_unit = transmission_unit;

    const uint8_t *p = (const uint8_t *)&s;
    size_t len = sizeof(s);
    while (len > 0) {
        ssize_t n = write(STDOUT_FILENO, p, len);
        if (n < 0) return false;
        p   += (size_t)n;
        len -= (size_t)n;
    }

    /* zero-fill the assembly buffer area */
    size_t total  = rx_total_size(transmission_unit);
    size_t buf_sz = total - sizeof(mp_rx_session_t);
    static const uint8_t zero[4096] = {0};
    while (buf_sz > 0) {
        size_t chunk = buf_sz < sizeof(zero) ? buf_sz : sizeof(zero);
        const uint8_t *zp = zero;
        size_t clen = chunk;
        while (clen > 0) {
            ssize_t n = write(STDOUT_FILENO, zp, clen);
            if (n < 0) return false;
            zp   += (size_t)n;
            clen -= (size_t)n;
        }
        buf_sz -= chunk;
    }
    return true;
}

mp_rx_session_t *mp_session_rx_load(uint8_t **assembly_buffer, const char *path)
{
    int fd = open(path, O_RDWR);
    if (fd < 0) return NULL;

    struct stat st;
    if (fstat(fd, &st) < 0 || st.st_size < (off_t)sizeof(mp_rx_session_t)) {
        close(fd);
        return NULL;
    }

    size_t   total = (size_t)st.st_size;
    uint16_t tu;
    {
        /* peek transmission_unit to validate */
        mp_rx_session_t peek;
        if (pread(fd, &peek, sizeof(peek), 0) != (ssize_t)sizeof(peek)) {
            close(fd);
            return NULL;
        }
        if (peek.magic != MP_SESSION_RECV_MAGIC) {
            close(fd);
            return NULL;
        }
        tu = peek.transmission_unit;
    }

    size_t expected = rx_total_size(tu);
    if (total != expected) {
        close(fd);
        return NULL;
    }

    void *p = mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (p == MAP_FAILED) return NULL;

    *assembly_buffer = (uint8_t *)p + sizeof(mp_rx_session_t);
    return (mp_rx_session_t *)p;
}

void mp_session_rx_save(mp_rx_session_t *session, uint8_t *assembly_buffer)
{
    if (!session) return;
    size_t total = rx_total_size(session->transmission_unit);
    msync(session, total, MS_SYNC);
    munmap(session, total);
    (void)assembly_buffer; /* assembly_buffer = session + sizeof(*session), same munmap */
}