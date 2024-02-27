#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QString>
#include <QFileDialog>
#include <QDir>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QFile f(":/ui/image/key.png");
    if(f.open(QFile::ReadOnly))
    {
        key = f.readAll();
        f.close();
        ui->plainTextEdit->appendPlainText("key值读取完成\n");
    }
    m_file = NULL;
}

MainWindow::~MainWindow()
{
    if(m_file)
    {
        m_file->close();
        delete m_file;
    }
    key.clear();
    file_all.clear();
    delete ui;
}

void MainWindow::on_btnOpen_clicked()
{
    QString curPath = QDir::currentPath();
    QString dlgTitle = "Select a destination file";
    QString filter = "media file(*.mp3 *.mp4);;";
    QString filename = QFileDialog::getOpenFileName(this, dlgTitle, curPath, filter);
    if(filename.isEmpty())
    {
        ui->plainTextEdit->appendPlainText("选择路径为空，请重新选择\n");
        return;
    }

    m_file = new QFile(filename);
    if(m_file != NULL)
    {
        ui->lineEdit->setText(filename);
        ui->plainTextEdit->appendPlainText("文件加载成功...\n");
        if(m_file->open(QFile::ReadOnly))
        {
            fsize = m_file->size();
            pos = uint16_t(fsize & 0xFFFF);
            ui->plainTextEdit->appendPlainText("参数获取完成\n");
            ui->plainTextEdit->appendPlainText("fsize = " + QString::number(fsize, 10));
            ui->plainTextEdit->appendPlainText("pos = " + QString::number(pos));
        }
    }
    else
    {
        ui->plainTextEdit->appendPlainText("读取错误，请检查bug.\n");
        return;
    }
}

void MainWindow::on_btnCode_clicked()
{
    file_all = m_file->readAll();
    ui->plainTextEdit->appendPlainText("readAll大小\t" + QString::number(file_all.size()));

    int i = 0;
    for (; i < file_all.size(); ++i) {
        file_all[i] = file_all[i] ^ key.at((pos + i) % key.size());
    }
    ui->plainTextEdit->appendPlainText("转换运算完成，运算次数\t" + QString::number(i));
}


void MainWindow::on_btnSave_clicked()
{
    QString curPath = QDir::currentPath();
    QString title = "Save the file";
    QString filter = "media file(*.mp3 *.mp4);;";
    QString filename = QFileDialog::getSaveFileName(this, title, curPath, filter);
    if(filename.isEmpty())
    {
        ui->plainTextEdit->appendPlainText("没有指定文件名，请重新输入\n");
        return;
    }

    QFile file(filename);
    file.open(QIODevice::WriteOnly);
    file.write(file_all, file_all.length());

    file.close();
    ui->lineEdit->clear();
    ui->plainTextEdit->appendPlainText("文件保存成功!\n");
}


void MainWindow::on_btnExit_clicked() { exit(0); }
