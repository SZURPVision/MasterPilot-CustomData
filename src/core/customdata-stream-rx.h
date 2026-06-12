#ifndef __MP_CUSTOMDATA_STREAM_RX_H_
#define __MP_CUSTOMDATA_STREAM_RX_H_

#include "customdata-rx.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma region RX Coordinator -- 乱序 payload 暂存接口

/*
 * @brief 协调器: 仅用于乱序场景暂存/取出 payload
 *
 * 流层维护 bitmap 状态，不乱序时不调用 coordinator。
 */
typedef struct {
    void *ctx;

    /*
     * @brief 暂存乱序 slice 的 payload
     */
    void (*payload_put)(
        void *ctx,
        mp_coordinate_t  coord,
        const uint8_t   *payload,
        uint16_t         size
    );

    /*
     * @brief 取出之前暂存的 payload
     * @return payload 指针, NULL 表示无此坐标数据
     */
    const uint8_t* (*payload_get)(
        void *ctx,
        mp_coordinate_t coord
    );

} mp_rx_coordinator_t;

#pragma endregion

#pragma region RX Data Callback

/*
 * @brief 连续数据就绪回调
 *
 * watermark 推进时调用。offset..offset+size 区间已连续就绪。
 */
typedef void (*mp_rx_data_cb)(
    void        *user,
    const uint8_t *data,
    uint32_t      offset,
    uint16_t      size,
    bool          eop
);

#pragma endregion

#pragma region RX Stream

/*
 * @brief RX 流 — 本地维护 bitmap + watermark
 */
typedef struct {
    mp_config_t          config;
    mp_rx_coordinator_t  coordinator;
    mp_rx_data_cb        on_data;
    void                *user;

	/* 内部状态, 0初始化 */
    mp_rx_slice_state_t  state;
    uint32_t             watermark;
    bool                 completed;
} mp_rx_stream_t;

/*
 * @brief 输入一个完整 slice 帧
 *
 * 不乱序 (offset == watermark):
 *   本地更新 bitmap → 推进 watermark → on_data 直接推送
 *   (不调用 coordinator)
 *
 * 乱序 (offset > watermark):
 *   本地更新 bitmap → payload_put 暂存
 *   后续 slice 补齐缺口后 → payload_get 取出 → on_data 推送
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