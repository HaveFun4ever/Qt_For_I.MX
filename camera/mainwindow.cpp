
#include "mainwindow.h"
#include "capture_thread.h"

#include <QApplication>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QTimer>        // QTimer::singleShot
#include <QCloseEvent>   // closeEvent 参数
#include <QImageReader>
#include <QFileDialog>
#include <QMessageBox>

/* ==================== 样式表定义 ==================== */

QString checkBoxStyle = R"(
    QCheckBox {
        color: white;
        font-size: 14px;
        font-weight: bold;
        spacing: 8px;
        padding: 5px;
    }
    QCheckBox::indicator {
        width: 24px;
        height: 24px;
        border-radius: 6px;
        border: 2px solid #c0c4cc;
        background-color: #f5f7fa;
    }
    QCheckBox::indicator:hover {
        border: 2px solid #409eff;
        background-color: #ecf5ff;
    }
    QCheckBox::indicator:checked {
        border: 2px solid #409eff;
        background-color: #409eff;
        image: url(:/icons/check-white.png);
    }
    QCheckBox::indicator:checked:hover {
        background-color: #66b1ff;
        border: 2px solid #66b1ff;
    }
    QCheckBox::indicator:pressed {
        background-color: #3a8ee6;
    }
)";

static const QString transparentBtnStyle = R"(
    QPushButton {
        background-color: transparent;
        border: none;
    }
    QPushButton:hover {
        background-color: rgba(255, 255, 255, 30);
    }
    QPushButton:pressed {
        background-color: rgba(255, 255, 255, 60);
    }
    QPushButton:disabled {
        background-color: transparent;
    }
)";

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    this->setGeometry(0, 0, 800, 480);

    /* ========== 视频预览标签 ========== */
    videoLabel = new QLabel(this);
    videoLabel->setText("点击\"开始采集\"启动相机");
    videoLabel->setStyleSheet("color: #888; font-size: 16px;");
    videoLabel->setAlignment(Qt::AlignCenter);
    videoLabel->resize(640, 480);
    videoLabel->setStyleSheet(
        "QLabel {"
        "  color: #888;"
        "  font-size: 16px;"
        "  background-color: #1a1a1a;"
        "  border: 2px solid #333;"
        "  border-radius: 8px;"
        "}"
    );


    /* ========== 控制按钮 ========== */
    // 开始/停止采集按钮
    //    startCaptureButton = new QPushButton(this);
    //    startCaptureButton->setCheckable(true);
    //    startCaptureButton->setText("开始采集");
    //    startCaptureButton->setStyleSheet(
    //        "QPushButton {"
    //        "  background-color: #409eff;"
    //        "  color: white;"
    //        "  font-size: 14px;"
    //        "  font-weight: bold;"
    //        "  border-radius: 10px;"
    //        "  padding: -1;"
    //        "}"
    //        "QPushButton:pressed { background-color: #3a8ee6; }"
    //        "QPushButton:checked { background-color: #f56c6c; }"
    //    );

    // 拍照按钮
    btnPhoto_ = new QPushButton(this);
    btnPhoto_->setText("拍照");
    btnPhoto_->setStyleSheet(
        "QPushButton {"
        "  background-color: #67c23a;"
        "  color: white;"
        "  font-size: 14px;"
        "  font-weight: bold;"
        "  border-radius: 10px;"
        "  padding: -1 "
        "}"
        "QPushButton:pressed { background-color: #5daf34; }"
        "QPushButton:disabled {"
        "  background-color: #606266;"
        "  color: #a6a9ad;"
        "}"
    );
    // 拍照按钮
    btnVideo_ = new QPushButton(this);
    btnVideo_->setText("录像");
    btnVideo_->setStyleSheet(
        "QPushButton {"
        "  background-color: #FF3333;"
        "  color: white;"
        "  font-size: 14px;"
        "  font-weight: bold;"
        "  border-radius: 10px;"
        "  padding: -1 "
        "}"
        "QPushButton:pressed { background-color: #5daf34; }"
        "QPushButton:disabled {"
        "  background-color: #606266;"
        "  color: #a6a9ad;"
        "}"
    );
    //相册按钮
    btnAlbum_ = new QPushButton(this);
    btnAlbum_->setText("相册");
    btnAlbum_->setStyleSheet(
            "QPushButton {"
            "  background-color: #e6a23c;"      // 橙色区分功能
            "  color: white;"
            "  border-radius: 10px;"
            "  padding: -1;"

            "}"
            "QPushButton:pressed { background-color: #cf9236; }"
            "QPushButton:disabled {"
            "  background-color: #606266;"
            "  color: #a6a9ad;"
            "}"
        );

    // 退出按钮
    quitButton = new QPushButton(this);
    quitButton->setText("退出");
    quitButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #ff6b6b;"
        "  color: white;"
        "  border-radius: 10px;"
        "  padding: 8px 16px;"
        "}"
        "QPushButton:pressed { background-color: #c0392b; }"
    );

    btnPrev_ = new QPushButton(this);
    btnPrev_->setStyleSheet(transparentBtnStyle);
    btnPrev_->setCursor(Qt::PointingHandCursor);
    btnPrev_->setToolTip("上一张");  // 鼠标悬停提示
    btnPrev_->hide();           // 初始隐藏

    btnNext_ = new QPushButton(this);
    btnNext_->setStyleSheet(transparentBtnStyle);
    btnNext_->setCursor(Qt::PointingHandCursor);
    btnNext_->setToolTip("下一张");
    btnNext_->hide();

    // 状态标签
    labelStatus_ = new QLabel(this);
    updateStatus("正在采集...");
    labelStatus_->setStyleSheet("color: #aaa; font-size: 12px;");
    labelStatus_->setAlignment(Qt::AlignCenter);

    /* ========== 设置背景颜色 ========== */
    QColor color = QColor(Qt::black);
    QPalette p;
    p.setColor(QPalette::Window, color);
    this->setPalette(p);

    /* ========== 初始化采集线程 ========== */
    captureThread = new CaptureThread(this);

    /* ========== 信号连接 ========== */
    // 原有连接
