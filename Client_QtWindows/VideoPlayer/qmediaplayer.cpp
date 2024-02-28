#include <Windows.h>
#include "qmediaplayer.h"
#include <QDebug>
#include <QPainter>

QMediaPlayer::QMediaPlayer(QWidget* parent, Qt::WindowFlags f)
    : QLabel(parent, f), timer(NULL), vlc(NULL) //给一些成员属性赋值，尤其注意指针类型的成员
{
    init_member();  //初始化参数，仅能在构造函数调用一次
}

QMediaPlayer::QMediaPlayer(const QString& text, QWidget* parent, Qt::WindowFlags f)
    : QLabel(text, parent, f), timer(NULL), vlc(NULL)
{
    init_member();  //重载的构造也是构造，反正构造只会调用一个，所以也要调用初始化函数
}

QMediaPlayer::~QMediaPlayer()
{
    if(timer)   //一般指针类型都要先判断是否非空再执行删除，否则有可能出现删除一个空指针的错误
    {
        qDebug() << __FILE__ << "(" << __LINE__ << "):";
        delete timer;
        qDebug() << __FILE__ << "(" << __LINE__ << "):";
        timer = NULL;
    }
    if(vlc)
    {
        qDebug() << __FILE__ << "(" << __LINE__ << "):";
        delete vlc;
        qDebug() << __FILE__ << "(" << __LINE__ << "):";
        vlc = NULL;
    }
    stat = MP_DESTORY;  //更改状态为销毁，状态是自定义枚举类型
}

bool QMediaPlayer::is_playing()
{
    if(vlc) //真正调用的是vlc的函数，最终的播放控制都由vlc来完成
    {
        return vlc->isplaying();
    }
    return false;
}

bool QMediaPlayer::is_paused()
{
    if(vlc) //同上
    {
        return vlc->ispause();
    }
    return false;
}

bool QMediaPlayer::has_media_player()
{
    if(vlc) //同上
    {
        return vlc->has_media_player();
    }
    return false;
}

bool QMediaPlayer::is_mute()
{
    return vlc->ismute();   //同上
}

int64_t QMediaPlayer::get_duration()
{
    return vlc->getduration(); //同上
}

void QMediaPlayer::set_float_text(const QString& text)
{
    vlc->init_text(text);   //利用vlc的初始化文本来设置文本内容
}

void QMediaPlayer::set_title_text(const QString& /*title*/)
{
}

/*
void QMediaPlayer::paintEvent(QPaintEvent* event)
{
}
*/

void QMediaPlayer::open(const QUrl& path)
{
    if(vlc) //先准备播放器，然后准备视频，所以要先判断vlc非空，然后才去打开文件
    {
        vlc->stop();    //停止当前的
        vlc->prepare(path.toString());  //准备打开新的
        stat = MP_OPEN; //同步更改状态
    }
    else
    {
        stat = MP_MEDIA_INIT_FAILED;    //运行到这里说明vlc初始化失败，后续均无法进行，同步修改状态
    }
}

void QMediaPlayer::play()
{
    if(vlc) //在vlc非空的情况下，更改状态
    {
        if(vlc->play() >= 0)
        {
            stat = MP_PLAY;
        }
        else
        {
            stat = MP_OPERATOR_FAILED;
        }
    }
    else
    {
        stat = MP_MEDIA_INIT_FAILED;
    }
}

void QMediaPlayer::pause()
{
    if(vlc) //在vlc非空的情况下，更改状态
    {
        if(vlc->pause() >= 0)
        {
            stat = MP_PAUSE;
        }
        else
        {
            stat = MP_OPERATOR_FAILED;
        }
    }
    else
    {
        stat = MP_MEDIA_INIT_FAILED;
    }
}

void QMediaPlayer::stop()
{
    if(vlc) //在vl非空情况下，更改状态
    {
        vlc->stop();    //调用vlc的stop函数关闭当前播放
        stat = MP_STOP;
    }
    else
    {
        stat = MP_MEDIA_INIT_FAILED;
    }
}

void QMediaPlayer::close()
{
    if(vlc)
    {
        vlc->stop();    //close也要先关闭播放
        stat = MP_NONE;
    }
    else
    {
        stat = MP_MEDIA_INIT_FAILED;
    }
}

void QMediaPlayer::seek(double pos)
{
    if(vlc) //重定位调用vlc的settime函数完成
    {
        vlc->settime(vlc->getduration()*pos);
    }
    else
    {
        stat = MP_MEDIA_INIT_FAILED;
    }
}

void QMediaPlayer::set_position(int64_t pos)
{
    if(vlc) //调用vlc的函数完成
    {
        vlc->settime(pos);
    }
    else
    {
        stat = MP_MEDIA_INIT_FAILED;
    }
}

void QMediaPlayer::set_position(int hour, int minute, int second, int millisecond)
{
    int64_t pos = (hour * 3600 + minute * 60 + second) * 1000 + millisecond;
    set_position(pos);
}

void QMediaPlayer::set_scale(float scale)
{
    if(vlc) //调用vlc的函数来设置播放速率
    {
        vlc->set_play_rate(scale);
    }
    else
    {
        stat = MP_MEDIA_INIT_FAILED;
    }
}

void QMediaPlayer::pick_frame(QImage& /*frame*/, int64_t /*pos*/)
{
    //TODO:
}

void QMediaPlayer::set_size(const QSize& sz)
{
    this->resize(sz); //更改大小
}

void QMediaPlayer::handleTimer()
{   //计时结束触发handleTimer函数
    unsigned char tick = 0;
    tick++;
    if(last != stat) //last:上一次的状态 stat：当前状态
    {
        //状态变化了，发送信号
        emit media_status(stat);
        last = stat;
    }
    if(stat == MP_PLAY) //状态为:正在播放
    {
        if(vlc)
        {
            double pos = vlc->gettime();
            pos /= vlc->getduration();  //这里这么计算播放位置是为了取已播放时长的百分比
            emit position(pos);
        }
        else
        {
            stat = MP_MEDIA_INIT_FAILED;
        }
    }
    if(tick % 10)//每5秒变化一次
    {
        vlc->update_text(); //每5秒更新一下文本显示，防盗用
    }
}

void QMediaPlayer::volume(int vol)
{
    vlc->volume(vol);   //设置音量
}

void QMediaPlayer::silence()
{
    vlc->silence();     //是否静音
}

void QMediaPlayer::init_member()
{
    //警告：仅能在构造函数使用一次！！！！
    timer = new QTimer(this);   //初始化一个计时器
    if(timer == NULL)
    {
        stat = MP_TIMER_INIT_FAILED;
        return;
    }
    try
    {
        vlc = new vlchelper(this); //初始化vlc
    }
    catch (...) //...代表捕获所有异常
    {
        if(vlc == NULL)
        {
            stat = MP_MEDIA_LOAD_FAILED;
            return;
        }
    }
    QLabel::connect(timer, &QTimer::timeout,
                    this, &QMediaPlayer::handleTimer);
    timer->start(500); //500ms计时一次
    stat = MP_NONE; //确定初始化状态 stat由改变状态的函数控制
    last = MP_NONE; //last由计时器控制
}

