# CustomData 纯函数框架设计思路

## 核心理念

整个 CustomData 库围绕一个简单想法构建：**把数据传输层的决策逻辑变成纯函数，把副作用推到最外层**。

纯函数意味着：相同输入永远产生相同输出，不读不写外部状态。这样的函数可以随意测试、替换、形式化验证，而不用担心隐式依赖。

## 分层哲学

```
┌─────────────────────────────────┐
│  Tool / Nanopb / Protobuf       │  ← 副作用封装层 (IO, 内存分配, 状态持久化)
├─────────────────────────────────┤
│  Stream (customdata-stream-*)   │  ← 流控制 (累积/切片/乱序暂存)
├─────────────────────────────────┤
│  Core Pure (customdata-tx/rx)   │  ← 纯数学决策 (无状态, 无IO)
├─────────────────────────────────┤
│  Common (customdata-common)     │  ← 共享类型与基础运算
└─────────────────────────────────┘
```

关键约束：**上层可以依赖下层，下层绝不可依赖上层**。Stream 层调用 Core 层，Core 层只做数学运算。

## 坐标系统

```
(sender_id, package_id, offset)
```

- `sender_id`：发送者身份。同一发送者的数据流内部有序，不同发送者间独立。
- `package_id`：Package 序号。一个 Package 是一段连续数据的容器。Tool 层每次 tx 调用产生一个 Package。
- `offset`：Package 内字节偏移。**这是唯一由 Core 纯函数推进的维度**。每发一个 Slice，offset 步进 `max_payload`。

### 为什么需要坐标

**纯函数化**如果按照传统思路设计, 那么坐标这些参数应该是存在于可变的状态机里. 将这三个核心要素抽离出来并作为不可变的坐标, 就可以不依赖于实时状态进行各种推演, 从而直接消灭bug.

**内存访问抽象化**坐标可以让内存访问抽象化, 实现不再绑死环形缓冲区. 这样上层的封装就可以有更好的选择, 采用小的环形缓冲区, 大数组映射, 文件IO映射, 哈希等等. 最后库不需要去管理内存, 只需要在各个位置拿过来, 往后推流. 而上层封装则可以使用对应语言最擅长的内存管理机制和数据结构.

**隔离和并发**坐标可以直接隔离不同package, 不同sender的状态, 不用去管理恶心的并发.

### sender_id 和 package_id 由谁管

- `sender_id`：Tool/Session 层设置，整个流生命周期不变。Header 里只存 3 bit（0~7），够用。
- `package_id`：Tool 层在每个 Package 结束后递增。Header 里只存低 8 位（0~255），够区分相邻的 Package。

Core 纯函数**不改变这两者**。`mp_tx_advance` 只推进 offset。这是一种有意的约束：Core 只管"在一个固定 identity 的流里往前走"，identity 切换是上层的事。

### offset 的步进规则

`mp_tx_advance(config, current)` 将 offset 增加一个 `max_payload`。

这保证了核心不变量：**每调用一次 `mp_tx_prepare` → 产生一个 Slice → offset 步进一个 slot → `slice_serial` 递增 1**。

为什么不是加到实际的 payload_size？因为每个 Slice 占用固定大小的传输块（`transmission_unit` 字节），无论实际 payload 多大。offset 步进的是"占用"，不是"发送"。

## Header 设计

### 4 字节紧凑布局

```
byte 0-1: package_serial(8) | slice_serial(8)
byte 2-3: sender_id(3) | eop(1) | payload_size(12)
```

### 为什么用 eop bit 而不是只靠 payload_size

旧设计依赖 `payload_size < max_payload` 判断终止。这有一个边界问题：当数据正好是 `max_payload` 的整数倍时，最后一个 Slice 也是满的，接收端分不清它是中间 Slice 还是终止 Slice。

eop (end-of-package) bit 显式标记终止。从 sender_id 扣出 1 bit（4→3），代价很小（8 个 sender 够用），收益很大（终止判定不需要发空包）。

### 双保险

实际终止判定是 `eop == 1 || payload_size < max_payload`。这意味着即使 eop bit 在传输中翻转，只要 payload 不满也能正确终止。乱序到达时，任一条件命中即可。

### 为什么 mp_tx_make_header 需要 is_final 参数

`is_final` 由调用者传入——只有调用者知道"后面还有没有数据"。Core 层不知道自己处于流中的位置，它只负责把"这是最后一个"这个事实编码到 header 中。

## TX 纯函数族

### mp_tx_advance

最简函数：offset += max_payload。这是唯一的坐标变化操作。单独拆出来是为了让 Stream 层可以只推进不编码（比如累积数据时）。

### mp_tx_make_header

纯映射：坐标 + payload 大小 + 终止标志 → 4 字节 header。无状态，无 IO，可独立测试。

