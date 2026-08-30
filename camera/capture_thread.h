#ifndef CAPTURE_THREAD_H
#define CAPTURE_THREAD_H

#include <QThread>
#include <QImage>
#include <QMutex>
#include <QWaitCondition>

// 视频录制器前向声明
class VideoRecorder;

class CaptureThread : public QThread
{
    Q_OBJECT
public:
    explicit CaptureThread(QObject *parent = nullptr);
    ~CaptureThread();
    bool isCapturing_ = false;
    void startCapture();      // 启动采集
    void stopCapture();       // 停止采集
    void setLocalDisplay(bool enable);  // 设置本地预览开关

    // 相机功能接口
    void takePhoto(const QString &savePath);   // 触发拍照
    void startRecord(const QString &savePath); // 开始录像
    void stopRecord();                          // 停止录像
    bool isRecording() const;

signals:
    void imageReady(QImage image);      // 预览帧信号
    void photoSaved(QString path);      // 照片保存完成
    void recordStarted();               // 录像开始
    void recordStopped(QString path);   // 录像停止
    void captureError(QString msg);     // 错误信号

protected:
    void run() override;

private:
    // V4L2 参数
    static constexpr int VIDEO_BUFFER_COUNT = 4;
    static constexpr const char* VIDEO_DEV = "/dev/video1";

    // 线程控制
    volatile bool startFlag = false;
    bool startLocalDisplay = true;

    // 拍照控制
    QMutex photoMutex_;
    bool photoPending_ = false;
    QString photoPath_;

    // 录像控制
    QMutex recordMutex_;
    VideoRecorder *recorder_ = nullptr;
    bool isRecording_ = false;
    QString recordPath_;
    // 格式转换
    QImage convertRGB565toRGB888(const void *data, int w, int h);
};

#endif
