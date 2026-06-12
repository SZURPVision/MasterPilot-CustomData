#ifndef __MP_CUSTOMDATA_STREAM_TX_H_
#define __MP_CUSTOMDATA_STREAM_TX_H_

#include "customdata-stream-common.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma region TX Stream Context

/*
 * @brief TX 流上下文
 *
 * 维护当前发送坐标，无数据缓存。
 * 每次 mp_tx_encode_stream 调用完全消费输入数据后返回。
 */
typedef struct {
    mp_config_t      config;
    mp_coordinate_t  cursor;
    mp_stream_sink_t downstream;
} mp_tx_stream_context_t;

/*
 * @brief 初始化 TX 流
 * @param start  起始坐标 (由 coordinator 设置 sender_id / package_id)
 */
mp_tx_stream_context_t mp_tx_stream_init(
    mp_config_t          config,
    mp_stream_sink_t     downstream,
    mp_coordinate_t      start
);

#pragma endregion

#pragma region TX Encode

/*
 * @brief 流式编码 — 将数据切片并推送给下游
 *
 * 逐 max_payload 切片输入数据，对每片调用 mp_tx_prepare 生成 header，
 * 通过 downstream.push 回调输出 (header, payload_ptr, payload_size)。
 *
 * 不做任何数据缓存：每次调用完全消费 data[0..size-1]。
 * 兼容 nanopb/protobuf 的小 buffer 反复调用模式。
 *
 * @param is_final_slice  本次数据的最后一字节是否为 package 的终止字节
 * @return 实际消费的字节数 (= size，除非错误)
 */
uint16_t mp_tx_encode_stream(
    mp_tx_stream_context_t *ctx,
    const uint8_t          *data,
    uint16_t                size,
    bool                    is_final_slice
);

#pragma endregion

#ifdef __cplusplus
}
#endif

#endif