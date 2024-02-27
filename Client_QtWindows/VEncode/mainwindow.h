#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFile>
#include <QByteArray>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    bool read(uint8_t* buffer, size_t length);

private slots:
    //警告是因为槽函数是自动生成的，如果有一天ui名字变了槽函数就不起效了，解决办法是可以手动connect，当然，也可以不管
    void on_btnOpen_clicked();

    void on_btnCode_clicked();

    void on_btnSave_clicked();

    void on_btnExit_clicked();

private:
    Ui::MainWindow *ui;
    /*
     * 先确定一下思路，要实现的是三个功能
     * 1、选择出来文件，所以需要一个承接文件的QFile，并且选择出来的文件名字要输出到LineEdit上去
     *    难点：要确保文件读入成功，之后才能继续进行下一步操作，并且要考虑到后续进行更改时是改的源文件，还是复制出来一份
     * 2、进行变化编码，加密文件异或一次就是源文件，源文件异或一次就是加密文件，总之都是一次异或，所以通过一个按钮功能实现
     *    需要一个QByteArray来保存key值，需要读取文件内容到buffer，需要在buffer上进行变换
     *    难点：对buffer的处理要确保生效，上一个就是这一步没有生效所以不行
     * 3、另存文件，需要将处理完的文件重新保存，保存的文件需要另写一份，如何去写也是个问题
     */
    QByteArray file_all;//另存运算后的文件
    QFile* m_file;      //读取的源文件
    QByteArray key;     //key值
    uint64_t fsize;     //文件大小
    uint16_t pos;       //运算用的一个position，其实也就是大小
};
#endif // MAINWINDOW_H
