/*
 * read_music_info.c
 * 使用 open/read 系统调用读取 mp3_player 写入的歌曲时长信息
 *
 * 编译: gcc read_music_info.c -o read_music_info_c -O2
 *
 * 用法:
 *   ./read_music_info_c          读取一次并退出
 *   ./read_music_info_c loop     持续监测，检测到变化时打印
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define INFO_PATH "/tmp/music_format"
#define BUF_SIZE 32

// 将秒数转换为 分:秒 格式
void format_time(int total_seconds, char *out, size_t out_len)
{
    int minutes = total_seconds / 60;
    int seconds = total_seconds % 60;
    snprintf(out, out_len, "%d:%02d", minutes, seconds);
}

// 单次读取
int read_once(void)
{
    int fd = open(INFO_PATH, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "[错误] 无法打开信息文件 '%s'\n", INFO_PATH);
        fprintf(stderr, "        请确认 mp3_player 已经开始播放。\n");
        return 1;
    }

    char buf[BUF_SIZE];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (n <= 0) {
        fprintf(stderr, "[错误] 信息文件为空。\n");
        return 1;
    }

    // read 不保证以 \0 结尾，需要手动添加
    buf[n] = '\0';
    // 去掉末尾可能存在的换行符
    buf[strcspn(buf, "\n")] = '\0';

    int total_seconds = atoi(buf);
    char time_str[16];
    format_time(total_seconds, time_str, sizeof(time_str));

    printf("歌曲总时长: %s (%d 秒)\n", time_str, total_seconds);
    return 0;
}

// 循环监测模式：检测到文件内容变化时自动打印
int read_loop(void)
{
    char last_value[BUF_SIZE] = {0};
    int first = 1;

    printf("开始监测 %s (按 Ctrl+C 退出)...\n", INFO_PATH);

    while (1) {
        if (access(INFO_PATH, F_OK) == 0) {
            int fd = open(INFO_PATH, O_RDONLY);
            if (fd >= 0) {
                char buf[BUF_SIZE];
                ssize_t n = read(fd, buf, sizeof(buf) - 1);
                close(fd);

                if (n > 0) {
                    buf[n] = '\0';
                    buf[strcspn(buf, "\n")] = '\0';

                    if (strcmp(buf, last_value) != 0) {
                        strncpy(last_value, buf, sizeof(last_value) - 1);
                        last_value[sizeof(last_value) - 1] = '\0';

                        int total_seconds = atoi(buf);
                        char time_str[16];
                        format_time(total_seconds, time_str, sizeof(time_str));

                        printf("[更新] 歌曲总时长: %s (%d 秒)\n", time_str, total_seconds);
                    }
                }
            }
        } else if (!first && last_value[0] != '\0') {
            printf("[提示] 播放器已退出，信息文件被清除。\n");
            last_value[0] = '\0';
        }

        first = 0;
        sleep(1);  // 每秒检查一次
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc >= 2 && strcmp(argv[1], "loop") == 0) {
        return read_loop();
    } else {
        return read_once();
    }
}