//    connect(startCaptureButton, SIGNAL(clicked(bool)), this, SLOT(startCaptureButtonClicked(bool)));
    connect(captureThread, SIGNAL(imageReady(QImage)), this, SLOT(showImage(QImage)));
    connect(quitButton, SIGNAL(clicked()), this, SLOT(quitCapture()));

    // 拍照连接
    connect(btnPhoto_, &QPushButton::clicked, this, &MainWindow::onBtnPhotoClicked);
    connect(captureThread, &CaptureThread::photoSaved, this, &MainWindow::onPhotoSaved);
    connect(captureThread, &CaptureThread::captureError, this, &MainWindow::onCaptureError);
    //Album连接
    connect(btnAlbum_, &QPushButton::clicked, this, &MainWindow::onBtnAlbumClicked);
    connect(btnNext_,&QPushButton::clicked,this,&MainWindow::onBtnNextClicked);
    connect(btnPrev_,&QPushButton::clicked,this,&MainWindow::onBtnPrevClicked);
    //视频链接
    connect(btnVideo_, &QPushButton::clicked, this, &MainWindow::onBtnvideoClicked);
    connect(captureThread,&CaptureThread::recordStarted,this,&MainWindow::onRecodeStarted);
    connect(captureThread,&CaptureThread::recordStopped,this,&MainWindow::onRecodeStopped);

    // 默认勾选本地显示

    captureThread->setLocalDisplay(true);
    captureThread->startCapture();
}

MainWindow::~MainWindow()
{
    if (captureThread) {
        captureThread->stopCapture();
        captureThread->wait(3000);
    }
}

/* ==================== 布局调整 ==================== */

void MainWindow::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);

    int w = this->width();
    int h = this->height();

    // 预览窗口居中，上方留状态栏空间
    videoLabel->move((w - 640) / 2, 40);
    videoLabel->resize(640, 360);

    // 状态栏在预览上方
    labelStatus_->move(10, 10);
    labelStatus_->resize(w - 20, 24);

    // 上一张按钮：videoLabel左侧
    btnPrev_->move((w - 640)/2, 40);
    btnPrev_->resize(80, videoLabel->height());

    // 下一张按钮：videoLabel右侧
    btnNext_->move((w - 640)/2 + videoLabel->width() - 80,40);
    btnNext_->resize(80, videoLabel->height());

    // 右侧控制区

    // 底部按钮区
    int btnY = h - 70;
//    startCaptureButton->move((w - 500) / 2, btnY);
//    startCaptureButton->resize(160, 50);

    btnAlbum_->move((w - 500) / 2 , btnY);
    btnAlbum_->resize(140, 50);

    btnPhoto_->move(5, btnY/2-50);
    btnPhoto_->resize(70,50);

    btnVideo_->move(5, btnY/2+50);
    btnVideo_->resize(70,50);

    quitButton->move((w - 500) / 2 + 340, btnY);
    quitButton->resize(140, 50);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (captureThread && captureThread->isCapturing_) {
        captureThread->stopCapture();
        captureThread->wait(2000);
    }
    event->accept();
}

/* ==================== 槽函数实现 ==================== */

void MainWindow::showImage(QImage image)
{
    // 缩放并显示，保持比例
    QPixmap pixmap = QPixmap::fromImage(image).scaled(
        videoLabel->size(),
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );
    videoLabel->setPixmap(pixmap);
    videoLabel->setText("");  // 清除提示文字
}

