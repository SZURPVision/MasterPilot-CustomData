/*
 * customdata-ffi.c — FFI / 共享库导出桩
 *
 * 本文件是唯一将 MP_INLINE 标记函数实例化为外部符号的编译单元.
 *
 * 编译选项:
 *   常规构建 (STATIC/SHARED 均默认):
 *     MP_INLINE 展开为 static inline → header-only 零开销抽象.
 *
 *   FFI 导出模式 (cmake -DMP_FFI_EXPORT=ON -DBUILD_CORE_SHARED=ON):
 *     cmake 通过 set_source_files_properties 对 本文件 单独定义
 *     MP_INLINE= (空宏), 使得所有 MP_INLINE 函数变为普通函数声明,
 *     从而在此编译单元中生成外部符号并导出到共享库.
 *     stream-rx.c / stream-tx.c 不受影响, 仍保有 static inline 副本.
 *
 *   ClangSharp 使用:
 *     对启用了 MP_FFI_EXPORT 构建出的 .so/.dll 运行 ClangSharpPInvokeGenerator,
 *     即可自动扫描并生成 C# P/Invoke 绑定.
 */

#include "customdata-common.h"
#include "customdata-rx.h"
#include "customdata-tx.h"
#include "customdata-stream-rx.h"
#include "customdata-stream-tx.h"
