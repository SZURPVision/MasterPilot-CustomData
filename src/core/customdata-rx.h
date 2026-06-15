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
 * @brief offset → slice 序号
 */
MP_PURE
inline uint16_t mp_offset_to_slice_idx(const uint32_t offset, const uint16_t max_payload)
{
    return (uint16_t)(offset / max_payload);
}

/*
 * @brief 计算终止大小
 */
MP_PURE
inline uint32_t mp_calc_termination(
    const uint16_t slice_idx,
    const uint16_t payload_size,
    const uint16_t max_payload
)
{
    return mp_slice_idx_to_offset(slice_idx, max_payload) + payload_size;
}

/*
 * @brief 终止大小 → slice 数
 */
MP_PURE
inline uint16_t mp_termination_to_slice_count(
    const uint32_t termination,
    const uint16_t max_payload
)
{
    return (uint16_t)((termination + max_payload - 1) / max_payload);
}

/*
 * @brief 判断是否收齐
 */
MP_PURE
inline bool mp_is_complete(
    const mp_rx_slice_state_t* state,
    const uint16_t            max_payload
)
{
    return state->termination > 0
        && state->received_count >= mp_termination_to_slice_count(state->termination, max_payload);
}

/*
 * @brief header → 坐标重建
 */
MP_PURE
inline mp_coordinate_t mp_rx_header_to_coordinate(
    const mp_config_t config,
    const mp_header_meta_t header
)
{
    const uint16_t max_p = mp_max_payload(config);
    return (mp_coordinate_t){
        .sender_id  = header.sender_id,
        .package_id = header.package_serial,
        .offset     = mp_slice_idx_to_offset(header.slice_serial, max_p)
    };
}

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
inline mp_rx_action_t mp_rx_classify(
    const mp_rx_slice_state_t* state,
    uint16_t slice_id,
    uint32_t offset
)
{
    const uint8_t  arr_idx  = (uint8_t)(slice_id >> 5);
    const uint32_t bit_mask = (uint32_t)(1u << (slice_id & 31));

    if (state->bitmap[arr_idx] & bit_mask)
        return MP_RX_ACT_IGNORE;

    if (offset > state->watermark)
        return MP_RX_ACT_STORE;

    return MP_RX_ACT_DELIVER;
}

/*
 * @brief 计算 watermark 推进
 *
 * 从 old_watermark 开始扫描 bitmap 中连续置位的 slice，
 * 返回新的连续 offset。若 termination 已知且到达则返回 termination。
 */
MP_PURE
inline uint32_t mp_rx_advance_watermark(
    const mp_rx_slice_state_t* state,
    uint16_t       max_payload
)
{
    uint32_t wm = state->watermark;
    const uint32_t termination = state->termination;
    const uint32_t* bitmap = state->bitmap;

    while (1) {
        if (termination > 0 && wm >= termination) break;

        const uint16_t  slice_id = mp_offset_to_slice_idx(wm, max_payload);
        const uint8_t   arr_idx  = (uint8_t)(slice_id >> 5);
        const uint32_t  bit_mask = (uint32_t)(1u << (slice_id & 31));

        if (!(bitmap[arr_idx] & bit_mask)) break;

        wm += max_payload;
    }

    if (termination > 0 && wm > termination) {
        wm = termination;
    }

    return wm;
}

/*
 * @brief 计算单个交付切片的大小
 *
 * 根据当前 offset、区间右端点 new_wm 和 max_payload，返回本切片应交付的字节数。
 * 尾部切片会被 new_wm 自动截断。
 */
MP_PURE
inline uint16_t mp_rx_deliver_size(uint32_t cur_off, uint32_t new_wm, uint16_t max_payload)
{
    if (cur_off + max_payload > new_wm)
        return (uint16_t)(new_wm - cur_off);
    return max_payload;
}

/*
 * @brief RX流状态转移方程
 */
MP_PURE
inline mp_rx_slice_inst_t mp_calc_slice_step(
    const mp_rx_slice_state_t* state,
    const uint16_t            slice_id,
    const uint16_t            payload_size,
    const uint16_t            max_payload,
    const bool                is_eop
)
{
    const uint8_t  arr_idx  = (uint8_t)(slice_id >> 5);
    const uint32_t bit_mask = (uint32_t)(1u << (slice_id & 31));

    /* Duplicate check */
    if (state->bitmap[arr_idx] & bit_mask) {
        return (mp_rx_slice_inst_t){
            .status     = MP_STREAM_DUPLICATE,
            .next_state = *state
        };
    }

    /* Construct new pure state */
    uint32_t new_bitmap[8];
    for (int i = 0; i < 8; ++i) {
        new_bitmap[i] = state->bitmap[i];
    }
    new_bitmap[arr_idx] |= bit_mask;

    uint32_t new_term = state->termination;
    if (payload_size < max_payload || is_eop) {
        new_term = mp_calc_termination(slice_id, payload_size, max_payload);
    }

    /* Empty terminal slice does not count toward received_count */
    const uint16_t new_count = (payload_size == 0 && is_eop)
        ? state->received_count
        : state->received_count + 1;

    mp_rx_slice_state_t next_state = {
        .bitmap         = { new_bitmap[0], new_bitmap[1], new_bitmap[2], new_bitmap[3],
                            new_bitmap[4], new_bitmap[5], new_bitmap[6], new_bitmap[7] },
        .received_count = new_count,
        .termination    = new_term,
        .watermark      = state->watermark
    };

    mp_stream_status_t new_status = mp_is_complete(&next_state, max_payload)
        ? MP_STREAM_COMPLETE : MP_STREAM_ACCEPT;

    return (mp_rx_slice_inst_t){
        .status     = new_status,
        .next_state = next_state
    };
}

#pragma endregion

#ifdef __cplusplus
}
#endif

#endif