#include "mainwindow.h"
#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QElapsedTimer>
#include <QApplication>
#include <QFrame>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUI();
    loadStyleSheet();
    m_progressTimer = new QTimer(this);
    m_progressTimer->setInterval(500);
    m_jumpDebounceTimer = new QTimer(this);
    m_jumpDebounceTimer->setSingleShot(true);  // 单次触发
    connect(m_jumpDebounceTimer, &QTimer::timeout, this, &MainWindow::jumpToPlay);
    connect(m_progressTimer, &QTimer::timeout, this, &MainWindow::updateProgress);
    // 初始化播放器进程
    m_playerProcess = new QProcess(this);
    m_ctlProcess= new QProcess(this);
    connect(m_playerProcess, &QProcess::started, this, &MainWindow::onProcessStarted);
    connect(m_playerProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onProcessFinished);
    connect(m_playerProcess, &QProcess::errorOccurred, this, &MainWindow::onProcessError);
    connect(m_playerProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::onReadyReadStandardOutput);
    connect(m_playerProcess, &QProcess::readyReadStandardError, this, &MainWindow::onReadyReadStandardError);

    // 设置默认路径
    m_musicDir = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    if (m_musicDir.isEmpty()) {
        m_musicDir = "/home/root/music_source";
    }

    m_pathEdit->setText(m_musicDir);

    // 查找播放器可执行文件
    m_playerExecutable ="/home/root/arm_mp3_player/alsa_mp3_player_armV2";
    qDebug()<<m_playerExecutable<<endl;
    if (!QFile::exists(m_playerExecutable)) {
        m_playerExecutable = "alsa_mp3_player";  // 尝试PATH环境变量
    }

    // 文件监控
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &MainWindow::onDirectoryChanged);

    scanDirectory();

    setWindowTitle("MP3 控制器");
    setFixedSize(1024, 600);
}

MainWindow::~MainWindow()
{
    killCurrentProcess();  // 确保退出时停止播放
}

