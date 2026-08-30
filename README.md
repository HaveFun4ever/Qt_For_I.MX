# Qt_For_I.MX

基于正点原子 **I.MX6ULL** 嵌入式开发板的综合 Qt 应用合集，集成了**相机、音乐播放器、经典扫雷游戏**三个模块。项目使用 Qt 5 / C / C++ 开发，交叉编译后运行在开发板的 Linux 系统（Yocto，`fsl-imx-x11 4.1.15-2.1.0` SDK）上。

## 功能模块

| 模块 | 说明 |
| ---- | ---- |
| `camera/` | 相机应用：基于 V4L2 的摄像头采集与实时预览、拍照、相册浏览、H.264 视频录制（FFmpeg 编码）、LED 灯控制 |
| `mp3_player/` | 基于 Qt Widgets 的 MP3 播放器界面：通过 `QProcess` 调用底层播放器，支持播放列表、进度条与时间显示、暂停/继续、循环播放 |
| `alsa_player/` | 基于 ALSA + mpg123 的 C 语言 MP3 播放器（CMake 交叉编译工程），包含 ARM 版本及音乐信息读取工具 |
| `mines_sweeper-master/` | 经典扫雷游戏（Qt 5.6.1 / C++，基于 QGraphicsView 图形框架），支持自定义棋盘宽高与地雷数量、自动寻路扩展、插旗/问号标记 |

## 开发环境

- 开发板：正点原子 I.MX6ULL（Cortex-A7）
- 交叉编译 SDK：`fsl-imx-x11 4.1.15-2.1.0`（sysroot：`cortexa7hf-neon-poky-linux-gnueabi`）
- 主机工具：Qt 5.6.1 / Qt Creator、CMake、`arm-linux-gnueabihf-gcc`
- 依赖库：ALSA（libasound）、mpg123、FFmpeg（libavformat / libavcodec / libswscale / libswresample）

## 编译与部署

1. 克隆代码后，根据本机环境修改相关路径：
   - `camera/camera.pro`：`INCLUDEPATH` 与 `LIBS` 中的 SDK sysroot 路径；
   - `alsa_player/CMakeLists.txt`：交叉编译器、`ALSA_SYSROOT` 与 `mpg123` 安装路径。
2. 在主机上交叉编译各模块，得到 ARM 可执行文件。
3. 将可执行文件拷贝到开发板（如通过 NFS / TFTP / U 盘），在板端 Linux 上运行。

## 致谢

- 扫雷游戏部分的思路参考了王桂林老师的项目课程；
- 相机部分基于正点原子（ALIENTEK）video_server 示例工程改造。
