#include <Windows.h>
#include "qscreentop.h"
#include <QPainter>
#include <QBrush>
#include <QDebug>
/*RECT rect;
WNDPROC lpfnOldProc = 0;
LRESULT CALLBACK SCWndProc(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam);
*/
QScreenTop::QScreenTop(QWidget* parent) : QWidget(parent)
{
}



void QScreenTop::setText(const QString& text)                                       //为什么用这种弯弯绕绕的实现方式，而不是直接在ui里设置
{                                                                                   //答：因为它确实是直接用的ui->的形式去实现的，并没有用到这个类
    m_text = text;  //就通过它传入一下文本内容，真正设置不在本类中执行，而是由messageForm来执行的
}
/*
void QScreenTop::paintEvent(QPaintEvent* /*event* /)
{
    QPainter parent(parentWidget());
    QPainter painter(this);
    painter.begin(painter.device());
    QColor color(128, 128, 128, 128);
    QBrush brush(color);
    painter.setBrush(brush);
    painter.fillRect(frameGeometry(), brush);
}
*/
