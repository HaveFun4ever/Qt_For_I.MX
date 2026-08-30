
// video_recorder.cpp - 兼容 FFmpeg 2.x/3.0 版本

#include "video_recorder.h"
#include <QDebug>

VideoRecorder::VideoRecorder(QObject *parent) : QThread(parent) {}

VideoRecorder::~VideoRecorder()
{
    stop();
    wait(3000);
}

bool VideoRecorder::init(const QString &filename, int w, int h, int fps)
{
    filename_ = filename;
    width_ = w;
    height_ = h;
    fps_ = fps;
    abort_ = false;
    return openEncoder();
}

void VideoRecorder::addFrame(const QImage &frame)
{
    QMutexLocker lock(&queueMutex_);
    if (frameQueue_.size() >= maxQueueSize_) {
        frameQueue_.dequeue();
    }
    frameQueue_.enqueue(frame);
    queueNotEmpty_.wakeOne();

}

//void VideoRecorder::addFrame(const uint8_t* data, int w, int h, int stride, AVPixelFormat fmt)
//{
//    QMutexLocker lock(&queueMutex_);
//    if (frameQueue_.size() >= maxQueueSize_) {
//        frameQueue_.dequeue();  // 队列满则丢最旧帧，保证实时性
//    }
//    RawFrame rf = {data, w, h, stride, fmt};
//    frameQueue_.enqueue(rf);
//    queueNotEmpty_.wakeOne();
//}

void VideoRecorder::stop()
{
    QMutexLocker lock(&queueMutex_);
    abort_ = true;
    queueNotEmpty_.wakeAll();
}

bool VideoRecorder::openEncoder()
{
    av_register_all();  // FFmpeg 3.x 需要初始化

    avformat_alloc_output_context2(&fmtCtx_, nullptr, nullptr,
                                   filename_.toUtf8().constData());
    if (!fmtCtx_) return false;

    // 查找编码器
    AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        qDebug() << "Using software H.264 encoder";
        codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    }

    if (!codec) {
        emit recordError("H.264 encoder not found");
        return false;
    }

    // ========== FFmpeg 2.x/3.0 兼容：使用 stream->codec 而非 codecpar ==========
    stream_ = avformat_new_stream(fmtCtx_, codec);
    if (!stream_) return false;

    // 旧版 API：直接操作 stream->codec（AVCodecContext 指针）
    codecCtx_ = stream_->codec;  // ← 关键修改：不使用 avcodec_alloc_context3

    codecCtx_->codec_id = AV_CODEC_ID_H264;
    codecCtx_->codec_type = AVMEDIA_TYPE_VIDEO;
    codecCtx_->width = width_;
    codecCtx_->height = height_;
    codecCtx_->time_base = (AVRational){1, fps_};
    codecCtx_->framerate = (AVRational){fps_, 1};
    codecCtx_->pix_fmt = AV_PIX_FMT_YUV420P;
    codecCtx_->bit_rate = 600000;
    codecCtx_->gop_size = fps_*2;
    codecCtx_->max_b_frames = 0;

    // x264 特定参数（旧版通过 AVCodecContext 直接设置）
    if (codec->id == AV_CODEC_ID_H264) {
        av_opt_set(codecCtx_->priv_data, "preset", "ultrafast", 0);
        av_opt_set(codecCtx_->priv_data, "tune", "zerolatency", 0);
    }

    // 打开编码器（旧版需要传入 codec）
    if (avcodec_open2(codecCtx_, codec, nullptr) < 0) {
        emit recordError("Failed to open codec");
        return false;
    }

    // 打开输出文件
    if (!(fmtCtx_->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&fmtCtx_->pb, filename_.toUtf8().constData(), AVIO_FLAG_WRITE) < 0) {
            emit recordError("Failed to open output file");
            return false;
        }
    }

    // 写文件头
    if (avformat_write_header(fmtCtx_, nullptr) < 0) {
        emit recordError("Failed to write header");
        return false;
    }

    // 初始化图像转换
    swsCtx_ = sws_getContext(width_, height_, AV_PIX_FMT_RGB565LE,
                             width_, height_, AV_PIX_FMT_YUV420P,
                             SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

    // 分配帧
    frameYUV_ = av_frame_alloc();
    frameYUV_->format = AV_PIX_FMT_YUV420P;
    frameYUV_->width = width_;
    frameYUV_->height = height_;
//    avpicture_alloc((AVPicture*)frameYUV_, AV_PIX_FMT_YUV420P, width_, height_);
    int ret = av_image_alloc(frameYUV_->data, frameYUV_->linesize,
                                 width_, height_, AV_PIX_FMT_YUV420P, 32);
        if (ret < 0) {
            emit recordError("Failed to allocate YUV frame buffer");
            return false;
        }

    return true;
}