void MainWindow::setupUI()
{

    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setSpacing(25);
    mainLayout->setContentsMargins(40, 35, 40, 35);

    // 标题
    auto *title = new QLabel("AUDIO PLAYER", this);
    title->setObjectName("titleLabel");
    title->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(title);

    // 分隔线
    auto *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setObjectName("separator");
    mainLayout->addWidget(line);

    // 路径选择区域
    auto *pathLayout = new QHBoxLayout();
    pathLayout->setSpacing(10);

    auto *pathLabel = new QLabel("目录:", this);
    pathLabel->setObjectName("pathLabel");
    pathLayout->addWidget(pathLabel);

    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setObjectName("pathEdit");
    m_pathEdit->setReadOnly(true);
    pathLayout->addWidget(m_pathEdit, 1);

    m_browseBtn = new QPushButton("浏览", this);
    m_browseBtn->setObjectName("browseBtn");
    m_browseBtn->setFixedWidth(70);
    connect(m_browseBtn, &QPushButton::clicked, this, &MainWindow::onBrowseClicked);
    pathLayout->addWidget(m_browseBtn);

    mainLayout->addLayout(pathLayout);

    // 歌曲选择区域
    auto *songLayout = new QHBoxLayout();
    songLayout->setSpacing(15);

    auto *songLabel = new QLabel("曲目:", this);
    songLabel->setObjectName("songLabel");
    songLabel->setFixedWidth(40);
    songLayout->addWidget(songLabel);

    m_songCombo = new QComboBox(this);
    m_songCombo->setObjectName("songCombo");
    m_songCombo->setMinimumWidth(350);
//    m_songCombo->setPlaceholderText("选择要播放的MP3文件...");
    connect(m_songCombo, QOverload<int>::of(&QComboBox::activated),
            this, &MainWindow::onSongSelected);

    songLayout->addWidget(m_songCombo, 1);

    // 刷新按钮
    auto *refreshBtn = new QPushButton("⟳", this);
    refreshBtn->setObjectName("iconBtn");
    refreshBtn->setToolTip("刷新列表");
    refreshBtn->setFixedSize(32, 32);
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::scanDirectory);
    songLayout->addWidget(refreshBtn);

    mainLayout->addLayout(songLayout);

    // 信息显示
    m_infoLabel = new QLabel("准备就绪", this);
    m_infoLabel->setObjectName("infoLabel");
    m_infoLabel->setAlignment(Qt::AlignCenter);
    m_infoLabel->setMinimumHeight(60);
    m_infoLabel->setWordWrap(true);
    mainLayout->addWidget(m_infoLabel);

    // ===== 进度条区域 =====
       auto *progressLayout = new QHBoxLayout();
    progressLayout->setSpacing(12);

    m_timeLabel = new QLabel("00:00 / 00:00", this);
    m_timeLabel->setObjectName("timeLabel");
    progressLayout->addWidget(m_timeLabel);

    m_progressSlider = new QSlider(Qt::Horizontal, this);
    m_progressSlider->setObjectName("progressSlider");
    m_progressSlider->setRange(0, 100);
    m_progressSlider->setValue(0);
    connect(m_progressSlider,&QSlider::sliderMoved,this,&MainWindow::onSliderMoved);
    // 当前仅作显示，不连接 slider 拖动信号；如需拖动跳转可后续扩展
    progressLayout->addWidget(m_progressSlider, 1);

    mainLayout->addLayout(progressLayout);

    mainLayout->addStretch();

    // 控制按钮
    auto *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(20);
    btnLayout->addStretch();

    m_playBtn = new QPushButton("▶ 播放", this);
    m_playBtn->setObjectName("playBtn");
    m_playBtn->setFixedSize(130, 48);
    m_playBtn->setEnabled(false);
    connect(m_playBtn, &QPushButton::clicked, this, &MainWindow::onPlayClicked);
    btnLayout->addWidget(m_playBtn);

    m_pauseBtn = new QPushButton("!!暂停!!", this);
    m_pauseBtn->setObjectName("pauseBtn");
    m_pauseBtn->setFixedSize(130, 48);
    m_pauseBtn->setEnabled(false);
    connect(m_pauseBtn, &QPushButton::clicked, this, &MainWindow::onPauseClicked);
    btnLayout->addWidget(m_pauseBtn);

    m_stopBtn = new QPushButton(" 停止", this);
    m_stopBtn->setObjectName("stopBtn");
    m_stopBtn->setFixedSize(130, 48);
    m_stopBtn->setEnabled(false);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    btnLayout->addWidget(m_stopBtn);

    m_exitBtn = new QPushButton(" 退出", this);
    m_exitBtn->setObjectName("stopBtn");
    m_exitBtn->setFixedSize(130, 48);
    m_exitBtn->setEnabled(true);
    connect(m_exitBtn, &QPushButton::clicked, this, &MainWindow::onExitClicked);
    btnLayout->addWidget(m_exitBtn);


    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    // 状态栏
    m_statusLabel = new QLabel("就绪", this);
    m_statusLabel->setObjectName("statusLabel");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_statusLabel);
}

