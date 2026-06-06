# EnsureGenerated.cmake — 确保 buf 生成产物存在
if(NOT EXISTS "${CUSTOMDATA_GENERATED_DIR}/c/model.pb.h")
    message(FATAL_ERROR
        "[Error] Missing buf generated files at ${CUSTOMDATA_GENERATED_DIR}.\n"
        "  Run 'buf generate' first, or pass -DCUSTOMDATA_GENERATED_DIR=<path>")
endif()