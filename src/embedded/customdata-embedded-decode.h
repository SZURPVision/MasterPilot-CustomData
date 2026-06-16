#ifndef __MP_CUSTOM_DATA_EMBEDDED_DECODE_H_
#define __MP_CUSTOM_DATA_EMBEDDED_DECODE_H_

#include <stdbool.h>

/**
 * @brief 将自定义数据传输层与`nanopb`解码二合一的实例
 * 
 */
//TODO: 思路: 利用乱序的时间局部特性, 以及乱序的低概率性. 可以用一个栈+倒序遍历来搞. 
// 申请: 压入栈顶
// 查找: 从栈顶向下搜索
// 销毁: 把栈顶移动过来
// GC: Age机制, 从底层向上清除死包, 再触发销毁逻辑
typedef struct {
    void* user;
} mp_pb_decoder_inst_t;

bool mp_pb_decode_feed();

#endif