void MainWindow::loadStyleSheet()
{
    // 移除所有 letter-spacing 和 transform，使用兼容属性
    setStyleSheet(R"(
        QMainWindow {
            background-color: #121212;
        }

        QWidget {
            background-color: #121212;
        }

        #titleLabel {
            color: #ffffff;
            font-size: 26px;
            font-weight: bold;
            padding-bottom: 5px;
        }

        #separator {
            background-color: #282828;
            max-height: 1px;
            margin: 0 60px 10px 60px;
        }

        #pathLabel, #songLabel {
            color: #b3b3b3;
            font-size: 13px;
            font-weight: bold;
        }

        #pathEdit {
            background-color: #1e1e1e;
            color: #ffffff;
            border: 1px solid #333333;
            border-radius: 6px;
            padding: 8px 12px;
            font-size: 12px;
        }

        #browseBtn {
            background-color: #333333;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            font-size: 12px;
            font-weight: bold;
            padding: 5px;
        }

        #browseBtn:hover {
            background-color: #404040;
        }

        #browseBtn:pressed {
            background-color: #282828;
        }

        #songCombo {
            background-color: #1e1e1e;
            color: #ffffff;
            border: 1px solid #333333;
            border-radius: 8px;
            padding: 10px;
            font-size: 13px;
            min-height: 24px;
        }

        #songCombo:hover {
            border-color: #404040;
        }

        #songCombo::drop-down {
            border: none;
            width: 30px;
        }

        #songCombo::down-arrow {
            image: none;
            border-left: 6px solid transparent;
            border-right: 6px solid transparent;
            border-top: 6px solid #888888;
            margin-right: 12px;
        }

        #songCombo QAbstractItemView {
            background-color: #1e1e1e;
            color: #ffffff;
            border: 1px solid #333333;
            selection-background-color: #2a2a2a;
            outline: none;
            padding: 5px;
        }

        #songCombo QAbstractItemView::item {
            padding: 8px;
            border-radius: 4px;
        }

        #songCombo QAbstractItemView::item:selected {
            background-color: #333333;
        }

        #infoLabel {
            color: #535353;
            font-size: 14px;
            font-weight: bold;
            padding: 15px;
            background-color: #181818;
            border-radius: 8px;
            margin: 10px 0;
        }

        #playBtn {
            background-color: #1db954;
            color: #000000;
            border: none;
            border-radius: 24px;
            font-size: 15px;
            font-weight: bold;
            padding: 10px;
        }

        #playBtn:hover {
            background-color: #1ed760;
            border: 2px solid #ffffff;
        }

        #playBtn:pressed {
            background-color: #169c45;
        }

        #playBtn:disabled {
            background-color: #282828;
            color: #555555;
            border: none;
        }

        #stopBtn {
            background-color: transparent;
            color: #ffffff;
            border: 2px solid #535353;
            border-radius: 24px;
            font-size: 15px;
            font-weight: bold;
            padding: 10px;
        }

        #stopBtn:hover {
            border-color: #ffffff;
            background-color: #333333;
        }

        #stopBtn:pressed {
            background-color: #444444;
        }

        #stopBtn:disabled {
            border-color: #333333;
            color: #444444;
            background-color: transparent;
        }
        #pauseBtn {
            background-color: transparent;
            color: #ffffff;
            border: 2px solid #535353;
            border-radius: 24px;
            font-size: 15px;
            font-weight: bold;
            padding: 10px;
                 }

        #pauseBtn:hover {
            border-color: #ffffff;
            background-color: #333333;
              }

        #pauseBtn:pressed {
            background-color: #444444;
                }

        #pauseBtn:disabled {
            border-color: #333333;
            color: #444444;
            background-color: transparent;
               }

        #iconBtn {
            background-color: #1e1e1e;
            color: #888888;
            border: 1px solid #333333;
            border-radius: 16px;
            font-size: 14px;
            font-weight: bold;
        }

        #iconBtn:hover {
            background-color: #282828;
            color: #ffffff;
            border-color: #404040;
        }

        #iconBtn:pressed {
            background-color: #151515;
        }

        #statusLabel {
            color: #404040;
            font-size: 10px;
            font-weight: bold;
            margin-top: 5px;
        }
        #timeLabel {
            color: #b3b3b3;
            font-size: 11px;
            font-family: "Consolas", "Monaco", monospace;
            min-width: 90px;
        }

            #progressSlider {
            background: transparent;
        }

            #progressSlider::groove:horizontal {
            height: 4px;
            background: #333333;
            border-radius: 2px;
        }

            #progressSlider::sub-page:horizontal {
            background: #1db954;
            border-radius: 2px;
        }

            #progressSlider::add-page:horizontal {
            background: #333333;
            border-radius: 2px;
        }

            #progressSlider::handle:horizontal {
            width: 12px;
            height: 12px;
            margin: -4px 0;
            background: #ffffff;
            border-radius: 6px;
            border: none;
         }

           #progressSlider::handle:horizontal:hover {
           width: 16px;
           height: 16px;
           margin: -6px 0;
           background: #1db954;
        }
    )");
}


