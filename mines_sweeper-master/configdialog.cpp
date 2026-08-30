#include "configdialog.h"
#include "ui_configdialog.h"
#include <QIntValidator>
#include <QDebug>

configDialog::configDialog(int cw, int ch, int cm, QWidget *parent) :

    QDialog(parent),

    _width(cw),
    _height(ch),
    _mines(cm),
    difficultyStatus(-1),

    ui(new Ui::configDialog)
{
    ui->setupUi(this);

    QIntValidator * vaildW = new QIntValidator(1, 30, this);//��������
    QIntValidator * vaildH = new QIntValidator(1, 19, this);//�߶�����
    QIntValidator * vaildM = new QIntValidator(0, 570, this);//������������(���滹�и�������)



    connect(ui->buttonBox,SIGNAL(accepted()),this,SLOT(accept()));
    connect(ui->buttonBox,SIGNAL(rejected()),this,SLOT(reject()));
}

//�Զ���accept�ۣ�����Ҫ���ø����Ĳۣ����ڵ����Լ��ģ������Ժ���Ҫ��ʾ���ø����ۣ�
void configDialog::accept(){


    if(difficultyStatus==0){
        _width=10;
        _height=10;
        _mines=(_height*_width)/12;
    }
    if(difficultyStatus==1){
        _width=20;
        _height=10;
        _mines=(_height*_width)/10;
    }
    if(difficultyStatus==2){
        _width=15;
        _height=10;
        _mines=(_height*_width)/10;
    }

    QDialog::accept();//�Լ���ʵ���Ժ�������ʾ���ø�����accept��
}

configDialog::~configDialog() {
    delete ui;
}

void configDialog::on_easyButton_clicked()
{
    difficultyStatus=easy;
    qDebug()<<difficultyStatus<<endl;
}
void configDialog::on_hardButton_3_clicked()
{
    difficultyStatus=hard;
    qDebug()<<difficultyStatus<<endl;

}

void configDialog::on_mediumButton_2_clicked()
{
    difficultyStatus=med;
    qDebug()<<difficultyStatus<<endl;
}
