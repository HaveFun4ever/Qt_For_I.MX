#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProcess>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QFile>
#include <QSlider>
#include <QElapsedTimer>
#define FIFO_INFO_PATH "/tmp/music_format"

QT_BEGIN_NAMESPACE
class QComboBox;
class QPushButton;
class QLabel;
class QLineEdit;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onPlayClicked();
    void onStopClicked();
    void onPauseClicked();
    void onExitClicked();
    void onSongSelected(int index);
    void onBrowseClicked();
    void scanDirectory();
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onDirectoryChanged();
    void jumpToPlay();
    void onSliderMoved(int value);

private:
    void setupUI();
    void loadStyleSheet();
    void killCurrentProcess();  // 强制停止当前播放
    void pauseCurrentProcess();
    void resumeCurrentprocess();
    void readFifo();
    void updateProgress();
    void loopPlay();
    QFile file;
    QTimer *m_jumpDebounceTimer;
    int  m_pendingJumpValue;
    QProcess *m_playerProcess = nullptr;
    QProcess *m_ctlProcess = nullptr;
    QComboBox *m_songCombo = nullptr;
    QPushButton *m_playBtn = nullptr;
    QPushButton *m_stopBtn = nullptr;
    QPushButton *m_exitBtn = nullptr;
    QPushButton *m_pauseBtn = nullptr;
    QPushButton *m_browseBtn = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_infoLabel = nullptr;
    QLineEdit *m_pathEdit = nullptr;
    int next_index;
    QString m_musicDir;
    QString m_playerExecutable;
    QFileSystemWatcher *m_watcher = nullptr;
    QSlider *m_progressSlider;   // 进度条
    QLabel *m_timeLabel;         // 时间显示 00:00 / 00:00
    QTimer *m_progressTimer;     // 定时读取 FIFO
    int m_totalSeconds = 0;          // 从 FIFO 读取的总时长
    int m_playedSeconds = 0;         // 暂停前已累积的秒数
    QElapsedTimer m_playTimer;       // 播放时段计时器（用于计算当前这一段播了多久）
    bool m_isPause = false;      // 如果原来没声明，确认已有
    bool m_isPlaying = false;
    bool is_loopPlay=true;
};

#endif // MAINWINDOW_H
