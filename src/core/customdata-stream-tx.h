#ifndef __MP_CUSTOMDATA_STREAM_TX_H_
#define __MP_CUSTOMDATA_STREAM_TX_H_

#include "customdata-tx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MP_TX_ACT_DATA, /**< 下游接收数据, 在无header时表示攒存,有header时表示封包发送. */
    MP_TX_ACT_FINALIZE /**< 这是流最后的信号, 根据传来的帧头修改slice, 并随机填满tu发送. */
} mp_tx_action_t;

/*
 * @brief 发送的下游执行器回调
 * @param act 要进行的操作
 * @param hdr header, 可空. 如果为空, 表示积攒数据, 如果非空, 表示需要回填header并发送.
 * @param data 数据块, 可空.
 * @param size 数据块大小. data为空时为0.
 */
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