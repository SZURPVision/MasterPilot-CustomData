#ifndef __MP_SESSION_H_
#define __MP_SESSION_H_

#include <masterpilot/customdata-core.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MP_SESSION_SEND_MAGIC  (('M' << 0) | ('P' << 8) | ('S' << 16) | ('d' << 24))
#define MP_SESSION_RECV_MAGIC  (('M' << 0) | ('P' << 8) | ('R' << 16) | ('v' << 24))

/*
 * @brief 将 send session 初始化并写入 stdout
 * @param mtu          MTU 大小
 * @param buffer_count buffer 数量
 * @return true 成功
 */
bool mp_session_send_create(uint16_t mtu, uint8_t buffer_count);

/*
 * @brief 将 recv session 初始化并写入 stdout
 * @param mtu          MTU 大小
 * @param buffer_count buffer 数量
 * @return true 成功
 */
bool mp_session_recv_create(uint16_t mtu, uint8_t buffer_count);

/*
 * @brief 从文件加载 send session
 * @param sender 输出初始化的 sender（含 buffer）
 * @param path   文件路径
 * @return true 成功
 */
bool mp_session_send_load(mp_sender_t *sender, const char *path);

/*
 * @brief 将 send session 保存到文件
 * @param sender 当前 sender 状态
 * @param path   文件路径
 * @return true 成功
 */
bool mp_session_send_save(const mp_sender_t *sender, const char *path);

/*
 * @brief 从文件加载 recv session
 * @param receiver 输出初始化的 receiver（含 buffer）
 * @param path     文件路径
 * @return true 成功
 */
bool mp_session_recv_load(mp_receiver_t *receiver, const char *path);

/*
 * @brief 将 recv session 保存到文件
 * @param receiver 当前 receiver 状态
 * @param path     文件路径
 * @return true 成功
 */
bool mp_session_recv_save(const mp_receiver_t *receiver, const char *path);

/*
 * @brief 释放 session 关联的 buffer
 */
void mp_session_free(mp_config_t cfg);

#ifdef __cplusplus
}
#endif

#endif