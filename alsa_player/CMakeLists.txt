cmake_minimum_required(VERSION 3.10)
project(alsa_mp3_player)

# 设置交叉编译器
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)

# 设置路径变量
set(MPG123 "$ENV{HOME}/mpg123_arm_install")
set(ALSA_SYSROOT "/opt/fsl-imx-x11/4.1.15-2.1.0/sysroots/cortexa7hf-neon-poky-linux-gnueabi")

# 设置 sysroot
set(CMAKE_SYSROOT ${ALSA_SYSROOT})

# 添加头文件路径
include_directories(
    ${MPG123}/include
    ${ALSA_SYSROOT}/usr/include
)

# 添加库搜索路径
link_directories(
    ${ALSA_SYSROOT}/usr/lib
    ${ALSA_SYSROOT}/lib
)

# 编译选项
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")

# 生成可执行文件
add_executable(alsa_mp3_player_armV2  mp3_player.c)

# 链接 mpg123 静态库
target_link_libraries(alsa_mp3_player_armV2
    ${MPG123}/lib/libmpg123.a
)

# 链接系统库
target_link_libraries(alsa_mp3_player_armV2
    asound
    m
    pthread
    dl
    rt
)