void MainWindow::scanDirectory()
{
    m_songCombo->clear();

    // 修复：安全地移除监控
    QStringList dirs = m_watcher->directories();
    if (!dirs.isEmpty()) {
        m_watcher->removePaths(dirs);
    }

    if (QDir(m_musicDir).exists()) {
        m_watcher->addPath(m_musicDir);
    }

    QDir dir(m_musicDir);
    QStringList filters;
    filters << "*.mp3" << "*.MP3";
    dir.setNameFilters(filters);
    dir.setSorting(QDir::Name);

    QFileInfoList files = dir.entryInfoList();

    if (files.isEmpty()) {
        m_songCombo->addItem("— 目录中没有MP3文件 —");
        m_songCombo->setItemData(0, "", Qt::UserRole);
        m_songCombo->setEnabled(false);
        m_playBtn->setEnabled(false);
        m_statusLabel->setText(QString("监控目录: %1 (空)").arg(m_musicDir));
    } else {
        m_songCombo->setEnabled(true);
        // 添加默认提示项
        m_songCombo->addItem("— 请选择MP3文件 —");
        m_songCombo->setItemData(0, "", Qt::UserRole);

        for (const QFileInfo &file : files) {
            m_songCombo->addItem("♪ " + file.fileName(), file.absoluteFilePath());
        }

        m_songCombo->setCurrentIndex(0);
        m_statusLabel->setText(QString("已加载 %1 首歌曲").arg(files.count()));

        // 重要：此时播放按钮应保持禁用，直到用户选择有效文件
        m_playBtn->setEnabled(false);
    }
}
void MainWindow::onDirectoryChanged()
{
    // 延迟刷新避免频繁操作
    QTimer::singleShot(500, this, &MainWindow::scanDirectory);
}

void MainWindow::onSliderMoved(int value)
{
    m_progressTimer->stop();
    m_pendingJumpValue = value;
    m_jumpDebounceTimer->start(300);  // 300ms 内无新移动才执行
}

void MainWindow::jumpToPlay()
{
    m_progressTimer->start();
    is_loopPlay=false;
    int value = m_pendingJumpValue;
    if(next_index<0) return;
    QString filePath=m_songCombo->itemData(next_index).toString();
    if (filePath.isEmpty()) return;

    // 如果正在播放，先停止（实现切换中断功能）
    if (m_isPlaying) {
        killCurrentProcess();
        // 短暂延迟确保资源释放
        QApplication::processEvents();
    }
    // 启动新进程播放

    QStringList args;

    args << filePath<<QString::number(value);

    m_playerProcess->start(m_playerExecutable,args);

    if (!m_playerProcess->waitForStarted(3000)) {
        QMessageBox::critical(this, "错误",
            QString("无法启动播放器程序\n请确保 '%1' 存在且具有执行权限").arg(m_playerExecutable));
    }
    is_loopPlay=true;
    readFifo();
    m_playedSeconds = 0;
    m_progressTimer->start();
    m_playTimer.start();
    m_progressSlider->setMaximum(m_totalSeconds);// 开始计时
    m_progressSlider->setValue(value);
    m_playedSeconds=value;
    auto fmtTime = [](int seconds) {
        int m = seconds / 60;
        int s = seconds % 60;
        return QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
    };

    m_timeLabel->setText(QString("%1 / %2")
                         .arg(fmtTime(m_playedSeconds))
                         .arg(fmtTime(m_totalSeconds)));
    qDebug()<<args<<endl;
}

void MainWindow::onBrowseClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择音乐文件夹", m_musicDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty()) {
        m_musicDir = dir;
        m_pathEdit->setText(dir);
        scanDirectory();
    }
}

void MainWindow::killCurrentProcess()
{
    if (m_playerProcess && m_playerProcess->state() != QProcess::NotRunning) {
        m_playerProcess->terminate();  // 发送SIGTERM，C程序会捕获并优雅退出

        // 等待最多1秒
        if (!m_playerProcess->waitForFinished(1000)) {
            m_playerProcess->kill();  // 强制结束
            m_playerProcess->waitForFinished();
        }
    }
}

void MainWindow::pauseCurrentProcess()
{
    QStringList args;
    args << "pause";
    qDebug()<<"cout: "<<args<<endl;
    m_ctlProcess->start(m_playerExecutable,args);
}

void MainWindow::resumeCurrentprocess()
{
    QStringList args;
    args << "resume";
    qDebug()<<"cout: "<<args<<endl;
    m_ctlProcess->start(m_playerExecutable,args);
}

