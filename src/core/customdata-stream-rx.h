#ifndef __MP_CUSTOMDATA_STREAM_RX_H_
#define __MP_CUSTOMDATA_STREAM_RX_H_

#include "customdata-rx.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma region RX Coordinator

/*
 * @brief RX的`坐标定位器`, 根据传入的坐标去操作内存
 */
typedef struct {
    void *ctx;

    const mp_rx_slice_state_t *(*state_get)(void *ctx, mp_coordinate_t coord);
    void (*state_put)(void *ctx, mp_coordinate_t coord, const mp_rx_slice_state_t* state);

	/*
	 * @brief 存入临时的乱序 slice payload.
	 * @param payload 要存入的payload
	*/
    void (*payload_put)(void *ctx, mp_coordinate_t coord,
                        const uint8_t *payload, uint16_t size);
	/*
	 * @brief 取出临时的乱序 slice payload.
	 * @return payload头部只读指针
	*/
    const uint8_t *(*payload_get)(void *ctx, mp_coordinate_t coord);

} mp_rx_coordinator_t;

#pragma endregion

#pragma region RX Data Callback

/*
 * @brief RX数据下游回调. 在无乱序/恢复有序时向下游推送流式数据.
 * @param data 数据块. 无需额外算offset.
 * @param coord 当前数据块对应坐标起点
 * @param eop 是否终止流. 若为true, 表示当前包的切片流已经全部发完.
*/
typedef void (*mp_rx_data_cb)(
    void            *user,
    const uint8_t   *data,
    mp_coordinate_t  coord,
    uint16_t         size,
    bool             eop
);

#pragma endregion

#pragma region RX Stream

/*
 * @brief RX流会话. 同一实例不可被并发.
*/
typedef struct {
    mp_config_t          config;
    mp_rx_coordinator_t  coordinator;
    mp_rx_data_cb        on_data;
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