//void MainWindow::startCaptureButtonClicked(bool start)
//{
//    if (start) {
//        // 启动采集
//        isCapturing_ = true;
//        startCaptureButton->setText("停止采集");
//        btnPhoto_->setEnabled(true);
//        updateStatus("正在采集...");
//        captureThread->startCapture();
//    } else {
//        // 停止采集
//        isCapturing_ = false;
//        startCaptureButton->setText("开始采集");
//        btnPhoto_->setEnabled(false);
//        updateStatus("已停止");
//        captureThread->stopCapture();
//        videoLabel->setText("点击\"开始采集\"启动相机");
//        videoLabel->setPixmap(QPixmap());  // 清除图像
//    }
//}





void MainWindow::quitCapture()
{
    QApplication::quit();
}

/* ==================== 拍照功能 ==================== */

void MainWindow::onBtnPhotoClicked()
{
    if (!captureThread->isCapturing_) return;
    qDebug()<<"clicked"<<endl;

    // 生成保存路径
    QString dirPath = "/mnt/sdcard/DCIM";
    QDir dir(dirPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QString filename = QString("IMG_%1.jpg")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    QString fullPath = dir.filePath(filename);

    // 触发拍照
    captureThread->takePhoto(fullPath);

    // 视觉反馈
    btnPhoto_->setEnabled(false);
    updateStatus("拍照中...");
    QTimer::singleShot(500, this, [this]() {
        btnPhoto_->setEnabled(true);
    });
}

void MainWindow::onPhotoSaved(QString path)
{
    QFileInfo info(path);
    QString msg = QString("已保存: %1 (%2)")
        .arg(info.fileName())
        .arg(formatFileSize(info.size()));

    updateStatus(msg, false);

    // 3秒后恢复状态
    QTimer::singleShot(3000, this, [this]() {
        if (captureThread->isCapturing_) {
            updateStatus("正在采集...");
        } else {
            updateStatus("就绪");
        }
    });
}

void MainWindow::onBtnAlbumClicked()
{
    if (!isAlbum_) {
            // 进入相册模式
            enterAlbumMode();
            isAlbum_=true;
            btnAlbum_->setText("关闭相册");
        } else {
            // 退出相册模式，恢复采集
            exitAlbumMode();
            isAlbum_=false;
            btnAlbum_->setText("相册");
            updateStatus("正在采集...");
    }
}

void MainWindow::onBtnPrevClicked()
{
     if (albumCurrentIndex_ > 0) {
            albumCurrentIndex_--;
            loadAlbumImage(albumCurrentIndex_);
            refreshAlbumButtons();
      }
}
void MainWindow::onBtnNextClicked()
{
     if (albumCurrentIndex_ <albumFiles_.size()-1) {
            albumCurrentIndex_++;
            loadAlbumImage(albumCurrentIndex_);
            refreshAlbumButtons();
     }
}

void MainWindow::onBtnvideoClicked()
{
    if (!captureThread->isCapturing_) return;
    qDebug()<<"clicked"<<endl;

    // 生成保存路径
    QString dirPath = "/mnt/sdcard/VIDEO";
    QDir dir(dirPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QString filename = QString("IMG_%1.jpg")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    QString fullPath = dir.filePath(filename);

    // 如果正在录像，则停止；否则开始录像
     if (captureThread->isRecording()) {
         captureThread->stopRecord();  // 触发 stop → 发射 recordStopped
         btnAlbum_->setEnabled(true);
     } else {
         // 生成保存路径
         QString dirPath = "/mnt/sdcard/VIDEO";
         QDir dir(dirPath);
         if (!dir.exists()) {
             dir.mkpath(".");
         }

         QString filename = QString("VID_%1.mp4")
             .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
         QString fullPath = dir.filePath(filename);


         // 触发录像开始
         captureThread->startRecord(fullPath);
     }

}
void MainWindow::onRecodeStarted()
{
    // 更新按钮为停止状态
       btnVideo_->setText("停止");
       btnVideo_->setStyleSheet(
           "QPushButton {"
           "  background-color: #f56c6c;"      // 红色表示可停止
           "  color: white;"
           "  font-size: 14px;"
           "  font-weight: bold;"
           "  border-radius: 10px;"
           "  padding: -1;"
           "}"
           "QPushButton:pressed { background-color: #c0392b; }"
       );

       // 录像期间禁用拍照和相册按钮
       btnPhoto_->setEnabled(false);
       btnAlbum_->setEnabled(false);

       updateStatus("● 正在录像...", false);
}

void MainWindow::onRecodeStopped(QString recordPath)
{
    // 恢复按钮为开始录像状态
    btnVideo_->setText("录像");
    btnVideo_->setStyleSheet(
        "QPushButton {"
        "  background-color: #FF3333;"
        "  color: white;"
        "  font-size: 14px;"
        "  font-weight: bold;"
        "  border-radius: 10px;"
        "  padding: -1;"
        "}"
        "QPushButton:pressed { background-color: #5daf34; }"
        "QPushButton:disabled {"
        "  background-color: #606266;"
        "  color: #a6a9ad;"
        "}"
    );

    // 恢复拍照和相册按钮
    btnPhoto_->setEnabled(true);
    btnAlbum_->setEnabled(true);

    // 显示保存信息
    QFileInfo info(recordPath);
    QString msg = QString("录像已保存: %1 (%2)")
        .arg(info.fileName())
        .arg(formatFileSize(info.size()));

    updateStatus(msg, false);

    //1秒后恢复采集状态提示
    QTimer::singleShot(1000, this, [this]() {
        if (captureThread->isCapturing_) {
            updateStatus("正在采集...");
        }
    });
}

void MainWindow::enterAlbumMode()
{
     captureThread->stopCapture();
     QString albumPath = "/mnt/sdcard/DCIM";
     QDir dir(albumPath);

     QStringList nameFilters;
     QList<QByteArray> formats = QImageReader::supportedImageFormats();
     for (const QByteArray &fmt : formats) {
         nameFilters << "*." + QString(fmt).toLower();
     }

     albumFiles_ = dir.entryList(nameFilters, QDir::Files | QDir::NoDotAndDotDot, QDir::Name);

     for (int i = 0; i < albumFiles_.size(); ++i) {
             albumFiles_[i] = dir.absoluteFilePath(albumFiles_[i]);
         }

     if (albumFiles_.isEmpty()) {
        QMessageBox::information(this, "相册为空", "目录中没有找到图片:\n" + albumPath);
        captureThread->startCapture();
     }

     btnPhoto_->setEnabled(false);  // 相册模式下禁用拍照


     btnPrev_->show();
     btnNext_->show();
     btnPrev_->raise();  // 确保在videoLabel上层
     btnNext_->raise();

         // 5. 加载第一张（最新的，因为按名称排序，最后的是最新的）
     albumCurrentIndex_ =0;

     videoLabel->clear();//刷新显示区域
     loadAlbumImage(albumCurrentIndex_);
     refreshAlbumButtons();


}

void MainWindow::exitAlbumMode(){

    btnPrev_->hide();
    btnNext_->hide();
    captureThread->startCapture();
    btnPhoto_->setEnabled(true);
    // 清空列表释放内存
    albumFiles_.clear();
    albumCurrentIndex_ = -1;

}

void MainWindow::loadAlbumImage(int index)
{
     if (index < 0 || index >= albumFiles_.size()) return;

     QString path = albumFiles_[index];
     QImage image(path);

     if (image.isNull()) {
         updateStatus("无法加载: " + QFileInfo(path).fileName(), true);
         return;
     }

     // 显示图片（复用showImage，保持缩放逻辑一致）
     showImage(image);

     // 更新状态栏
     QFileInfo info(path);
     QString msg = QString("相册 [%1/%2]: %3 (%4)")
         .arg(index + 1)
         .arg(albumFiles_.size())
         .arg(info.fileName())
         .arg(formatFileSize(info.size()));
      updateStatus(msg);
}
void MainWindow::refreshAlbumButtons()
{
    btnPrev_->setEnabled(albumCurrentIndex_ > 0);
    btnNext_->setEnabled(albumCurrentIndex_ < albumFiles_.size() - 1);

    // 视觉反馈
    if (!btnPrev_->isEnabled()) {
        btnPrev_->setStyleSheet("QPushButton { background-color: transparent; border: none; }");
    } else {
        btnPrev_->setStyleSheet(transparentBtnStyle);
    }

    if (!btnNext_->isEnabled()) {
        btnNext_->setStyleSheet("QPushButton { background-color: transparent; border: none; }");
    } else {
        btnNext_->setStyleSheet(transparentBtnStyle);
    }
}
void MainWindow::onCaptureError(QString msg)
{
    updateStatus("错误: " + msg, true);
    qWarning() << "Capture error:" << msg;
}

/* ==================== 工具函数 ==================== */

void MainWindow::updateStatus(QString text, bool isError)
{
    labelStatus_->setText(text);
    if (isError) {
        labelStatus_->setStyleSheet("color: #f56c6c; font-size: 12px; font-weight: bold;");
    } else {
        labelStatus_->setStyleSheet("color: #67c23a; font-size: 12px;");
    }
}

QString MainWindow::formatFileSize(qint64 bytes)
{
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
#if __arm__
    if (event->key()  ==  Qt::Key_VolumeDown)
    {
        if(isAlbum_==false){
        onBtnPhotoClicked();
        }
    }
#endif
}


