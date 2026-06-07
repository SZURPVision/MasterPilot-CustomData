#ifndef __MP_CUSTOM_DATA_NANOPB_H_
#define __MP_CUSTOM_DATA_NANOPB_H_

#include "customdata-core.h"
#include <pb.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * @brief 直接从nanopb结构体编码到缓冲区
 * @param fields nanopb生成的消息名_fields结构体
 * @param message 要发送的 nanopb消息结构体
 * @return 是否成功
*/
bool MP_Encode(mp_sender_t *sender, const pb_msgdesc_t *fields, const void *message);

/*
 * @brief 直接从缓冲区解码到nanopb消息结构体, 并释放相应缓冲区
 * @param fields nanopb生成的消息名_fields结构体
 * @param message 目标nanopb消息结构体, 解码完成可后读取内容
 * @return 是否成功
*/
bool MP_Decode(mp_receiver_t *receiver, const pb_msgdesc_t *fields, void *message);

#ifdef __cplusplus
}
#endif

#endif