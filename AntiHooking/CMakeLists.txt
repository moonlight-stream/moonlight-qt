add_library(AntiHooking SHARED
    antihookingprotection.cpp
)

target_include_directories(AntiHooking PRIVATE ${CMAKE_SOURCE_DIR}/libs/windows/include)
target_link_directories(AntiHooking PRIVATE ${CMAKE_SOURCE_DIR}/libs/windows/lib/${MOONLIGHT_WIN_ARCH})
target_link_libraries(AntiHooking PRIVATE detours)
target_compile_definitions(AntiHooking PRIVATE ANTIHOOKING_LIBRARY)