void MainWindow::readFifo()
{
    QElapsedTimer timer;
    timer.start();
    file.setFileName((QString)FIFO_INFO_PATH);
    int ret = file.open(QIODevice::ReadOnly | QIODevice::Text);
    while(!ret){
       ret = file.open(QIODevice::ReadOnly | QIODevice::Text);
       if ((int)timer.elapsed()>2000){
           return;
       };
    }
    qDebug()<<"open file ret:"<<ret<<endl;
    QByteArray musicTime=file.readAll();
    m_totalSeconds=musicTime.trimmed().toInt();
    qDebug()<<m_totalSeconds<<endl;
    file.close();

}

void MainWindow::updateProgress()
{
    if (m_totalSeconds <= 0) return;

       // 当前已播秒数 = 之前累积的 + 当前这一段实际播放时长
       int currentSeconds = m_playedSeconds;
       if (m_playTimer.isValid()) {
           currentSeconds += static_cast<int>(m_playTimer.elapsed() / 1000);
       }

       // 防越界
       if (currentSeconds > m_totalSeconds)
           currentSeconds = m_totalSeconds;

       m_progressSlider->setMaximum(m_totalSeconds);
       m_progressSlider->setValue(currentSeconds);

       auto fmtTime = [](int seconds) {
           int m = seconds / 60;
           int s = seconds % 60;
           return QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
       };

       m_timeLabel->setText(QString("%1 / %2")
                            .arg(fmtTime(currentSeconds))
                            .arg(fmtTime(m_totalSeconds)));
}

void MainWindow::loopPlay()
{
    if(is_loopPlay){
    next_index+=1;
    if(next_index<0) return;
    QString filePath=m_songCombo->itemData(next_index).toString();
    if (filePath.isEmpty()) return;

    // 如果正在播放，先停止（实现切换中断功能）
    if (m_isPlaying) {
        killCurrentProcess();
        // 短暂延迟确保资源释放
        QApplication::processEvents();
    }
    // 启动新进程播放

    QStringList args;
    args << filePath;

    m_playerProcess->start(m_playerExecutable,args);  // 通过 nice 启动

    if (!m_playerProcess->waitForStarted(3000)) {
        QMessageBox::critical(this, "错误",
            QString("无法启动播放器程序\n请确保 '%1' 存在且具有执行权限").arg(m_playerExecutable));
    }
    readFifo();
    m_playedSeconds = 0;
    m_progressTimer->start();
    m_playTimer.start();          // 开始计时
    m_progressSlider->setValue(0);
    m_timeLabel->setText("00:00 / 00:00");
    }
}

void MainWindow::onPlayClicked()
{
    int index = m_songCombo->currentIndex();
    next_index=index;
    if (index < 0) return;

    QString filePath = m_songCombo->itemData(index).toString();
    if (filePath.isEmpty()) return;

    // 如果正在播放，先停止（实现切换中断功能）
    if (m_isPlaying) {
        is_loopPlay=false;
        killCurrentProcess();
        // 短暂延迟确保资源释放
        QApplication::processEvents();
    }

    // 启动新进程播放

    QStringList args;
    args << filePath;

    m_playerProcess->start(m_playerExecutable,args);  // 通过 nice 启动

    if (!m_playerProcess->waitForStarted(3000)) {
        QMessageBox::critical(this, "错误",
            QString("无法启动播放器程序\n请确保 '%1' 存在且具有执行权限").arg(m_playerExecutable));
    }
//    sleep(1);
    readFifo();
    m_playedSeconds = 0;
    m_progressTimer->start();
    m_playTimer.start();          // 开始计时
    m_progressSlider->setValue(0);
    m_timeLabel->setText("00:00 / 00:00");
    is_loopPlay=true;
}

void MainWindow::onStopClicked()
{
    is_loopPlay=false;
    // 重置计时相关状态
    m_progressTimer->stop();
    m_playTimer.invalidate();
    m_playedSeconds = 0;
    m_totalSeconds = 0;

    m_progressSlider->setValue(0);
    m_timeLabel->setText("00:00 / 00:00");
    //重置暂停
    m_isPause=false;
    m_pauseBtn->setEnabled(false);
    m_pauseBtn->setText("!!暂停!!");
    killCurrentProcess();
}

