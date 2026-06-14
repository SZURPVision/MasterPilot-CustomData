#ifndef __MP_CUSTOMDATA_RX_H_
#define __MP_CUSTOMDATA_RX_H_

#include "customdata-common.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma region RX Pure Operators

/*
 * @brief 接收端包组装纯状态
 */
typedef struct {
    uint32_t bitmap[8];      /**< 256位图, 用于判断包是否收齐分片 */
    uint16_t received_count; /**< 已接收大小 */
    uint32_t termination;    /**< 包范围左闭右开区间 [0, termination] 的右端点. 0初始化表示未知 */
    uint32_t watermark;      /**< 包内已连续接收完毕的最大offset */
} mp_rx_slice_state_t;

/*
 * @brief 流状态
*/
typedef enum {
    MP_STREAM_ACCEPT = 0, /**< 接受数据 */
    MP_STREAM_DUPLICATE, /**< 遇到重复数据 */
    MP_STREAM_COMPLETE /**< 流结束 */
} mp_stream_status_t;

typedef struct {
    mp_stream_status_t   status;
    mp_rx_slice_state_t  next_state;
} mp_rx_slice_inst_t;

/*
 * @brief RX流状态转移方程
 */
MP_PURE
mp_rx_slice_inst_t mp_calc_slice_step(
    const mp_rx_slice_state_t state,
    const uint16_t            slice_id,
    const uint16_t            payload_size,
    const uint16_t            max_payload,
    const bool                is_eop
);

/*
 * @brief offset → slice 序号
 */
MP_PURE
uint16_t mp_offset_to_slice_idx(const uint32_t offset, const uint16_t max_payload);

/*
 * @brief 计算终止大小
 */
MP_PURE
uint32_t mp_calc_termination(
    const uint16_t slice_idx,
    const uint16_t payload_size,
    const uint16_t max_payload
);

/*
 * @brief 终止大小 → 总大小
 */
MP_PURE
uint32_t mp_termination_to_total_size(const uint32_t termination);

/*
 * @brief 终止大小 → slice 数
 */
MP_PURE
uint16_t mp_termination_to_slice_count(
    const uint32_t termination,
    const uint16_t max_payload
);

/*
 * @brief 判断是否收齐
 */
MP_PURE
bool mp_is_complete(
    const mp_rx_slice_state_t state,
    const uint16_t            max_payload
);

/*
 * @brief header → 坐标重建
 */
MP_PURE
mp_coordinate_t mp_rx_header_to_coordinate(
    const mp_config_t config,
    const mp_header_t header
);

/**
 * @brief 判断 header 是否标记了 package 终止
 */
MP_PURE
bool mp_rx_is_slice_eop(const mp_header_t header);

/*
 * @brief 决策动作类型
 */
typedef enum {
    MP_RX_ACT_IGNORE,    // 重复 slice, 忽略
    MP_RX_ACT_STORE,     // 乱序, 暂存到 coordinator
    MP_RX_ACT_DELIVER    // 有序/补齐, 将 watermark 区间交付
} mp_rx_action_t;

/*
 * @brief 纯决策: 给定当前 bitmap 状态 + 新 slice + watermark, 返回动作
 */
MP_PURE
mp_rx_action_t mp_rx_classify(
    const mp_rx_slice_state_t *state,
    uint16_t slice_id,
    uint32_t offset,
    uint32_t watermark
);

/*
 * @brief 计算 watermark 推进
 *
 * 从 old_watermark 开始扫描 bitmap 中连续置位的 slice，
 * 返回新的连续 offset。若 termination 已知且到达则返回 termination。
 */
MP_PURE
uint32_t mp_rx_advance_watermark(
    const uint32_t bitmap[8],
    uint32_t       old_watermark,
    uint16_t       max_payload,
    uint32_t       termination
);

#pragma endregion

#ifdef __cplusplus
}
#endif

#endif