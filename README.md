# MasterPilot-CustomData 自定义数据协议

本仓库基于`Protobuf`开发, 用于约定自定义客户端通信协议中的自定义数据.

前排提示: 生成代码已纳入版本控制, 直接submodule拿代码就能用, 不用配环境

通信协议中有几处自定义数据:

| 消息名 | 发送方 | 频率 | 大小 |
| --- | --- | --- | --- |
| `CustomByteBlock` | 机器人 | 50Hz | 300Byte |
| `CustomControl` | 自定义客户端 | 75Hz | 30Byte |

## 数据格式

### `CustomControl` (自定义客户端 -> 机器人)

`[size_sent]` `[size_package]` `[custom_data ...]`

- `size_sent`: **uint8** 数据, 表示此包发送前,数据体`custom_data`已经发送的长度. 第一个分片为0.
- `size_package`: **uint8** 数据, 表示数据体`custom_data`总长度
- `custom_data`: 使用`ProtoBuf`编码出来的`uint8[]`数据

### `CustomByteBlock` (机器人 -> 自定义客户端)
`[size_sent]` `[size_package]` `[custom_data ...]`

- `size_sent`: **小端序 uint16** 数据, 表示此包发送前,数据体`custom_data`的已经发送的长度. 第一个分片为0.
- `size_package`: **小端序 uint16** 数据, 表示数据体`custom_data`的总长度
- `custom_data`: 使用`ProtoBuf`编码出来的`uint8[]`数据

## 通信流程 (重要)

对于电控(C)和视觉(C++)等运行在机器人端的开发者，请遵循以下原则：

1.  不要发送里面的小消息, 要直接发送`proto`中定义的最大消息, 通常是类似于`DroneCustomData`, 按照惯例, 这个消息会放在`proto`文件的最后. 解析时直接反序列化大消息, 再直接读里面的嵌套内容. 编码时, 直接将大消息填好, 再跑序列化.
2.  **序列化与打包**：
    *   机器人发送：先将 `DroneCustomData` 序列化为字节流，计算其长度，填入 2 字节小端序长度帧头，组成 `CustomByteBlock` 发往客户端。
    *   机器人接收：先读取 1 字节长度，按该长度接收字节流，再将其作为 `CustomControl` 的 Payload 进行反序列化。

## 速通教学

### C
添加submodule:
```
git submodule add [仓库url] --recursive
```

添加文件:
- `generated/c`
- `nanopb`

如果是`C99`编译器, 添加宏`PB_C99_STATIC_ASSERT`

编译, 根据自己对应的兵种做适配



## 环境配置

### Nix（推荐）
确保装有direnv和nix, 以及vscode的direnv插件, 在项目根目录执行
```bash
ln -s .envrc.template .envrc
direnv allow
```

### 通用前置

