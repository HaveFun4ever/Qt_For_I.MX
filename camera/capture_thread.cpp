#include "capture_thread.h"
#include "video_recorder.h"

#include <QImage>
#include <QBuffer>
#include <QFile>
#include <QImageWriter>

#ifdef linux
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/videodev2.h>

struct buffer_info {
    void *start;
    unsigned int length;
};
#endif

CaptureThread::CaptureThread(QObject *parent)
    : QThread(parent)
{
}

CaptureThread::~CaptureThread()
{
    stopCapture();
    wait(3000);
}

void CaptureThread::startCapture()
{
    startFlag = true;
    isCapturing_ = true;
    start();
}

void CaptureThread::stopCapture()
{
    startFlag = false;
}

void CaptureThread::setLocalDisplay(bool enable)
{
    startLocalDisplay = enable;
}

/* ==================== 相机功能接口 ==================== */

void CaptureThread::takePhoto(const QString &savePath)
{
    QMutexLocker lock(&photoMutex_);
    photoPath_ = savePath;
    photoPending_ = true;
}

void CaptureThread::startRecord(const QString &savePath)
{
    QMutexLocker lock(&recordMutex_);
    if (isRecording_) return;

    recordPath_ = savePath;
    recorder_ = new VideoRecorder(this);
    if (recorder_->init(savePath, 320, 240, 8)) {
        recorder_->start();
        isRecording_ = true;
        emit recordStarted();
    } else {
        delete recorder_;
        recorder_ = nullptr;
        emit captureError("Failed to initialize video recorder");
    }
}

void CaptureThread::stopRecord()
{
    QMutexLocker lock(&recordMutex_);
    if (!isRecording_ || !recorder_) return;

    recorder_->stop();
    recorder_->wait(5000);
    delete recorder_;
    recorder_ = nullptr;
    isRecording_ = false;
    emit recordStopped(recordPath_);
}

bool CaptureThread::isRecording() const
{
    return isRecording_;
}

/* ==================== 格式转换 ==================== */

QImage CaptureThread::convertRGB565toRGB888(const void *data, int w, int h)
{
    QImage img(w, h, QImage::Format_RGB888);
    const quint16 *src = (const quint16*)data;

    int dstBytesPerLine = img.bytesPerLine();
    for (int row = 0; row < h; row++) {
            // 目标行：从底向上（第0行→最后一行，第h-1行→第0行）
            uchar *dst = img.bits() + (h - 1 - row) * dstBytesPerLine;

            // 源数据当前行起始
            const quint16 *srcRow = src + row * w;

            for (int col = 0; col < w; col++) {
                quint16 pixel = srcRow[col];
                dst[col*3]   = ((pixel >> 11) & 0x1F) << 3;   // R
                dst[col*3+1] = ((pixel >> 5)  & 0x3F) << 2;   // G
                dst[col*3+2] = (pixel & 0x1F) << 3;            // B
            }
        }
    return img;
}

/* ==================== 主采集循环 ==================== */

