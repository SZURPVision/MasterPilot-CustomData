#ifndef __MP_CUSTOMDATA_HEADER_H_
#define __MP_CUSTOMDATA_HEADER_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 4 字节 wire header
 *
 * Wire layout (little-endian):
 *   byte0:             package_serial(8)
 *   byte1:             slice_serial(8)
 *   byte2:             sender_id(3) | end_of_package(1) | payload_size_hi(4)
 *   byte3:             payload_size_lo(8)
 *
 * @warning 位域结构体仅用作解码后的内部表示, 不可直接 memcpy 到 wire.
 *          序列化/反序列化请使用 mp_header_pack() / mp_header_unpack().
 */
#pragma pack(push,1)
typedef struct {
    uint16_t package_serial       : 8;  /**< 消息包序号 */
    uint16_t slice_serial         : 8;  /**< 包分片序号 */
    uint16_t sender_id            : 3;  /**< 发送者id (0~7) */
    uint16_t end_of_package       : 1;  /**< 终止标志, 1表示终止 */
    uint16_t slice_payload_size   : 12; /**< 本slice的载荷实际大小 */
} mp_header_t;
#pragma pack(pop)

/** @brief wire header 固定大小 */
#define MP_HEADER_SIZE 4

/**
 * @brief 将内部表示打包为 wire bytes
 * @param[out] out 至少 4 字节的输出缓冲区
 * @param[in]  h   内部表示的 header
 */
static inline void mp_header_pack(uint8_t out[MP_HEADER_SIZE], const mp_header_t *h)
{
    out[0] = (uint8_t)h->package_serial;
    out[1] = (uint8_t)h->slice_serial;
    out[2] = (uint8_t)((h->sender_id << 5) | ((h->end_of_package & 1u) << 4)
                       | ((h->slice_payload_size >> 8) & 0xFu));
    out[3] = (uint8_t)(h->slice_payload_size & 0xFFu);
}

/**
 * @brief 从 wire bytes 解包为内部表示
 * @param[out] h  输出的 header
 * @param[in]  in 至少 4 字节的 wire 数据
 */
static inline void mp_header_unpack(mp_header_t *h, const uint8_t in[MP_HEADER_SIZE])
{
    h->package_serial     = in[0];
    h->slice_serial       = in[1];
    h->sender_id          = in[2] >> 5;
    h->end_of_package     = (in[2] >> 4) & 1u;
    h->slice_payload_size = (uint16_t)(((in[2] & 0xFu) << 8) | in[3]);
}

/**
 * @brief 比较两个 header 是否相等
 * @return 全部字段相等时返回 true
 */
static inline bool mp_header_equals(const mp_header_t a, const mp_header_t b)
{
    return (a.package_serial == b.package_serial)
        && (a.slice_serial == b.slice_serial)
        && (a.sender_id == b.sender_id)
        && (a.end_of_package == b.end_of_package)
        && (a.slice_payload_size == b.slice_payload_size);
}

#ifdef __cplusplus
}
#endif

#endif