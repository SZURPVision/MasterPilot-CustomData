# FindNanopb
# ----------
# Nix 会将 nanopb 安装到标准 prefix，提供：
#   - include/nanopb/ 下的头文件
#   - lib/ 下的库（如有）
# 本 find module 检测头文件即宣告成功。

if(NOT Nanopb_FOUND)
    find_path(Nanopb_INCLUDE_DIR
        NAMES pb.h
        PATH_SUFFIXES nanopb
    )

    if(Nanopb_INCLUDE_DIR)
        set(Nanopb_FOUND TRUE)
        set(Nanopb_INCLUDE_DIRS "${Nanopb_INCLUDE_DIR}")

        if(NOT TARGET nanopb::nanopb)
            add_library(nanopb::headers INTERFACE IMPORTED)
            set_target_properties(nanopb::headers PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${Nanopb_INCLUDE_DIR}"
            )
        endif()

        if(NOT TARGET nanopb::nanopb)
            add_library(nanopb::nanopb ALIAS nanopb::headers)
        endif()
    endif()

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(Nanopb
        REQUIRED_VARS Nanopb_INCLUDE_DIR
    )

    mark_as_advanced(Nanopb_INCLUDE_DIR)
endif()