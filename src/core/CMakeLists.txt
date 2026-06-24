option(BUILD_CORE_SHARED "Build customdata-core as shared library" OFF)
option(MP_FFI_EXPORT "Export inline functions for FFI/shared-library symbol visibility" OFF)

set(CORE_SOURCES
    customdata-stream-tx.c
    customdata-stream-rx.c
    customdata-ffi.c
)

# 当开启 FFI 导出时, 单独对 ffi.c 覆盖 MP_INLINE 为空,
# 使其生成外部符号 (而非 static inline), 便于 ClangSharp/FFI 绑定.
# stream-*.c 仍保持 static inline, 各自持有私有无冲突副本.
if(MP_FFI_EXPORT AND BUILD_CORE_SHARED)
    set_source_files_properties(customdata-ffi.c PROPERTIES
        COMPILE_DEFINITIONS "MP_INLINE="
    )
endif()

if(BUILD_CORE_SHARED)
    add_library(mp-customdata-core SHARED ${CORE_SOURCES})
    set_target_properties(mp-customdata-core PROPERTIES
        OUTPUT_NAME customdata-core
        VERSION ${PROJECT_VERSION}
        SOVERSION 0
    )
	# 严判
	if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
		target_compile_options(mp-customdata-core PRIVATE
			-Wall
			-Wextra
			
			-Wno-narrowing
			
			-Wconversion
			-Wsign-conversion
		)
	endif()
else()
    add_library(mp-customdata-core STATIC ${CORE_SOURCES})
endif()

add_library(mp::customdata::core ALIAS mp-customdata-core)
target_include_directories(mp-customdata-core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>"
        "$<INSTALL_INTERFACE:include>"
)
target_compile_features(mp-customdata-core PRIVATE c_std_99)

if(NOT SRC_DIST)
    install(TARGETS mp-customdata-core
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
    )
    install(FILES 
        customdata-common.h
        customdata-tx.h
        customdata-rx.h
        customdata-stream-tx.h
        customdata-stream-rx.h
        DESTINATION include/masterpilot
    )
endif()
