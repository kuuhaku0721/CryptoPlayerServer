#ifndef QSTATUSCHECK_H
#define QSTATUSCHECK_H

#include <QObject>
#include <QThread>

class QStatusCheck : public QThread
{
    Q_OBJECT
public:
    explicit QStatusCheck(QObject* parent = nullptr);
protected:
    virtual void run();         //TODO:为什么要继承自线程来执行这个函数
signals:
    void postStatus();
private:
    bool m_status;
};

#endif // QSTATUSCHECK_H
