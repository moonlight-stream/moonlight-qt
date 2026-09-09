add_library(h264bitstream STATIC
    h264_nal.c
    h264_stream.c
)

target_include_directories(h264bitstream PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(h264bitstream PRIVATE -std=gnu99)
endif()