### mp_tx_prepare

组合函数：make_header + advance。这是最常用的入口——"给我准备好下一个 Slice 的一切信息，并告诉我发完后坐标在哪"。

返回的 `mp_tx_slice_t` 里：
- `header` 反映**发送前**的坐标（这个 Slice 的身份）
- `next` 反映**发送后**的坐标（供下次调用）

这样下游的逻辑非常直白：拿 header 编码，拿 next 推进。

## RX 纯函数族

### mp_calc_slice_step

接收端的核心纯函数。输入旧状态 + 新 Slice 信息 → 输出新状态 + 判定结果。

这个函数是纯的意味着：给定相同的 bitmap 状态和相同的 Slice，永远产生相同的判定。重复 Slice 永远返回 DUPLICATE，不会因为调用顺序不同而产生不同结果。

### is_eop 参数

和 TX 端对称：`is_eop` 告诉函数这个 Slice 是否是终止 Slice。如果是，即使 payload_size == max_payload 也会计算 termination。空终止 Slice（payload_size == 0, is_eop == true）不计入 received_count——它只是一个标记，不是数据。

### mp_is_complete

termination 已知 + 收到的 Slice 数 ≥ 需要的 Slice 数 → 包收齐。

### mp_rx_advance_watermark

这是乱序容忍的核心。从当前 watermark 开始，扫描 bitmap 中连续置位的 Slice，返回新的连续 offset。

纯粹的位图扫描，不涉及任何外部状态。watermark 推进意味着"这些 Slice 已连续就绪，可以交付上层了"。

### mp_rx_is_slice_eop

双重判定：`eop == 1 || payload_size < max_payload`。这是接收端"知道包已结束"的全部依据。

## Stream 层：薄胶水

### TX Stream 职责

`mp_tx_encode_stream` 只做两件事：
1. 把输入数据切成 `max_payload` 大小的块
2. 对每块调 `mp_tx_prepare`，把结果推给回调

不做数据缓存。调用者负责数据准备（Tool 层用 accumulation buffer 模拟 Nanopb 的小块产出行为）。

空终止 Slice 由 stream 层自动插入：当 `is_final && size == 0` 时，emit 一个 payload=0, eop=1 的 Slice。这解决了精确倍数无终止标记的边界问题。

### RX Stream 职责

`mp_rx_stream_feed` 维护本地 bitmap 状态 + watermark：
- 不乱序：直接推进 watermark，连续区域直接交付
- 乱序：payload 暂存到 coordinator，等空缺填满后再交付

coordinator 是一个简单的接口结构体（`payload_put` / `payload_get`），负责乱序暂存。流层不关心暂存的具体实现（可以是哈希表、链表、内存池）。

不乱序时不调用 coordinator——这是性能关键路径的优化。

## Tool 层：最小化

Tool 层的 tx 命令只做一件事：从 stdin 累积数据到 `max_payload`，满一片就喂给 `mp_tx_encode_stream`。

`-b` 参数控制 stdin 读取粒度，模拟 Nanopb 的小 buffer 输出。这不是 Core 或 Stream 层需要关心的事——它是 Tool 层的模拟策略。

rx 命令同样极简：读 block → 解析 header → `mp_calc_slice_step` → 写 assembly buffer → 完整就输出。

## 为什么不缓存但保持正确性

整个 TX 流程中没有"等等再发"的缓存逻辑。每个 Slice 一旦累积满立刻发出。为什么这不会导致问题？

因为 offset 的步进与数据消费同步：每发一个 Slice，offset 前进一个 slot。`slice_serial = offset / max_payload` 自动递增。Tool 层的 accumulation buffer 只是把上游的碎片聚合成完整 Slice——它不是"缓存以后再发"，而是"数据还没凑够一个 Slice"。

真正的幂等性保证来自接收端：重复 Slice 被 bitmap 查重拦截，乱序 Slice 被暂存等待补齐。发送端不需要为可靠性做任何额外工作。

## 测试覆盖的设计意图

`test/tools-test.sh` 的每个测试都在验证一个设计决策：

| 测试 | 验证的设计决策 |
|------|---------------|
| single-packet | 基本编码/解码闭环 |
| multi-packet | package_id 递增正确，session 保存/恢复正常 |
| exact-multiple | 空终止 Slice 机制（精确倍数边界） |
| binary-round-trip | 二进制数据无损，无截断 |
| tiny-stdin-buf | Nanopb 的最坏情况模拟：碎片累积正确 |
| slice-reorder | bitmap + watermark 乱序恢复 |
| duplicate-blocks | 查重机制（重复 Slice 被忽略） |
| mixed-reorder | 跨 Package 乱序：不同 package_id 的状态隔离 |
| empty-input | 空输入不崩溃，不产生垃圾输出 |