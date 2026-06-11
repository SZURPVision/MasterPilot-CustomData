using System.Reflection;
using System.Runtime.InteropServices;

namespace MasterPilot.Communicator.CustomData;

/// <summary>
/// 模块初始化器：在程序集加载时设置 DllImport 解析器，
/// 将空库名映射到 libcustomdata-core.so / libcustomdata-core.dylib。
/// </summary>
internal static class NativeResolver
{
    [System.Runtime.CompilerServices.ModuleInitializer]
    internal static void Initialize()
    {
        NativeLibrary.SetDllImportResolver(
            typeof(NativeResolver).Assembly,
            ResolveCore
        );
    }

    private static IntPtr ResolveCore(
        string libraryName,
        Assembly assembly,
        DllImportSearchPath? searchPath)
    {
        if (libraryName == "")
        {
            if (NativeLibrary.TryLoad("customdata-core", assembly, searchPath, out IntPtr handle))
                return handle;
        }

        return IntPtr.Zero;
    }
}