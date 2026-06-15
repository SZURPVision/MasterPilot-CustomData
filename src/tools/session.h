#ifndef __MP_SESSION_H_
#define __MP_SESSION_H_

#include <stdint.h>
#include <stdbool.h>
#include <masterpilot/customdata-common.h>
#include <masterpilot/customdata-rx.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MP_SESSION_SEND_MAGIC  0x4D505364
#define MP_SESSION_RECV_MAGIC  0x4D505276

/*
 * @brief TX session — 持久化发送坐标 (mmap 到文件, 直接内存操作)
 *
 * Layout: magic(4) | transmission_unit(2) | sender_id(2) | package_id(2) | offset(4) = 14B
 */
#pragma pack(push,1)
typedef struct {
    uint32_t        magic;
    uint16_t        transmission_unit;
    mp_coordinate_t coord;
} mp_tx_session_t;

/*
 * @brief RX session — 持久化状态 + assembly buffer (mmap 到文件)
 *
 * Layout: magic(4) | transmission_unit(2) | sender_id(2) | package_id(2) | state(40) | assembly_buf(N)
 */
typedef struct {
    uint32_t             magic;
    uint16_t             transmission_unit;
    mp_rx_slice_state_t  state;
} mp_rx_session_t;
#pragma pack(pop)

/* ── TX session ── */
bool mp_session_tx_create(uint16_t transmission_unit);
mp_tx_session_t *mp_session_tx_load(const char *path);
void mp_session_tx_save(mp_tx_session_t *session);

/* ── RX session (stdout for create, mmap for load/save) ── */
bool mp_session_rx_create(uint16_t transmission_unit);
mp_rx_session_t *mp_session_rx_load(uint8_t **assembly_buffer, const char *path);
void mp_session_rx_save(mp_rx_session_t *session, uint8_t *assembly_buffer);

#ifdef __cplusplus
}
#endif

#endif
