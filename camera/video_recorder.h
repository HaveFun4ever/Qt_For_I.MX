
#ifndef VIDEO_RECORDER_H
#define VIDEO_RECORDER_H

#include <QThread>
#include <QImage>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

struct RawFrame {
    const uint8_t* data;
    int width;
    int height;
    int stride;
    AVPixelFormat format;
};


class VideoRecorder : public QThread
{
    Q_OBJECT
public:
    explicit VideoRecorder(QObject *parent = nullptr);
    ~VideoRecorder();

    bool init(const QString &filename, int width, int height, int fps = 30);
    void addFrame(const QImage &frame);
    void stop();

signals:
    void recordError(const QString &msg);

protected:
    void run() override;

private:
    QString filename_;
    int width_, height_, fps_;
    volatile bool abort_ = false;

    QQueue<QImage> frameQueue_;
    QMutex queueMutex_;
    QWaitCondition queueNotEmpty_;
    const int maxQueueSize_ = 8;  // i.MX6ULL 内存受限，限制队列

    // FFmpeg
    AVFormatContext *fmtCtx_ = nullptr;
    AVCodecContext *codecCtx_ = nullptr;
    AVStream *stream_ = nullptr;
    SwsContext *swsCtx_ = nullptr;
    AVFrame *frameYUV_ = nullptr;
    AVPacket *packet_ = nullptr;
    int64_t frameIndex_ = 0;

    bool openEncoder();
    void closeEncoder();
    bool writeFrame(const QImage& img);
};

#endif
