#ifndef CONFIGDIALOG_H
#define CONFIGDIALOG_H

#include <QDialog>
#include <QDialogButtonBox>
#define easy 0;
#define hard 1;
#define med 2;

namespace Ui {
class configDialog;
}

class configDialog : public QDialog
{
    Q_OBJECT

public:
    explicit configDialog(int cw,int ch,int cm,QWidget *parent = 0);
    ~configDialog();

    int _width;
    int _height;
    int _mines;
    int difficultyStatus;

public slots:
    virtual void accept() override;

private slots:
    void on_easyButton_clicked();

    void on_hardButton_3_clicked();

    void on_mediumButton_2_clicked();

private:
    Ui::configDialog *ui;
};

#endif // CONFIGDIALOG_H