void VideoRecorder::closeEncoder()
{
    if (frameYUV_) {
//        avpicture_free((AVPicture*)frameYUV_);  // 旧版释放
//        av_frame_free(&frameYUV_);
        av_freep(&frameYUV_->data[0]);
        av_frame_free(&frameYUV_);
    }
    sws_freeContext(swsCtx_);

    // 旧版：关闭 codec 后，avformat_free_context 会释放 stream->codec
    if (codecCtx_) {
        avcodec_close(codecCtx_);  // 旧版关闭方式
    }

    if (fmtCtx_ && !(fmtCtx_->oformat->flags & AVFMT_NOFILE))
        avio_closep(&fmtCtx_->pb);
    avformat_free_context(fmtCtx_);
}

// ========== FFmpeg 2.x/3.0 兼容：使用 avcodec_encode_video2 ==========
bool VideoRecorder::writeFrame(const QImage &img)
{
    const uint8_t *srcData[4] = { img.bits(), nullptr, nullptr, nullptr };
        int srcLinesize[4] = { img.bytesPerLine(), 0, 0, 0 };

        sws_scale(swsCtx_, srcData, srcLinesize, 0, height_,
                  frameYUV_->data, frameYUV_->linesize);

        frameYUV_->pts = frameIndex_++;

        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.data = nullptr;
        pkt.size = 0;

        int got_packet = 0;
        int ret = avcodec_encode_video2(codecCtx_, &pkt, frameYUV_, &got_packet);
        if (ret < 0) return false;

        if (got_packet) {
            if (pkt.pts != AV_NOPTS_VALUE)
                pkt.pts = av_rescale_q(pkt.pts, codecCtx_->time_base, stream_->time_base);
            if (pkt.dts != AV_NOPTS_VALUE)
                pkt.dts = av_rescale_q(pkt.dts, codecCtx_->time_base, stream_->time_base);

            pkt.stream_index = stream_->index;
            av_interleaved_write_frame(fmtCtx_, &pkt);
            av_packet_unref(&pkt);
        }

        return true;
}

void VideoRecorder::run()
{
    while (!abort_) {
            QImage img;
            {
                QMutexLocker lock(&queueMutex_);
                while (frameQueue_.isEmpty() && !abort_) {
                    queueNotEmpty_.wait(&queueMutex_, 100);
                }
                if (abort_ && frameQueue_.isEmpty()) break;
                if (!frameQueue_.isEmpty()) {
                    img = frameQueue_.dequeue();
                }
            }

            if (!img.isNull()) {
                if (!writeFrame(img)) {
                    emit recordError("Frame encoding failed");
                }
            }
        }

    // 刷新编码器（传入空帧）
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = nullptr;
    pkt.size = 0;
    int got_packet = 0;

    // 旧版 flush 方式
    do {
        got_packet = 0;
        int ret = avcodec_encode_video2(codecCtx_, &pkt, nullptr, &got_packet);
        if (ret < 0) break;
        if (got_packet) {
            if (pkt.pts != AV_NOPTS_VALUE)
                pkt.pts = av_rescale_q(pkt.pts, codecCtx_->time_base, stream_->time_base);
            pkt.stream_index = stream_->index;
            av_interleaved_write_frame(fmtCtx_, &pkt);
            av_packet_unref(&pkt);
        }
    } while (got_packet);

    av_write_trailer(fmtCtx_);
    closeEncoder();
    qDebug() << "Video saved to:" << filename_;
}
