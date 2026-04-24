# MasterPilot-CustomData 自定义数据协议

本仓库基于`Protobuf`开发, 用于约定自定义客户端通信协议中的自定义数据.

前排提示: 生成代码已纳入版本控制, 直接submodule拿代码就能用, 不用配环境

通信协议中有几处自定义数据:

| 消息名 | 发送方 | 频率 | 大小限制 |
| --- | --- | --- | --- |
| `CustomControl` | 自定义客户端 | 75Hz | 30Byte |
| `CustomByteBlock` | 机器人 | 50Hz | 300Byte |

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
// using MasterPilot.ProtoMessage.CustomData; // 引用 proto 中定义的 namespace (如果有)

// ==========================================
// 1. 序列化: 对象 -> 字节流 (发送 SpeedFactor)
// ==========================================
var speedData = new SpeedFactor {
    X = 255, // 满速
    Y = 0    // 静止
};

// 直接转换为 byte 数组
byte[] sendBytes = speedData.ToByteArray();
// 此处将 sendBytes 发送到网络或串口


// ==========================================
// 2. 反序列化: 字节流 -> 对象 (接收 RegionDetection)
// ==========================================
byte[] receiveBytes = /* 从底层读取到的数据 */;

// 从 byte 数组解析回对象
var regionData = RegionDetection.Parser.ParseFrom(receiveBytes);

// 提取所需数据
bool inBase = regionData.InBase;
// Console.WriteLine($"是否在基地内: {inBase}");
```

### C++

C++ 通常使用 std::string 来作为变长字节流的容器，也可以直接操作裸指针（例如串口编程时常用的 char* 数组）。

```cpp
#include "custom_control.pb.h"
#include "custom_byte_block.pb.h"
#include <string>

// ==========================================
// 1. 序列化: 对象 -> 字节流 (发送 RegionDetection)
// ==========================================
RegionDetection region_data;
region_data.set_in_base(true); // 赋值

// 转换为 std::string 以便获取字节流
std::string send_bytes;
region_data.SerializeToString(&send_bytes);

// 若要通过串口发送，可通过以下方式获取裸指针和长度：
// const char* data_ptr = send_bytes.data();
// size_t data_length = send_bytes.size();


// ==========================================
// 2. 反序列化: 字节流 -> 对象 (接收 SpeedFactor)
// ==========================================
char receive_buffer[30]; // 假设这是从串口或网络读到的裸数组数据
size_t received_length = /* 实际接收到的有效长度 */;

SpeedFactor speed_data;

// 直接从裸数组解析
bool success = speed_data.ParseFromArray(receive_buffer, received_length);

if (success) {
    // 解析成功，读取数据 (0~255直接强转为uint8_t)
    uint8_t speed_x = speed_data.x();
    uint8_t speed_y = speed_data.y();
}
```


### C

C 语言使用 nanopb。为避免动态内存分配导致单片机死机，我们全程基于固定大小的数组（栈或静态区）进行流操作。

```c
#include "custom_control.pb.h"
#include "custom_byte_block.pb.h"
#include "pb_encode.h"
#include "pb_decode.h"

// ==========================================
// 1. 序列化: 结构体 -> 字节流 (发送 RegionDetection)
// ==========================================
uint8_t send_buffer[30]; // 分配足够大的定长数组
size_t message_length;

// 【关键】必须使用 _init_default 进行初始化
RegionDetection region_data = RegionDetection_init_default; 
region_data.in_base = true; // 赋值

// 创建输出流并进行编码
pb_ostream_t ostream = pb_ostream_from_buffer(send_buffer, sizeof(send_buffer));
bool encode_status = pb_encode(&ostream, RegionDetection_fields, &region_data);

if (encode_status) {
    message_length = ostream.bytes_written; // 最终实际占用的有效字节数
    // 将 send_buffer 的前 message_length 个字节通过串口发出
}


// ==========================================
// 2. 反序列化: 字节流 -> 结构体 (接收 SpeedFactor)
// ==========================================
uint8_t receive_buffer[30] = {0}; // 接收缓存区
size_t received_length = /* 实际收到的长度 */;

// 【关键】必须使用 _init_default 进行初始化
SpeedFactor speed_data = SpeedFactor_init_default;

// 创建输入流并解码
pb_istream_t istream = pb_istream_from_buffer(receive_buffer, received_length);
bool decode_status = pb_decode(&istream, SpeedFactor_fields, &speed_data);

if (decode_status) {
    // 解码成功，直接读取结构体
    uint8_t speed_x = (uint8_t)speed_data.x;
    uint8_t speed_y = (uint8_t)speed_data.y;
}
```

## 注意事项

- 务必使用`submodule`+`软链接`引入本项目, 禁止直接复制文件.

- 请自行定义数据处理的`结构体`/`Model`, 不应该直接使用生成的代码来做其他代码逻辑, 不然架构爆炸.

- 修改协议可提issue或pr, 或者飞书交流
