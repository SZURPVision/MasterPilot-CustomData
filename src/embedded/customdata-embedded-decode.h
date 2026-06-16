#ifndef __MP_CUSTOM_DATA_EMBEDDED_DECODE_H_
#define __MP_CUSTOM_DATA_EMBEDDED_DECODE_H_

#include <masterpilot/customdata-stream-rx.h>
#include <stdbool.h>
#include <pb.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 表现层(NanoPb)解码消息定义.
 */
typedef struct {
    const pb_msgdesc_t* fields;       /**< 消息fields */
    void* msg;                         /**< 目标消息存储空间. 可写. */
    void (*on_message)(void* msg, void* user); /**< 解完消息时触发的回调. */
    void* user;                        /**< on_message 回调的 user 参数 */
} mp_pb_output_config_t;

/**
 * @brief 将自定义数据传输层与`nanopb`解码二合一的实例.
 *
 * 嵌入式中面向同一串口缓冲区的串行 feed, 内部使用单状态 coordinator.
 * 通过 mp_pb_decode_init() 一次性构造, 用户传入 payload_put / payload_get.
 */
typedef struct {
    const mp_config_t            config;
    const mp_pb_output_config_t  output_config;

    /* ── 内部状态, 构造后不应手动修改 ── */
    mp_rx_stream_t      stream;
    mp_rx_slice_state_t state;
    bool                last_decode_ok;
} mp_pb_decoder_inst_t;

/**
 * @brief 初始化 decoder 实例.
 *
 * @param inst          未初始化的实例 (config / output_config 需预先填入).
 * @param payload_put   用户提供的 payload 写入回调
 * @param payload_get   用户提供的 payload 读取回调
 */
void mp_pb_decode_init(
    mp_pb_decoder_inst_t     *inst,
    mp_rx_payload_setter_t    payload_put,
    mp_rx_payload_getter_t    payload_get
);

/**
 * @brief 喂入一个帧 (transmission unit). 包完整时自动触发 nanopb 解码.
 * @return true 解码成功或包未完整. false 表示 protobuf 解码失败.
 */
bool mp_pb_decode_feed(
    mp_pb_decoder_inst_t *inst,
    const uint8_t        *block,
    uint16_t              size
);

#ifdef __cplusplus
}
#endif

#endif