void CaptureThread::run()
{
#ifdef linux
#ifndef __arm__
    return;
#endif

    int video_fd = -1;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req_bufs;
    static struct v4l2_buffer buf;
    int n_buf;
    struct buffer_info bufs_info[VIDEO_BUFFER_COUNT];
    enum v4l2_buf_type type;

    /* ========== V4L2 设备初始化（保持原有逻辑）========== */
    video_fd = open(VIDEO_DEV, O_RDWR);
    if (0 > video_fd) {
        emit captureError(QString("Failed to open %1").arg(VIDEO_DEV));
        return;
    }

    // 设置格式: RGB565 640x480
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;


    if (0 > ioctl(video_fd, VIDIOC_S_FMT, &fmt)) {
        emit captureError("VIDIOC_S_FMT failed");
        close(video_fd);
        return;
    }

    // 申请缓冲区
    memset(&req_bufs, 0, sizeof(req_bufs));
    req_bufs.count = VIDEO_BUFFER_COUNT;
    req_bufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req_bufs.memory = V4L2_MEMORY_MMAP;

    if (0 > ioctl(video_fd, VIDIOC_REQBUFS, &req_bufs)) {
        emit captureError("VIDIOC_REQBUFS failed");
        close(video_fd);
        return;
    }

    // 查询并 MMAP 映射缓冲区
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    for (n_buf = 0; n_buf < VIDEO_BUFFER_COUNT; n_buf++) {
        buf.index = n_buf;
        if (0 > ioctl(video_fd, VIDIOC_QUERYBUF, &buf)) {
            emit captureError("VIDIOC_QUERYBUF failed");
            close(video_fd);
            return;
        }

        bufs_info[n_buf].length = buf.length;
        bufs_info[n_buf].start = mmap(NULL, buf.length,
                                      PROT_READ | PROT_WRITE, MAP_SHARED,
                                      video_fd, buf.m.offset);
        if (MAP_FAILED == bufs_info[n_buf].start) {
            emit captureError(QString("mmap failed, size 0x%1").arg(buf.length, 0, 16));
            close(video_fd);
            return;
        }
    }

    // 入队所有缓冲区
    for (n_buf = 0; n_buf < VIDEO_BUFFER_COUNT; n_buf++) {
        buf.index = n_buf;
        if (0 > ioctl(video_fd, VIDIOC_QBUF, &buf)) {
            emit captureError("VIDIOC_QBUF failed");
            close(video_fd);
            return;
        }
    }

    // 启动视频流
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (0 > ioctl(video_fd, VIDIOC_STREAMON, &type)) {
        emit captureError("VIDIOC_STREAMON failed");
        close(video_fd);
        return;
    }

    /* ========== 采集主循环 ========== */
    int frameCount = 0;

    while (startFlag) {

        for (n_buf = 0; n_buf < VIDEO_BUFFER_COUNT; n_buf++) {

            buf.index = n_buf;

            if (0 > ioctl(video_fd, VIDIOC_DQBUF, &buf)) {
                if (errno == EAGAIN) {
                    msleep(1);
                    continue;
                }
                emit captureError("VIDIOC_DQBUF failed");
                goto cleanup;
            }

            // 2. 转换为 RGB888 用于后续处理
//            QImage frameRGB888 = convertRGB565toRGB888(
//                bufs_info[n_buf].start,
//                fmt.fmt.pix.width,
//                fmt.fmt.pix.height);

            QImage previewImg=QImage(
                static_cast<const uchar*>(bufs_info[n_buf].start),
                fmt.fmt.pix.width,
                fmt.fmt.pix.height,
                fmt.fmt.pix.width * 2,  // RGB565 stride = width * 2
                QImage::Format_RGB16    // Qt 的 RGB565 格式
            ).mirrored(true, true);

            // 3. 本地预览 (降频发送，避免 UI 卡顿)
            if (startLocalDisplay && (frameCount % 2 == 0)) {
                emit imageReady(previewImg);
            }

            // 4. 处理拍照请求
            {
                QMutexLocker lock(&photoMutex_);
                if (photoPending_) {
                    // 使用 QImageWriter 保存 JPEG
                    QImageWriter writer(photoPath_);
                    writer.setFormat("jpeg");
                    writer.setQuality(95);

                    if (writer.write(previewImg)) {
                        emit photoSaved(photoPath_);
                    } else {
                        emit captureError("JPEG save failed: " + writer.errorString());
                    }
                    photoPending_ = false;
                }
            }

             //5. 送入录像队列
            {
                QMutexLocker lock(&recordMutex_);
                if (isRecording_ && recorder_) {
                    if (frameCount % 2 == 0) {
                        recorder_->addFrame(previewImg.copy());
                    }

                }
            }

            // 6. 归还缓冲区
            if (0 > ioctl(video_fd, VIDIOC_QBUF, &buf)) {
                emit captureError("VIDIOC_QBUF failed");
                goto cleanup;
            }

            frameCount++;
        }

        msleep(2);  // 防止 CPU 空转 100%
    }

cleanup:
    /* ========== 清理资源 ========== */
     //停止录像
    if (recorder_) {
        recorder_->stop();
        recorder_->wait(3000);
        delete recorder_;
        recorder_ = nullptr;
    }

    // 停止视频流
    msleep(800);  // 确保最后一帧处理完成
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(video_fd, VIDIOC_STREAMOFF, &type);

    // 释放 MMAP 缓冲区
    for (int i = 0; i < VIDEO_BUFFER_COUNT; i++) {
        if (bufs_info[i].start != MAP_FAILED && bufs_info[i].start != nullptr) {
            munmap(bufs_info[i].start, bufs_info[i].length);
        }
    }

    close(video_fd);
    printf("Capture thread exited cleanly\n");

#endif
}
