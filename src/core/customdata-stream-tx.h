#ifndef __MP_CUSTOMDATA_STREAM_TX_H_
#define __MP_CUSTOMDATA_STREAM_TX_H_

#include "customdata-tx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MP_TX_ACT_DATA, /**< 下游接收数据 */
    MP_TX_ACT_FINALIZE /**< 下游根据传来的可空帧头, */
} mp_tx_action_t;

typedef void (*mp_tx_push_cb)(
    void             *user,
    mp_tx_action_t    act,
    const mp_header_t *hdr,
    const uint8_t    *data,
    uint16_t          size
);

typedef struct {
    mp_tx_push_cb push;
    void          *user;
} mp_tx_sink_t;

typedef struct {
    mp_config_t      config;
    mp_coordinate_t  cursor;
    uint16_t         fill;
    mp_tx_sink_t     downstream;
} mp_tx_stream_t;

void mp_tx_stream_init(
    mp_tx_stream_t   *s,
    mp_config_t       config,
    mp_coordinate_t   start,
    mp_tx_sink_t      downstream
);

void mp_tx_stream_feed(
    mp_tx_stream_t *s,
    const uint8_t  *data,
    uint16_t        size
);

void mp_tx_stream_finalize(mp_tx_stream_t *s);

#ifdef __cplusplus
}
#endif

#endif