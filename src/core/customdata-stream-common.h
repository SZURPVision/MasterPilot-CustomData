#ifndef __MP_CUSTOMDATA_STREAM_COMMON_H_
#define __MP_CUSTOMDATA_STREAM_COMMON_H_

#include "customdata-common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * @brief 下游推送回调
 * 流层每完成一个 slice 的编码或解码即调用此回调。
 * user:   上游透传的上下文指针
 * header: 本 slice 的 wire header
 * data:   本 slice 的 payload 数据指针
 * size:   payload 大小 (字节)
 */
typedef void (*mp_stream_push_cb)(
    void              *user,
    const mp_header_t *header,
    const uint8_t     *data,
    uint16_t           size
);

/*
 * @brief 下游接收器
 */
typedef struct {
    mp_stream_push_cb push;
    void             *user;
} mp_stream_sink_t;

#ifdef __cplusplus
}
#endif

#endif