#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QDateTime>
#include <QStringList>

class CaptureThread;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void showImage(QImage image);
//    void startCaptureButtonClicked(bool start);

    void quitCapture();

    // 新增拍照相关槽
    void onPhotoSaved(QString path);
    void onCaptureError(QString msg);
    void onBtnPhotoClicked();
    void onBtnAlbumClicked();
    void onBtnPrevClicked();          // 上一张（隐藏按钮）
    void onBtnNextClicked();          // 下一张（隐藏按钮）
    void onBtnvideoClicked();

    void onRecodeStarted();
    void onRecodeStopped(QString path);


private:

    void setupUi();
    void updateStatus(QString text, bool isError = false);
    void enterAlbumMode();            // 进入相册模式
    void exitAlbumMode();             // 退出相册模式，恢复采集

    void loadAlbumImage(int index);   // 加载指定索引的图片
    void refreshAlbumButtons();       // 刷新隐藏按钮的显示/可用状态
    static QString formatFileSize(qint64 bytes);

    void   keyPressEvent (QKeyEvent   *event);//

    // 原有控件
    QLabel *videoLabel = nullptr;
//    QPushButton *startCaptureButton = nullptr;
    QPushButton *quitButton = nullptr;

    QPushButton *btnPrev_;            // 左侧：上一张（透明隐藏）
    QPushButton *btnNext_;            // 右侧：下一张（透明隐藏）
    QStringList albumFiles_;          // 相册图片路径列表
    int albumCurrentIndex_;           // 当前显示的图片索引

    // 新增控件
    QPushButton *btnPhoto_ = nullptr;      // 拍照按钮
    QLabel *labelStatus_ = nullptr;        // 状态提示
    QPushButton *btnAlbum_=nullptr;
    QPushButton *btnVideo_=nullptr;
    bool isAlbum_=false;
    CaptureThread *captureThread = nullptr;
    bool isVideo_=false;

};

#endif
//******************************************************************
//Copyright © Deng Zhimao Co., Ltd. 2021-2030. All rights reserved.
//* @projectName   video_server
//* @brief         mainwindow.h
//* @author        Deng Zhimao
//* @email         dengzhimao@alientek.com
//* @link          www.openedv.com
//* @date          2021-11-19
//*******************************************************************/
//#ifndef MAINWINDOW_H
//#define MAINWINDOW_H

//#include <QMainWindow>
//#include <QLabel>
//#include <QImage>
//#include <QPushButton>
//#include <QHBoxLayout>
//#include <QCheckBox>

//#include "capture_thread.h"

//class MainWindow : public QMainWindow
//{
//    Q_OBJECT

//public:
//    MainWindow(QWidget *parent = nullptr);
//    ~MainWindow();

//private:
//    void initCamera();
//    void on_btnPhoto_clicked();
//    /* 用于显示捕获到的图像 */
//    QLabel *videoLabel;

//    /* 摄像头线程 */
//    CaptureThread *captureThread;
//    CaptureThread *capture_;

//    /* 开始捕获图像按钮 */
//    QPushButton *startCaptureButton;
//    QPushButton *quitButton;
//    /* 用于开启本地图像显示 */
//    QCheckBox *checkBox1;

//    /* 用于开启网络广播 */
//    QCheckBox *checkBox2;

//    /* 重写大小事件 */
//    void resizeEvent(QResizeEvent *event) override;

//private slots:
//    /* 显示图像 */
//    void showImage(QImage);

//    /* 开始采集按钮被点击 */
//    void startCaptureButtonClicked(bool);
//    void onCheckBox1Clicked(bool);
//    void onCheckBox2Clicked(bool);
//    void quitCapture();
//};

