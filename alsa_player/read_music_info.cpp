/*
 * read_music_info.cpp
 * 读取 mp3_player 写入的歌曲时长信息
 *
 * 编译: g++ read_music_info.cpp -o read_music_info -O2
 *
 * 用法:
 *   ./read_music_info          读取一次并退出
 *   ./read_music_info loop     持续监测，检测到变化时打印
 */

#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <unistd.h>

#define INFO_PATH "/tmp/music_format.cmd"

// 将秒数转换为 分:秒 格式字符串
std::string format_time(int total_seconds)
{
    int minutes = total_seconds / 60;
    int seconds = total_seconds % 60;
    char buf[32];
    snprintf(buf, sizeof(buf), "%d:%02d", minutes, seconds);
    return std::string(buf);
}

// 单次读取
int read_once()
{
    // 先检查文件是否存在
    if (access(INFO_PATH, F_OK) != 0) {
        std::cerr << "[错误] 信息文件不存在: " << INFO_PATH << std::endl;
        std::cerr << "        请确认 mp3_player 已经开始播放。" << std::endl;
        return 1;
    }

    std::ifstream ifs(INFO_PATH);
    if (!ifs.is_open()) {
        std::cerr << "[错误] 无法打开信息文件。" << std::endl;
        return 1;
    }

    std::string line;
    if (std::getline(ifs, line)) {
        if (line.empty()) {
            std::cerr << "[错误] 信息文件为空。" << std::endl;
            return 1;
        }

        try {
            int total_seconds = std::stoi(line);
            std::cout << "歌曲总时长: " << format_time(total_seconds)
                      << " (" << total_seconds << " 秒)" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[错误] 文件内容格式不正确: " << line << std::endl;
            return 1;
        }
    } else {
        std::cerr << "[错误] 信息文件为空。" << std::endl;
        return 1;
    }

    return 0;
}

// 循环监测模式：检测到文件内容变化时自动打印
int read_loop()
{
    std::string last_value;
    bool first = true;

    std::cout << "开始监测 " << INFO_PATH << " (按 Ctrl+C 退出)..." << std::endl;

    while (true) {
        // 检查文件是否存在
        if (access(INFO_PATH, F_OK) == 0) {
            std::ifstream ifs(INFO_PATH);
            if (ifs.is_open()) {
                std::string line;
                if (std::getline(ifs, line) && !line.empty()) {
                    if (line != last_value) {
                        last_value = line;
                        try {
                            int total_seconds = std::stoi(line);
                            std::cout << "[更新] 歌曲总时长: " << format_time(total_seconds)
                                      << " (" << total_seconds << " 秒)" << std::endl;
                        } catch (...) {
                            // 忽略格式错误
                        }
                    }
                }
            }
        } else if (!first) {
            // 文件曾经存在过，现在消失了（播放器退出）
            if (!last_value.empty()) {
                std::cout << "[提示] 播放器已退出，信息文件被清除。" << std::endl;
                last_value.clear();
            }
        }

        first = false;
        sleep(1);  // 每秒检查一次
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc >= 2 && std::string(argv[1]) == "loop") {
        return read_loop();
    } else {
        return read_once();
    }
}
