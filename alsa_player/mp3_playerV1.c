/*
 * alsa_mp3_player.c
 * 基于 ALSA + libmpg123 的 MP3 播放器
 * 适用于 ARM Linux 开发板
 * 
 * 编译: gcc alsa_mp3_player.c -o alsa_mp3_player -lmpg123 -lasound -pthread -O2
 * 依赖: libmpg123-dev, libasound2-dev
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <alloca.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mpg123.h>
#include <alsa/asoundlib.h>

#define PCM_DEVICE "default"    // 默认音频设备，可改为 "hw:0,0" 等
#define BUFFER_SIZE 8192        // 解码缓冲区大小
#define FIFO_PATH "/tmp/mp3_player.cmd"  // 控制命令管道
#define FIFO_INFO_PATH "/tmp/music_format"

// 全局变量，用于信号处理
static volatile int g_running = 1;
static volatile int g_paused = 0;
static snd_pcm_t *g_pcm_handle = NULL;

// 信号处理函数 - 优雅退出
void signal_handler(int sig)
{
    printf("\n收到信号 %d，正在停止播放...\n", sig);
    g_running = 0;
    if (g_pcm_handle) {
        snd_pcm_drop(g_pcm_handle);  // 立即停止播放
    }
}

// FIFO 命令监听线程
void *fifo_listener_thread(void *arg)
{
    char cmd[64];
    int fd;

    // 创建 FIFO（如果已存在则忽略错误）
    mkfifo(FIFO_PATH, 0666);

    // 以读写方式打开，防止所有 writer 关闭后 reader 读到 EOF
    fd = open(FIFO_PATH, O_RDWR);
    if (fd < 0) {
        perror("打开 FIFO 失败");
        return NULL;
    }

    while (g_running) {
        ssize_t n = read(fd, cmd, sizeof(cmd) - 1);
        if (n > 0) {
            cmd[n] = '\0';

            // 处理缓冲区中可能包含的多条命令（以 \n 分隔）
            char *p = cmd;
            while (*p) {
                char *end = strchr(p, '\n');
                if (end) *end = '\0';

                if (strcmp(p, "pause") == 0) {
                    g_paused = 1;
                    printf("\n[命令] 暂停播放\n");
                } else if (strcmp(p, "resume") == 0 || strcmp(p, "play") == 0) {
                    g_paused = 0;
                    printf("\n[命令] 继续播放\n");
                } else if (strcmp(p, "stop") == 0) {
                    g_running = 0;
                    printf("\n[命令] 停止播放\n");
                    if (g_pcm_handle) {
                        snd_pcm_drop(g_pcm_handle);
                    }
                }

                if (!end) break;
                p = end + 1;
            }
        } else if (n == 0) {
            usleep(100000);
        } else {
            perror("读取 FIFO 失败");
            break;
        }
    }

    close(fd);
    return NULL;
}

// 向正在运行的播放器发送控制命令
int send_command(const char *cmd)
{
    int fd = open(FIFO_PATH, O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "错误: 无法连接到播放器进程，可能没有在播放。\n");
        return -1;
    }
    if (write(fd, cmd, strlen(cmd)) < 0) {
        // ignore
    }
    if (write(fd, "\n", 1) < 0) {
        // ignore
    }
    close(fd);
    return 0;
}

// 将音乐时长信息写入文件，供其他程序随时读取
// 注意：这里使用普通文件，而非 FIFO。因为 FIFO 要求必须有 reader 在监听，
// 且数据只能读一次；而普通文件可以随时打开读取。
int send_info_command(const char *cmd)
{
    int fd = open(FIFO_INFO_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("错误: 无法写入信息文件");
        return -1;
    }
    if (write(fd, cmd, strlen(cmd)) < 0) {
        // ignore
    }
    if (write(fd, "\n", 1) < 0) {
        // ignore
    }
    close(fd);
    return 0;
}

// 获取 ALSA 格式
snd_pcm_format_t get_alsa_format(int encoding)
{
    if (encoding & MPG123_ENC_16) {
        if (encoding & MPG123_ENC_SIGNED) {
            #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
                return SND_PCM_FORMAT_S16_LE;
            #else
                return SND_PCM_FORMAT_S16_BE;
            #endif
        } else {
            #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
                return SND_PCM_FORMAT_U16_LE;
            #else
                return SND_PCM_FORMAT_U16_BE;
            #endif
        }
    } else if (encoding & MPG123_ENC_8) {
        return (encoding & MPG123_ENC_SIGNED) ? SND_PCM_FORMAT_S8 : SND_PCM_FORMAT_U8;
    } else if (encoding & MPG123_ENC_24) {
        return SND_PCM_FORMAT_S24_LE;  // 假设小端
    } else if (encoding & MPG123_ENC_32) {
        return SND_PCM_FORMAT_S32_LE;
    }
    
    // 默认返回 S16_LE
    return SND_PCM_FORMAT_S16_LE;
}

// 初始化 ALSA 音频设备
int init_alsa(snd_pcm_t **pcm_handle, int sample_rate, int channels, snd_pcm_format_t format)
{
    snd_pcm_hw_params_t *hw_params;
    int err;
    unsigned int rate = sample_rate;

    // 打开 PCM 设备
    err = snd_pcm_open(pcm_handle, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        fprintf(stderr, "无法打开 PCM 设备 '%s': %s\n", PCM_DEVICE, snd_strerror(err));
        return -1;
    }

    g_pcm_handle = *pcm_handle;

    // 分配硬件参数结构
    snd_pcm_hw_params_alloca(&hw_params);
    
    // 初始化硬件参数
    err = snd_pcm_hw_params_any(*pcm_handle, hw_params);
    if (err < 0) {
        fprintf(stderr, "无法初始化硬件参数: %s\n", snd_strerror(err));
        return -1;
    }

    // 设置访问类型 - 交错模式
    err = snd_pcm_hw_params_set_access(*pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        fprintf(stderr, "无法设置访问类型: %s\n", snd_strerror(err));
        return -1;
    }

    // 设置采样格式
    err = snd_pcm_hw_params_set_format(*pcm_handle, hw_params, format);
    if (err < 0) {
        fprintf(stderr, "无法设置格式 %d: %s\n", format, snd_strerror(err));
        fprintf(stderr, "尝试使用 S16_LE 格式...\n");
        
        // 尝试使用 S16_LE 作为后备
        err = snd_pcm_hw_params_set_format(*pcm_handle, hw_params, SND_PCM_FORMAT_S16_LE);
        if (err < 0) {
            fprintf(stderr, "后备格式也失败: %s\n", snd_strerror(err));
            return -1;
        }
    }

    // 设置声道数
    err = snd_pcm_hw_params_set_channels(*pcm_handle, hw_params, channels);
    if (err < 0) {
        fprintf(stderr, "无法设置声道数 %d: %s\n", channels, snd_strerror(err));
        return -1;
    }

    // 设置采样率（允许近似值）
    err = snd_pcm_hw_params_set_rate_near(*pcm_handle, hw_params, &rate, 0);
    if (err < 0) {
        fprintf(stderr, "无法设置采样率 %d: %s\n", sample_rate, snd_strerror(err));
        return -1;
    }

    if ((int)rate != sample_rate) {
        printf("注意: 采样率被调整为 %u Hz (请求: %d Hz)\n", rate, sample_rate);
    }

    // 应用硬件参数
    err = snd_pcm_hw_params(*pcm_handle, hw_params);
    if (err < 0) {
        fprintf(stderr, "无法应用硬件参数: %s\n", snd_strerror(err));
        return -1;
    }

    // 准备设备
    err = snd_pcm_prepare(*pcm_handle);
    if (err < 0) {
        fprintf(stderr, "无法准备 PCM 设备: %s\n", snd_strerror(err));
        return -1;
    }

    return 0;
}

// 写入 ALSA 缓冲区，处理欠载（underrun）情况
int write_alsa(snd_pcm_t *pcm_handle, const void *buffer, snd_pcm_uframes_t frames)
{
    int err;
    snd_pcm_sframes_t written;

retry:
    written = snd_pcm_writei(pcm_handle, buffer, frames);
    
    if (written < 0) {
        // 处理错误
        if (written == -EPIPE) {
            // 欠载（underrun）
            fprintf(stderr, "ALSA 欠载，正在恢复...\n");
            err = snd_pcm_prepare(pcm_handle);
            if (err < 0) {
                fprintf(stderr, "无法恢复: %s\n", snd_strerror(err));
                return -1;
            }
            goto retry;
        } else if (written == -ESTRPIPE) {
            // 挂起
            fprintf(stderr, "ALSA 挂起，正在恢复...\n");
            while ((err = snd_pcm_resume(pcm_handle)) == -EAGAIN) {
                usleep(100000);  // 等待 100ms
            }
            if (err < 0) {
                err = snd_pcm_prepare(pcm_handle);
                if (err < 0) {
                    fprintf(stderr, "无法从挂起恢复: %s\n", snd_strerror(err));
                    return -1;
                }
            }
            goto retry;
        } else {
            fprintf(stderr, "ALSA 写入错误: %s\n", snd_strerror(-written));
            return -1;
        }
    }

    // 如果写入不完全，处理剩余数据
    if ((snd_pcm_uframes_t)written < frames) {
        buffer = (const char *)buffer + written * 4;  // 假设 16bit 立体声 = 4 bytes/frame
        frames -= written;
        goto retry;
    }

    return 0;
}

// 播放 MP3 文件
int play_mp3(const char *filename)
{
    mpg123_handle *mh = NULL;
    unsigned char *buffer = NULL;
    size_t buffer_size;
    size_t done;
    int err;
    int channels, encoding;
    long sample_rate;
    snd_pcm_t *pcm_handle = NULL;
    snd_pcm_format_t alsa_format;
    pthread_t listener_tid;

    printf("正在播放: %s\n", filename);

    // 1. 初始化 mpg123
    err = mpg123_init();
    if (err != MPG123_OK) {
        fprintf(stderr, "mpg123 初始化失败: %s\n", mpg123_plain_strerror(err));
        return -1;
    }

    // 2. 创建解码器句柄
    mh = mpg123_new(NULL, &err);
    if (!mh) {
        fprintf(stderr, "创建 mpg123 句柄失败: %s\n", mpg123_plain_strerror(err));
        mpg123_exit();
        return -1;
    }

    // 3. 打开 MP3 文件
    err = mpg123_open(mh, filename);
    if (err != MPG123_OK) {
        fprintf(stderr, "无法打开文件 '%s': %s\n", filename, mpg123_strerror(mh));
        goto cleanup;
    }

    // 4. 获取音频格式信息
    err = mpg123_getformat(mh, &sample_rate, &channels, &encoding);
    if (err != MPG123_OK) {
        fprintf(stderr, "无法获取音频格式: %s\n", mpg123_strerror(mh));
        goto cleanup;
    }

    // 获取总时长
    mpg123_scan(mh);
    off_t total_samples = mpg123_length(mh);

    // 打印音频信息
    printf("=== 音频信息 ===\n");
    printf("采样率: %ld Hz\n", sample_rate);
    printf("声道数: %d (%s)\n", channels, channels == 1 ? "单声道" : "立体声");
    if (total_samples > 0) {
        int total_seconds = total_samples / sample_rate;
        int minutes = total_seconds / 60;
        int seconds = total_seconds % 60;
        char ch[10];
        sprintf(ch,"%d",total_seconds);
        printf("总时长: %d:%02d\n", minutes, seconds);
        send_info_command(ch);
        printf("send:%s\n", ch);

    } else {
        printf("总时长: 未知\n");
    }
    printf("编码: 0x%x ", encoding);
    
    if (encoding & MPG123_ENC_16) printf("16-bit ");
    else if (encoding & MPG123_ENC_8) printf("8-bit ");
    else if (encoding & MPG123_ENC_24) printf("24-bit ");
    else if (encoding & MPG123_ENC_32) printf("32-bit ");
    
    if (encoding & MPG123_ENC_SIGNED) printf("有符号");
    else printf("无符号");
    printf("\n");

    // 5. 设置输出格式（强制使用标准格式）
    mpg123_format_none(mh);
    mpg123_format(mh, sample_rate, channels, encoding);

    // 6. 获取 ALSA 格式并初始化
    alsa_format = get_alsa_format(encoding);
    if (init_alsa(&pcm_handle, sample_rate, channels, alsa_format) < 0) {
        goto cleanup;
    }

    // 7. 分配解码缓冲区
    buffer_size = mpg123_outblock(mh);
    if (buffer_size < BUFFER_SIZE) {
        buffer_size = BUFFER_SIZE;
    }
    
    buffer = (unsigned char *)malloc(buffer_size);
    if (!buffer) {
        fprintf(stderr, "内存分配失败\n");
        goto cleanup;
    }

    // 启动 FIFO 监听线程
    g_paused = 0;
    if (pthread_create(&listener_tid, NULL, fifo_listener_thread, NULL) != 0) {
        fprintf(stderr, "创建监听线程失败\n");
        goto cleanup;
    }

    printf("开始播放 (按 Ctrl+C 停止)...\n");

    // 8. 解码并播放循环
    while (g_running) {
        // 处理暂停：暂停 ALSA 硬件，并等待恢复
        if (g_paused) {
            if (pcm_handle) {
                int ret = snd_pcm_pause(pcm_handle, 1);
                if (ret < 0) {
                    printf(" 设备不支持 pause直接跳过写入");
                    // 设备不支持 pause，直接跳过写入
                }
            }
            while (g_paused && g_running) {
                usleep(100000);  // 100ms
            }
            if (pcm_handle) {
                snd_pcm_pause(pcm_handle, 0);
            }
            if (!g_running) break;
        }

        err = mpg123_read(mh, buffer, buffer_size, &done);
        
        if (err == MPG123_DONE) {
            printf("\n播放完成\n");
            break;
        } else if (err == MPG123_NEW_FORMAT) {
            printf("检测到新格式，重新配置...\n");
            mpg123_getformat(mh, &sample_rate, &channels, &encoding);
            // 在实际应用中，这里需要重新配置 ALSA
            continue;
        } else if (err != MPG123_OK) {
            fprintf(stderr, "解码错误: %s\n", mpg123_strerror(mh));
            break;
        }

        if (done > 0) {
            // 计算帧数 (假设 16-bit 采样，2 bytes per sample)
            int sample_size = (encoding & MPG123_ENC_16) ? 2 : 
                             (encoding & MPG123_ENC_8) ? 1 : 4;
            snd_pcm_uframes_t frames = done / (channels * sample_size);
            
            if (write_alsa(pcm_handle, buffer, frames) < 0) {
                fprintf(stderr, "ALSA 写入失败\n");
                break;
            }
        }
    }

    // 9. 等待播放完成（排空缓冲区）
    if (pcm_handle && !g_running) {
        snd_pcm_drop(pcm_handle);
    } else if (pcm_handle) {
        snd_pcm_drain(pcm_handle);
    }

    // 停止监听线程
    pthread_cancel(listener_tid);
    pthread_join(listener_tid, NULL);

cleanup:
    // 清理资源
    if (buffer) free(buffer);
    if (pcm_handle) {
        snd_pcm_close(pcm_handle);
        g_pcm_handle = NULL;
    }
    if (mh) {
        mpg123_close(mh);
        mpg123_delete(mh);
    }
    mpg123_exit();
    unlink(FIFO_PATH);
    unlink(FIFO_INFO_PATH);
    
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("用法:\n");
        printf("  播放: %s <MP3文件路径>\n", argv[0]);
        printf("  暂停: %s pause\n", argv[0]);
        printf("  继续: %s resume\n", argv[0]);
        printf("  停止: %s stop\n", argv[0]);
        return 1;
    }

    // 控制命令处理
    if (strcmp(argv[1], "pause") == 0) {
        return send_command("pause") < 0 ? 1 : 0;
    }
    if (strcmp(argv[1], "resume") == 0) {
        return send_command("resume") < 0 ? 1 : 0;
    }
    if (strcmp(argv[1], "stop") == 0) {
        return send_command("stop") < 0 ? 1 : 0;
    }

    // 设置信号处理
    signal(SIGINT, signal_handler);   // Ctrl+C
    signal(SIGTERM, signal_handler);    // kill

    // 检查文件是否存在
    if (access(argv[1], F_OK) != 0) {
        fprintf(stderr, "错误: 文件 '%s' 不存在\n", argv[1]);
        return 1;
    }

    // 播放文件
    int ret = play_mp3(argv[1]);

    return ret;
}