1. 安装`protoc`:
	
	- 手动[下载](https://github.com/protocolbuffers/protobuf/releases) `protoc`, 并加入`path`

	- Debian系可使用`apt install protobuf-compiler`安装
	
	- RedHat系可使用`dnf install protoc`安装

2. 使用`git submodule`添加本仓库

	```shell
	git submodule add [仓库地址] [目标目录]
	```

	**请勿直接拷贝文件, 否则无法自动更新**

### 软开(C#)

1. 在项目中添加nugget包`Grpc.Tools`

	在项目目录执行
	```shell
	dotnet add package Grpc.Tools 
	dotnet add package Google.Protobuf
	```

2. 配置`.csproj`文件, 修改`<ItemGroup>`配置, 配置完成后编译项目会自动生成代码.

```xml
<ItemGroup>
	<!-- 将路径替换为 submodule 实际所在的相对路径 -->
	<Protobuf Include="[仓库根目录]/src/*.proto" GrpcServices="None" />
</ItemGroup>
```

### 电控(C)

1. 安装python3环境, 以及python库`types-protobuf`

```bash
# 提前装好python3
pip install types-protobuf
```

2. 生成代码

```shell
python3 nanopb/generator/nanopb_generator.py src/*.proto -I src -D [生成位置]
```

3. 工程配置：
将生成的 .pb.c 和 .pb.h 文件，以及本仓库 nanopb/ 目录下的核心依赖文件（pb.h, pb_common.h, pb_common.c, pb_encode.h, pb_encode.c, pb_decode.h, pb_decode.c）一并加入到工程中进行编译.

如果使用C99, 记得添加宏`PB_C99_STATIC_ASSERT`

**推荐将代码生成到本仓库/generated, 再通过软链接引用**

### 视觉(C++)

1. 安装`protobuf`的`C++`库, 以`debian系`为例:

```bash
apt install libprotobuf-dev
```

2. 生成代码

```shell
protoc --proto_path=src --cpp_out=[生成位置] src/*.proto
```

3. `CMake`配置: 将生成的`.pb.cc`加入编译, 并链接`protobuf`库

```cmake
find_package(Protobuf REQUIRED)
	include_directories(${Protobuf_INCLUDE_DIRS})

	add_executable(YourTarget main.cpp [生成位置]/custom_control.pb.cc)
	target_link_libraries(YourTarget ${Protobuf_LIBRARIES})
```


## 示例代码(与字节流相互转换)

### C#

利用`Google.Protobuf`的官方扩展方法即可

```csharp
using Google.Protobuf;
using MasterPilot.Communicator.ProtoMessages.Drone; // 引用生成的命名空间

// ==========================================
// 1. 序列化: 对象 -> 字节流 (发送 CustomControl)
// ==========================================
var speedData = new SpeedFactor {
    X = 255, // 满速
    Y = 0    // 静止
};

// 直接转换为 byte 数组 (Payload)
byte[] dataBytes = speedData.ToByteArray();

// 【关键】加上 1 字节长度帧头 (CustomControl: Client -> Robot, uint8)
byte[] sendBytes = new byte[dataBytes.Length + 1];
sendBytes[0] = (byte)dataBytes.Length;
dataBytes.CopyTo(sendBytes, 1);

// 发送 sendBytes...


// ==========================================
// 2. 反序列化: 字节流 -> 对象 (接收 CustomByteBlock)
// ==========================================
byte[] receiveBuffer = ...; // 从串口或网络读到的原始字节流 (包含帧头)

// 【关键】解析 2 字节小端序长度帧头 (CustomByteBlock: Robot -> Client, uint16 LE)
ushort payloadLength = BitConverter.ToUInt16(receiveBuffer, 0);
var receiveBytes = new ReadOnlySpan<byte>(receiveBuffer, 2, payloadLength);

// 从 byte 数组解析回对象
var droneData = DroneCustomData.Parser.ParseFrom(receiveBytes);

// 提取所需数据
if (droneData.GimbalData != null) {
    float yaw = droneData.GimbalData.Yaw;
}
```

### C++

C++ 通常使用 std::string 来作为变长字节流的容器，也可以直接操作裸指针（例如串口编程时常用的 char* 数组）。

```cpp
#include "drone.pb.h"
#include "custom_control.pb.h"
#include <string>

// ==========================================
// 1. 序列化: 对象 -> 字节流 (机器人发送 CustomByteBlock)
// ==========================================
#error "请务必阅读文档：机器人发送 CustomByteBlock 需加 2 字节小端序长度(uint16_t LE)"

DroneCustomData drone_data;
drone_data.mutable_gimbal_data()->set_yaw(1.57f);
drone_data.set_fric_on(true);

// 序列化 payload
std::string payload;
drone_data.SerializeToString(&payload);

// 【关键】加上 2 字节长度帧头 (Robot -> Client)
uint16_t size = static_cast<uint16_t>(payload.size());
std::string send_packet;
send_packet.resize(2 + payload.size());
memcpy(&send_packet[0], &size, 2); // 小端序 uint16
memcpy(&send_packet[2], payload.data(), payload.size());

// 发送 send_packet.data(), 长度为 send_packet.size(), 给电控, 电控直接按字节转发.


// ==========================================
// 2. 反序列化: 字节流 -> 对象 (机器人接收 CustomControl)
// ==========================================
#error "请务必阅读文档：机器人接收 CustomControl 需解析 1 字节长度(uint8_t)"

char receive_buffer[64]; // 包含帧头的原始数据, 由电控按字节转发给C++上位机
// 【关键】解析 1 字节长度帧头 (Client -> Robot)
uint8_t payload_len = static_cast<uint8_t>(receive_buffer[0]);

SpeedFactor speed_data;
// 从 buffer 偏移 1 字节位置开始解析
bool success = speed_data.ParseFromArray(receive_buffer + 1, payload_len);

if (success) {
    uint8_t speed_x = speed_data.x();
}
```


### C

C 语言使用 nanopb。为避免动态内存分配导致单片机死机，我们全程基于固定大小的数组（栈或静态区）进行流操作。

```c
#include "drone.pb.h"
#include "custom_control.pb.h"
#include "pb_encode.h"
#include "pb_decode.h"

// ==========================================
// 1. 序列化: 结构体 -> 字节流 (机器人发送 CustomByteBlock)
// ==========================================
#error "请务必阅读文档：机器人发送 CustomByteBlock 需加 2 字节长度帧头 (uint16 LE)"

uint8_t send_buffer[300]; 

DroneCustomData drone_data = DroneCustomData_init_default;
drone_data.has_gimbal_data = true;
drone_data.gimbal_data.yaw = 1.57f;

// 【关键】创建输出流时偏移 2 字节，留给 uint16 长度帧头
// 这段代码可以理解成把 send_buffer 的第二个位置到末尾这一段内存区域封装成了一个"句柄", 交给nanopb库来写入. 跑完之后send_buffer会被填入数据.
pb_ostream_t ostream = pb_ostream_from_buffer(send_buffer + 2, sizeof(send_buffer) - 2);
bool encode_status = pb_encode(&ostream, DroneCustomData_fields, &drone_data);

if (encode_status) {
    // 写入 2 字节长度 (小端序)
    uint16_t len = (uint16_t)ostream.bytes_written;
    send_buffer[0] = len & 0xFF;
    send_buffer[1] = (len >> 8) & 0xFF;
    
    size_t total_length = ostream.bytes_written + 2;
    // 发送 send_buffer...
}


// ==========================================
// 2. 反序列化: 字节流 -> 结构体 (机器人接收 CustomControl)
// ==========================================
#error "请务必阅读文档：机器人接收 CustomControl 需解析 1 字节长度帧头 (uint8)"

uint8_t receive_buffer[64]; 
// 【关键】解析 1 字节帧头
uint8_t payload_len = receive_buffer[0];

SpeedFactor speed_data = SpeedFactor_init_default;

// 【关键】从 payload 起始位置(偏移 1 字节)开始解码
pb_istream_t istream = pb_istream_from_buffer(receive_buffer + 1, payload_len);
bool decode_status = pb_decode(&istream, SpeedFactor_fields, &speed_data);

if (decode_status) {
    uint8_t speed_x = (uint8_t)speed_data.x;
}
```

## 注意事项

- 务必使用`submodule`+`软链接`引入本项目, 禁止直接复制文件.

- 请自行定义数据处理的`结构体`/`Model`, 不应该直接使用生成的代码来做其他代码逻辑, 不然架构爆炸.

- 修改协议可提issue or pr, 或者飞书交流
