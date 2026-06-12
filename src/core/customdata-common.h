#ifndef __MP_CUSTOMDATA_COMMON_H_
#define __MP_CUSTOMDATA_COMMON_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define MP_PURE __attribute__((pure))
#else
    #define MP_PURE
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

/*
 * @brief 4 字节 wire header
 */
#pragma pack(push,1)
typedef struct {
    const uint16_t package_serial       : 8; /**< 消息包序号  */
    const uint16_t slice_serial         : 8; /**< 包分片序号 */
    const uint16_t sender_id            : 3; /**< 发送者id */
    const uint16_t end_of_package       : 1; /**< 终止标志, 1表示终止 */
    const uint16_t slice_payload_size   : 12; /**< 本slice的载荷实际大小 */
} mp_header_t;
#pragma pack(pop)

#pragma endregion

#pragma region Derived & Common Pure Operators

/*
 * @brief 推导单`slice`内最大载荷
 */
MP_PURE
uint16_t mp_max_payload(const mp_config_t config);

/*
 * @brief slice 序号 → 包内 offset
 */
MP_PURE
uint32_t mp_slice_idx_to_offset(const uint16_t slice_idx, const uint16_t max_payload);

/*
 * @brief header 相等判断. 相等返回true
 */
MP_PURE
bool mp_header_equals(const mp_header_t a, const mp_header_t b);

#pragma endregion

#ifdef __cplusplus
}
#endif

#endif