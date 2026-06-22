#ifndef __MP_CUSTOMDATA_COMMON_H_
#define __MP_CUSTOMDATA_COMMON_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MP_PURE
    #if defined(__GNUC__) || defined(__clang__)
        #define MP_PURE __attribute__((const))
    #else
        #define MP_PURE
    #endif
#endif

#pragma region Data Models

/*
 * @brief 配置
 */
typedef struct {
    uint16_t transmission_unit; /**< 传输单元大小. 传输链路上, 数据块大小必须严格等于这个值. 不满需要随机填充.
									 这是裁判系统 <-> 自定义客户端 链路上的限制*/
} mp_config_t;

/*
 * @brief 正交坐标, 用于抽象寻址.
 */
typedef struct {
    uint16_t sender_id; /**< 发送者标识符, 用于区分package空间 */
    uint16_t package_id; /**< 包id, 用于区分slice空间 */
    uint32_t offset; /**< 包内地址 */
} mp_coordinate_t;

#pragma endregion

#pragma region Header

#define MP_SENDER_ID_BITS 3
#define MP_PACKAGE_ID_BITS 8
#define MP_SLICE_ID_BITS 8
#define MP_EOP_BITS 1
#define MP_SLICE_PAYLOAD_SIZE_BITS 12

#define MP_SENDER_ID_MAX ((1<<MP_SENDER_ID_BITS)-1)
#define MP_PACKAGE_ID_MAX ((1<<MP_PACKAGE_ID_BITS)-1)
#define MP_SLICE_ID_MAX ((1<<MP_SLICE_ID_BITS)-1)

/**
 * @brief header 元数据. 表示header解析后的内容.
 */
typedef struct {
    uint8_t package_serial       ;  /**< 消息包序号 */
    uint8_t slice_serial         ;  /**< 包分片序号 */
    uint8_t sender_id            ;  /**< 发送者id, 实际只用3bit */
    bool eop                     ;  /**< 终止标志, 1表示终止 */
    uint16_t slice_payload_size   ; /**< 本slice的载荷实际大小. 实际只用12个bit. */
} mp_header_meta_t;

/**
 * @brief header在通信过程中的压缩形式
*/
typedef uint32_t mp_header_packed_t;

/** @brief wire header 固定大小 */
#define MP_HEADER_SIZE sizeof(mp_header_packed_t)

#define MP_PACKED_HEADER_TO_ARRAY(packed)  \
    (uint8_t[]){                        \
        (uint8_t)((packed) >> 0), \
        (uint8_t)((packed) >> 8), \
        (uint8_t)((packed) >> 16),\
        (uint8_t)((packed) >> 24) \
    }

#define MP_ARRAY_TO_PACKED_HEADER(arr) (                        \
    ((mp_header_packed_t)((uint8_t*)(arr))[0] << 0)  |          \
    ((mp_header_packed_t)((uint8_t*)(arr))[1] << 8)  |          \
    ((mp_header_packed_t)((uint8_t*)(arr))[2] << 16) |          \
    ((mp_header_packed_t)((uint8_t*)(arr))[3] << 24)            \
)

/**
 * @brief 将header_meta打包为通信使用的header_packed
 */
MP_PURE
inline mp_header_packed_t mp_header_pack(const mp_header_meta_t meta)
{
    return ((uint32_t)(meta.package_serial)       << 0)  |
           ((uint32_t)(meta.slice_serial)         << 8)  |
           ((uint32_t)(meta.sender_id & 0x07)     << 16) |
           ((uint32_t)(meta.eop ? 1 : 0)          << 19) |
           ((uint32_t)(meta.slice_payload_size & 0x0fff) << 20);
}


/**
 * @brief 从通信中的header_packed解包为header_meta, 方便计算使用
 */
MP_PURE
inline mp_header_meta_t mp_header_unpack(const mp_header_packed_t packed)
{
    return (mp_header_meta_t) {
        .package_serial     = (uint8_t)(packed & 0xff),
        .slice_serial       = (uint8_t)((packed >> 8) & 0xff),
        .sender_id         = (uint8_t)((packed >> 16) & 0x07),
        .eop               = (bool)((packed >> 19) & 0x01),
        .slice_payload_size = (uint16_t)((packed >> 20) & 0x0fff)
    };
}

#define MP_PACKED_HEADER_FROM_ARRAY(array)  \
    {                        \
        [0] = (packed) >> 0, \
        [1] = (packed) >> 8, \
        [2] = (packed) >> 16,\
        [3] = (packed) >> 24 \
    }

/**
 * @brief 比较两个 header 是否相等
 * @return 全部字段相等时返回 true
 */
MP_PURE
inline bool mp_header_equals(const mp_header_meta_t a, const mp_header_meta_t b)
{
    return (a.package_serial == b.package_serial)
        && (a.slice_serial == b.slice_serial)
        && (a.sender_id == b.sender_id)
        && (a.eop == b.eop)
        && (a.slice_payload_size == b.slice_payload_size);
}

#pragma endregion

#pragma region Derived & Common Pure Operators

/*
 * @brief 推导单`slice`内最大载荷
 */
MP_PURE
inline uint16_t mp_max_payload(const mp_config_t config)
{
    return config.transmission_unit - MP_HEADER_SIZE;
}

/*
 * @brief slice 序号 → 包内 offset
 */
MP_PURE
inline uint32_t mp_slice_idx_to_offset(const uint16_t slice_idx, const uint16_t max_payload)
{
    return (uint32_t)slice_idx * max_payload;
}

MP_PURE
inline uint16_t mp_offset_to_slice_idx(const uint32_t offset, const uint16_t max_payload)
{
    return offset / max_payload;
}

#pragma endregion

#ifdef __cplusplus
}
#endif

#endif