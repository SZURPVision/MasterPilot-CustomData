#ifndef __MP_CUSTOMDATA_STREAM_RX_H_
#define __MP_CUSTOMDATA_STREAM_RX_H_

#include "customdata-rx.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma region RX Coordinator

typedef const mp_rx_slice_state_t *(*mp_rx_slice_state_getter_t)(
    void *ctx,
    mp_coordinate_t coord
);

typedef void (*mp_rx_slice_state_setter_t)(
    void *ctx,
    mp_coordinate_t coord,
    const mp_rx_slice_state_t* state
);

/**
 * @brief 存入临时的乱序 slice payload.
 * @param payload 要存入的payload
*/
typedef void (*mp_rx_payload_setter_t)(
    void *ctx,
    mp_coordinate_t coord,
    const uint8_t *payload,
    uint16_t size
);

/**
 * @brief 取出临时的乱序 slice payload.
 * @return payload头部只读指针
*/
typedef const uint8_t* (*mp_rx_payload_getter_t)(
    void *ctx,
    mp_coordinate_t coord
);

/*
 * @brief RX的`坐标定位器`, 根据传入的坐标去操作内存
 */
typedef struct {
    void *ctx;

    mp_rx_slice_state_getter_t state_get;
    mp_rx_slice_state_setter_t state_put;

    mp_rx_payload_setter_t payload_put;
    mp_rx_payload_getter_t payload_get;

} mp_rx_coordinator_t;

#pragma endregion

#pragma region RX Event Callback

/**
 * @brief rx 下游事件回调. 流发生状态变更时通知下游.
 *
 * 事件 + watermark 差值完整描述了发生了什么, 下游无需自行推理:
 *   OUT_OF_ORDER: old_watermark == state->watermark → 区间为空, 仅 payload_put 暂存.
 *   IN_ORDER:     old_watermark <  state->watermark → 连续区域补齐, payload_get 冲刷.
 *   COMPLETE:     old_watermark <  state->watermark → 完整包冲刷, 包边界信号.
 *
 * @param event 事件类型 (DUPLICATE 已被 stream 层拦截, 下游不会收到)
 * @param coord 当前 slice 坐标
 * @param coordinator 提供 payload_put / payload_get, 下游自行存取数据
 * @param state 事件后的新状态
 * @param old_watermark 事件前的 watermark
 * @param payload 指向当前 frame 的 payload 数据, 仅本次回调有效
 * @param payload_size payload 大小
 */
typedef void (*mp_rx_event_cb)(
    void                    *user,
    mp_rx_stream_event_t     event,
    const mp_coordinate_t   *coord,
    const mp_rx_coordinator_t* coordinator,
    const mp_rx_slice_state_t *state,
    uint32_t                 old_watermark,
    const uint8_t           *payload,
    uint16_t                 payload_size
);

#pragma endregion

#pragma region RX Stream

/*
 * @brief RX流会话. 同一实例不可被并发.
*/
typedef struct {
    mp_config_t          config;
    mp_rx_coordinator_t  coordinator;
    mp_rx_event_cb       on_event;
    void                *user;
} mp_rx_stream_t;

/*
 * @brief RX流入口, 从这里传入通信数据.
 * @param frame_size 数据块大小. 预期大小为tu, 也支持流式接收.
*/
void mp_rx_stream_feed(
    mp_rx_stream_t *s,
    const uint8_t  *frame,
    uint16_t        frame_size
);

#pragma endregion

#ifdef __cplusplus
}
#endif

#endif