void MainWindow::onPauseClicked()
{
    if(m_isPause==false){
        m_isPause=true;
        m_pauseBtn->setText("重新播放");
        pauseCurrentProcess();
        qDebug()<<m_isPause<<endl;
        if (m_playTimer.isValid()) {
                    m_playedSeconds += static_cast<int>(m_playTimer.elapsed() / 1000);
                    m_playTimer.invalidate();  // 停止计时器
                }
                m_progressTimer->stop();       // 停止刷新进度

    }else{

       m_isPause=false;
       m_pauseBtn->setText("!!暂停!!");
       resumeCurrentprocess();
       qDebug()<<m_isPause<<endl;
       m_playTimer.start();
       m_progressTimer->start();
    }


}

void MainWindow::onExitClicked()
{
    onStopClicked();
    close();
}

void MainWindow::onSongSelected(int index)
{
    if (index <= 0) {  // 第0项是提示文本，不是有效文件
        m_playBtn->setEnabled(false);
        return;
    }

    QString filePath = m_songCombo->itemData(index).toString();
    if (filePath.isEmpty()) {
        m_playBtn->setEnabled(false);
        return;
    }

    m_playBtn->setEnabled(true);

    // 如果正在播放，自动切换（中断当前播放新文件）
    if (m_isPlaying) {
        onPlayClicked();
    }
}
void MainWindow::onProcessStarted()
{
    m_isPlaying = true;
    m_playBtn->setEnabled(false);
    m_playBtn->setText("● 播放中");
    m_stopBtn->setEnabled(true);
    m_pauseBtn->setEnabled(true);

    QString fileName = m_songCombo->itemText(next_index);
    fileName.remove(0, 2);  // 移除前缀 "♪ "

    m_infoLabel->setText(QString("正在解码播放\n%1").arg(fileName));
    m_infoLabel->setStyleSheet("color: #1db954; background-color: #1a2f23; font-weight: 600;");
    m_statusLabel->setText("状态: 播放中");
}

void MainWindow::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    m_progressTimer->stop();
    m_playTimer.invalidate();
    m_playedSeconds = 0;
    m_totalSeconds = 0;
    //重置所有计时状态
    m_progressSlider->setValue(0);
    m_timeLabel->setText("00:00 / 00:00");
    m_isPlaying = false;
    m_playBtn->setEnabled(m_songCombo->count() > 0 &&
                         !m_songCombo->itemData(m_songCombo->currentIndex()).toString().isEmpty());
    m_playBtn->setText("▶ 播放");
    m_stopBtn->setEnabled(false);

    if (status == QProcess::NormalExit && exitCode == 0) {
        m_infoLabel->setText("播放完成");
        m_infoLabel->setStyleSheet("color: #888888; background-color: #181818;");
        m_statusLabel->setText("即将播放下一首");
        loopPlay();
    } else if (status == QProcess::CrashExit) {
        m_infoLabel->setText("播放被中断");
        m_infoLabel->setStyleSheet("color: #e74c3c; background-color: #2d1f1f;");
        m_statusLabel->setText("状态: 已停止");
    }
}

void MainWindow::onProcessError(QProcess::ProcessError error)
{
    QString errorMsg;
    switch (error) {
        case QProcess::FailedToStart:
            errorMsg = "播放器程序未能启动\n请检查程序路径和依赖库";
            break;
        case QProcess::Crashed:
            errorMsg = "播放器程序崩溃";
            break;
        default:
            errorMsg = "未知错误";
    }

    if (error != QProcess::Crashed) {  // Crashed通常在terminate时出现，不算真错误
        QMessageBox::critical(this, "播放错误", errorMsg);
        m_statusLabel->setText("状态: 错误");
    }
}

void MainWindow::onReadyReadStandardOutput()
{
    QString output = QString::fromLocal8Bit(m_playerProcess->readAllStandardOutput());
    // 可选：解析输出显示解码信息
    qDebug() << "Player Output:" << output;
}

void MainWindow::onReadyReadStandardError()
{
    QString error = QString::fromLocal8Bit(m_playerProcess->readAllStandardError());
    if (!error.isEmpty()) {
        qDebug() << "Player Error:" << error;
    }
}
