#include "widget.h"
#include "ui_widget.h"
#include <QDebug>
#include <QEvent>
#include <QMimeData>
#include <QMessageBox>
#include <QDesktopWidget>
#include <QPainter>
#include <QToolTip>
#include "qstatuscheck.h"
#include <QCryptographicHash>
#include <QDesktopWidget>
extern const char* MD5_KEY;         //MD5密钥
extern const char* HOST;            //主机地址
extern bool LOGIN_STATUS;           //登录状态
//这些线程是用来预防有人逆向绕开登录逻辑 如果没有真正的登录，则检测线程会退出程序
QStatusCheck check_thread[32];
#ifdef WIN32
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "libvlccore.lib")
#pragma comment(lib, "libvlc.lib")
#endif

//TODO:追加信号，使用信号来触发整个流程
    //一些按钮的样式表
//播放和暂停时，按钮的样式表
const char* PAUSE_STYLESHEET =
        "QPushButton#playButton{"
        "border-image: url(:/ui/button/white/UI/white/zanting.png);"
        "background-color: transparent;}"
        "QPushButton#playButton:hover{"
        "border-image: url(:/ui/button/blue/UI/blue/zanting.png);}"
        "QPushButton#playButton:pressed{"
        "border-image: url(:/ui/button/gray/UI/gray/zanting.png);}";
const char* PLAY_STYLESHEET =
        "QPushButton#playButton{"
        "border-image: url(:/ui/button/blue/UI/blue/bofang.png);"
        "background-color: transparent;}"
        "QPushButton#playButton:hover{"
        "border-image: url(:/ui/button/gray/UI/gray/bofang.png);}"
        "QPushButton#playButton:pressed{"
        "border-image: url(:/ui/button/white/UI/white/bofang.png);}";

//静音状态按钮
const char* SILENCE_STYLESHEET =
    "QPushButton#volumeButton{"
    "border-image: url(:/ui/button/blue/UI/blue/jingyin.png);"
    "background-color: transparent;}"
    "QPushButton#volumeButton:hover{"
    "border-image: url(:/ui/button/gray/UI/gray/jingyin.png);}"
    "QPushButton#volumeButton:pressed{"
    "border-image: url(:/ui/button/white/UI/white/jingyin.png);}";
//非静音状态按钮
const char* VOICE_STYLESHEET =
    "QPushButton#volumeButton{"
    "border-image: url(:/ui/button/blue/UI/blue/yinliang.png);"
    "background-color: transparent;}"
    "QPushButton#volumeButton:hover{"
    "border-image: url(:/ui/button/gray/UI/gray/yinliang.png);}"
    "QPushButton#volumeButton:pressed{"
    "border-image: url(:/ui/button/white/UI/white/yinliang.png);}";

//列表显示的时候，按钮的样式
const char* LIST_SHOW =
        "QPushButton{border-image: url(:/UI/images/arrow-right.png);}"
        "QPushButton:hover{border-image: url(:/UI/images/arrow-right.png);}"
        "QPushButton:pressed{border-image: url(:/UI/images/arrow-right.png);}";
//列表隐藏的时候，按钮的样式
const char* LIST_HIDE =
        "QPushButton{border-image: url(:/UI/images/arrow-left.png);}"
        "QPushButton:hover{border-image: url(:/UI/images/arrow-left.png);}"
        "QPushButton:pressed{border-image: url(:/UI/images/arrow-left.png);}";

//当前播放文件的样式
const char* CURRENT_PLAY_NORMAL =
        "background-color: rgba(255, 255, 255, 0);\nfont: 10pt \"黑体\";"
        "\ncolor: rgb(255, 255, 255);";

const char* CURRENT_PLAY_FULL =
        "background-image: url(:/UI/images/screentop.png);"
        "background-color: transparent;"
        "\nfont: 20pt \"黑体\";\ncolor: rgb(255, 255, 255);";

//最大化和恢复按钮
const char* SCREEN_RESTORE_STYLE =
        "QPushButton{border-image: url(:/UI/images/huifu.png);}\n"
        "QPushButton:hover{border-image:url(:/UI/images/huifu-hover.png);}\n"
        "QPushButton:pressed{border-image: url(:/UI/images/huifu.png);}";

const char* SCREEN_MAX_STYLE =
        "QPushButton{border-image: url(:/UI/images/fangda.png);}\n"
        "QPushButton:hover{border-image:url(:/UI/images/fangda-hover.png);}\n"
        "QPushButton:pressed{border-image: url(:/UI/images/fangda.png);}";

#define FLOAT_TYPE (Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint)
//      窗口浮动样式      无边框                  有工具栏        保持最前端
Widget::Widget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
    , helper(this)
{
    is_silence = false; //是否静音:默认为false
    slider_pressed = false;     //是否拖动进度条:默认false
    rate = 1.0f;                //播放速度:默认1倍
    ui->setupUi(this);
    //没有系统边框
    this->setWindowFlag(Qt::FramelessWindowHint);
    //接收文件拖拽
    this->setAcceptDrops(true);
    //当前没有播放任何视频
    current_play = -1;
    //按钮背景透明
    ui->preButton->setAttribute(Qt::WA_TranslucentBackground, true);
    //列表单选
    ui->listWidget->setSelectionMode(QAbstractItemView::SingleSelection); //单选模式
    ui->horizontalSlider->setRange(0, 1000); //进度条的范围0-1000，越大精度越细
    ui->horizontalSlider->setValue(0); //初始值为0
    //connect(ui->horizontalSlider, SIGNAL(sliderPressed()),
    //        this, SLOT(on_horizontalSlider_sliderPressed()));
    //connect(ui->horizontalSlider, SIGNAL(sliderReleased()),
    //        this, SLOT(on_horizontalSlider_sliderReleased()));
    //timerID = startTimer(500);//用于刷新进度的
    //设置事件过滤器，我们关注音量和窗口的事件
    ui->volumeButton->installEventFilter(this); //音量按钮
    ui->volumeSlider->installEventFilter(this); //音量条
    ui->listWidget->installEventFilter(this);   //右侧列表
    ui->scaleButton->installEventFilter(this);  //倍速按钮
    ui->horizontalSlider->installEventFilter(this); //进度条
    installEventFilter(this); //ui窗口
    //音量控件初始化
    ui->volumeSlider->setVisible(false); //音量条可视化
    ui->volumeSlider->setRange(0, 100);  //设置音量范围
    //透明顶字幕层
    ui->screentop->setHidden(true); //顶部那个透明层
    //播放倍数 初始速度为1.0倍，其余的都为false
    ui->time0_5->setVisible(false);
    ui->time1->setVisible(false);
    ui->time1_5->setVisible(false);
    ui->time2->setVisible(false);

    volumeSliderTimerID = -1;
    timesID = -1;
    timesCount = 0;
    fullScreenTimerID = -1;
    volumeCount = 0;  //音量滑动条计时器
    setTime(0, 0, 0); //记录播放时间的，就是进度条下面那个时间计数
    setTime2(0, 0, 0);  //同上，形式不一样

    QDesktopWidget* desktop = QApplication::desktop(); //获取当前的整个桌面，主要获取大小用的，比如全屏时控制大小
    int currentScreenWidth = QApplication::desktop()->width();
    int currentScreenHeight = QApplication::desktop()->height();
    if(desktop->screenCount() > 1)
    {
        //使用主显示器的尺寸，如果显示器有多个的情况下
        currentScreenWidth = desktop->screenGeometry(0).width();
        currentScreenHeight = desktop->screenGeometry(0).height();
    }
    //如果分辨率小于800*600则按800*600算
    if(currentScreenWidth > 800 && currentScreenHeight > 600)
    {
        setMaximumSize(currentScreenWidth, currentScreenHeight);
    }
    else
    {
        setMaximumSize(800, 600);
    }

    init_media(); //初始化媒体源，主要是进行一些信号和槽函数的绑定
    info.setWindowFlag(Qt::FramelessWindowHint); //消息提示窗口的格式，无边框
    info.setWindowModality(Qt::ApplicationModal); //以应用的形式出现，模态窗口
    connect(&info, SIGNAL(button_clicked()),
            this, SLOT(slot_connect_clicked()));
    QSize sz = size();
    info.move((sz.width() - info.width()) / 2, (sz.height() - info.height()) / 2); //移动info窗口的合适位置
    //u8是为了跨平台的时候中文不乱码
    ui->curplay->setText(u8""); //当前正在播放的媒体的label，在播放列表上面那个，初始化的时候为空，有媒体播放的时候显示的是媒体的名字
    //ui->curplay->setAttribute(Qt::WA_TranslucentBackground);
    //ui->curplay->setWindowOpacity(0.5);
    //窗口尺寸控制器
    helper.update(width(), height()); //helper是SizeHelper类，在Widget类内嵌的类,方便控制屏幕大小设置的
    //设置最小和最大窗口尺寸（最小是800*600 最大是屏幕尺寸）
    setMinimumSize(800, 600);
    save_default_rect_info();
    full_head = new MessageForm();
    full_head->setWindowFlags(FLOAT_TYPE); //消息框：无边框...
    full_head->full_size();
    full_head->move(0, 0); //移动到左上角，这个框框就是播放屏幕上面的那一条半透明的框框
    //full_head->show();
    //full_head->setHidden(false);
    screentopTimerID = startTimer(50);
    keep_activity_timerID = startTimer(300000);
    net = new QNetworkAccessManager(this);
    connect(net, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(slots_login_request_finshed(QNetworkReply*))); //登录状态槽函数，确定登录成功or失败
    connect(this, SIGNAL(update_pos(double)), ui->media, SLOT(seek(double)));
}

Widget::~Widget()
{   //前面new出来的东西记得都delete掉
    killTimer(keep_activity_timerID);
    delete net; //网络连接，登录用的
    delete ui;  //窗口的ui，就是自己
    delete full_head; //屏幕上面的半透明框
}

void Widget::timerEvent(QTimerEvent* event)
{
    //计时器事件：状态检查、播放速率自动隐藏、音量条自动隐藏、全屏时音量条隐藏、全屏时顶部的半透明框更新
    if(event->timerId() == keep_activity_timerID)
    {
        check_thread[GetTickCount()%32].start();    //每隔一段时间启动一个线程去检查登录状态
        //TODO:服务器保持活跃
    }
    else if(event->timerId() == timesID) //1s
    {   //播放倍率的那个框框
        timesCount++;   //1s计数加1，5秒后自动隐藏
        if(timesCount > 5)
        {
            timesCount = 0;
            ui->time0_5->setVisible(false);
            ui->time1->setVisible(false);
            ui->time1_5->setVisible(false);
            ui->time2->setVisible(false);
            killTimer(timesID);
            timesID = -1;
            //qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):";
        }
        //qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):" << timesCount;
    }
    else if(event->timerId() == volumeSliderTimerID)
    {   //音量的那个长条
        volumeCount++;  //1s计数加1，5秒后自动隐藏
        if(volumeCount > 5)//计时到了，自动销毁定时器
        {
            volumeCount = 0;
            ui->volumeSlider->setVisible(false);
            killTimer(volumeSliderTimerID);
            volumeSliderTimerID = -1;
        }
    }
    else if(event->timerId() == fullScreenTimerID)
    {   //全屏时隐藏音量条(不管计时器的计数)
        int currentScreenWidth = QApplication::desktop()->width();
        int currentScreenHeight = QApplication::desktop()->height();
        POINT point;
        GetCursorPos(&point); //获取当前鼠标所在位置坐标
        QPoint pt(point.x, point.y);
        QRect bottom(0, currentScreenHeight - 68, currentScreenWidth, 68); //音量条所在框框范围，鼠标还在这个范围内的话就先不隐藏音量条
        //qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):" << currentScreenWidth << "," << currentScreenHeight;
        //qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):" << bottom;
        if(full_head->geometry().contains(pt) || bottom.contains(pt))
        {
            //如果鼠标还在该区域活动，则无需隐藏，顺延一个周期
        }
        else
        {
            helper.auto_hide(); //隐藏掉音量条
            full_head->hide();  //隐藏顶部框框
            killTimer(fullScreenTimerID); //干掉计时器，因为全屏的隐藏时不管计时器状态的
            fullScreenTimerID = -1;
        }
    }
    else if(event->timerId() == screentopTimerID) //1s 20次
    {
        full_head->update();
    }
}

void Widget::save_default_rect_info()
{   //记录ui位置信息,在全屏或者最大化时更新ui位置信息
    QObjectList list = children();  //children函数返回的是当前控件对象的子对象(列表)类比想象一下MainWindow窗口(父对象)和StatusBar，toolBar(子对象)的关系
    QObjectList::iterator it = list.begin();
    QString names[] =//子ui名称
    {//34
            "backgroundLb", "media", "downbkgndLb", "listWidget", //4
            "horizontalSlider", "preButton", "nextButton", "volumeButton",//4
            "timeLb", "volumeSlider", "label", "logoLabel",//4
            "userInfo", "loginstatus", "settingBtn", "miniButton",//4
            "fangdaButton", "closeButton", "showhideList", "playInfo",//4
            "playInfoIco", "curplay", "stopButton", "time2Lb",//4
            "fullscreenBtn", "settingButton", "scaleButton", "autoButton",//4
            "screentop", "time1_5", "time2", "time1", "time0_5", "playButton"//6
    }; //窗口上有的子对象
    qDebug().nospace()<<"[" << __FILE__ << "]" <<"("<<__LINE__<<"):"<<__FUNCTION__<<" X,Y = "
                     <<GetSystemMetrics(SM_CXSCREEN)<<GetSystemMetrics(SM_CYSCREEN);

    int currentScreenWidth = QApplication::desktop()->screenGeometry(0).width();//QApplication::desktop()->width();
    int currentScreenHeight = QApplication::desktop()->screenGeometry(0).height() - 50;
    qDebug().nospace()<<"[" << __FILE__ << "]" <<"("<<__LINE__<<"):"<<__FUNCTION__<<
                        " width="<<currentScreenWidth<<" height"<<currentScreenHeight;
    qDebug().nospace()<<"[" << __FILE__ << "]" <<"("<<__LINE__<<"):"<<__FUNCTION__<<" screen cout="<<QApplication::desktop()->screenCount();
    QRect max_rect[] =//最大化时的尺寸
    {     //QRect(x, y, width, height)
            QRect(0, 41, currentScreenWidth, currentScreenHeight - 110),                //backgroundLb 视频播放背景黑底板
            QRect(0, 41, currentScreenWidth - 300, currentScreenHeight - 110),          //media 视频播放控件
            QRect(0, 0, 0, 0),                                                          //downbkgndLb 全屏的时候，下方的背景板
            QRect(currentScreenWidth - 300, 41, 300, currentScreenHeight - 41),         //listWidget 播放列表
            QRect(0, currentScreenHeight - 68, currentScreenWidth - 300, 22),           //horizontalSlider 播放进度条
            QRect(110, currentScreenHeight - 45, 32, 32),                               //preButton 上一条按钮
            QRect(160, currentScreenHeight - 45, 32, 32),                               //nextButton 下一条按钮
            QRect(currentScreenWidth - 400, currentScreenHeight - 45, 32, 32),          //volumeButton
            QRect(215, currentScreenHeight - 45, 90, 25),                               //timeLb
            QRect(currentScreenWidth - 398, currentScreenHeight - 205, 22, 160),        //volumeSlider 音量滑动条 10
            QRect(0, 0, 1, 1),                                                          //label
            QRect(5, 5, 140, 30),                                                       //logoLabel
            QRect(currentScreenWidth - 270, 7, 28, 28),                                 //userInfo
            QRect(currentScreenWidth - 270, 7, 45, 25),                                 //loginstatus
            QRect(currentScreenWidth - 160, 5, 30, 30),                                 //settingBtn
            QRect(currentScreenWidth - 120, 5, 30, 30),                                 //miniButton
            QRect(currentScreenWidth - 80, 5, 30, 30),                                  //fangdaButton
            QRect(currentScreenWidth - 40, 5, 30, 30),                                  //closeButton
            QRect(currentScreenWidth - 360, (currentScreenHeight - 170) / 2, 60, 60),   //showhideList
            QRect(currentScreenWidth - 298, 43, 296, 46),                               //playInfo
            QRect(currentScreenWidth - 300, 41, 32, 50),                                //playInfoIco
            QRect(currentScreenWidth - 265, 50, 260, 32),                               //curplay
            QRect(60, currentScreenHeight - 45, 32, 32),                                //stopButton
            QRect(300, currentScreenHeight - 45, 120, 25),                              //time2Lb
            QRect(currentScreenWidth - 350, currentScreenHeight - 45, 32, 32),          //fullscreenBtn
            QRect(currentScreenWidth - 450, currentScreenHeight - 45, 32, 32),          //settingButton
            QRect(currentScreenWidth - 500, currentScreenHeight - 45, 42, 32),          //scaleButton
            QRect(currentScreenWidth - 560, currentScreenHeight - 45, 42, 32),          //autoButton
            QRect(0, 0, 1, 1),                                                          //顶部内容提示
            QRect(currentScreenWidth - 200, currentScreenHeight - 157, 42, 48),         //倍速1.5
            QRect(currentScreenWidth - 200, currentScreenHeight - 185, 42, 48),         //倍速2
            QRect(currentScreenWidth - 200, currentScreenHeight - 129, 42, 48),         //倍速1
            QRect(currentScreenWidth - 200, currentScreenHeight - 101, 42, 48),         //倍速0.5
            QRect(5, currentScreenHeight - 45, 32, 32)                                  //playButton 播放按钮
    };
    currentScreenHeight += 50;
    QRect full_rect[] =//全屏时的尺寸
    {
            QRect(0, 0, currentScreenWidth, currentScreenHeight),                   //backgroundLb
            QRect(0, 0, currentScreenWidth, currentScreenHeight),                   //media
            QRect(0, currentScreenHeight - 60, currentScreenWidth, 60),             //downbkgndLb 全屏的时候，下方的背景板
            QRect(),                                                                //列表框（全屏的时候不显示列表）listWidget
            QRect(0, currentScreenHeight - 68, currentScreenWidth, 22),             //播放进度条 5 horizontalSlider
            QRect(110, currentScreenHeight - 45, 32, 32),                           //preButton
            QRect(160, currentScreenHeight - 45, 32, 32),                           //nextButton
            QRect(currentScreenWidth - 100, currentScreenHeight - 45, 32, 32),      //volumeButton 4
            QRect(215, currentScreenHeight - 45, 90, 25),                           //timeLb
            QRect(currentScreenWidth - 98, currentScreenHeight - 205, 22, 160),     //volumeSlider 音量滑动条
            QRect(),                                                                //label
            QRect(),                                                                //logoLabel 4
            QRect(),                                                                //userInfo
            QRect(),                                                                //loginstatus
            QRect(),                                                                //settingBtn
            QRect(),                                                                //miniButton 4
            QRect(),                                                                //fangdaButton
            QRect(),                                                                //closeButton
            QRect(),                                                                //列表显示按钮
            QRect(),                                                                //playInfo
            QRect(),                                                                //playInfoIco //4
            QRect(30, 30, 0, 0),                                                    //当前播放的内容 curplay
            QRect(60, currentScreenHeight - 45, 32, 32),                            //停止按钮
            QRect(300, currentScreenHeight - 45, 120, 25),
            QRect(currentScreenWidth - 50, currentScreenHeight - 45, 32, 32),       //fullscreenBtn
            QRect(currentScreenWidth - 150, currentScreenHeight - 45, 32, 32),
            QRect(currentScreenWidth - 200, currentScreenHeight - 45, 32, 32),
            QRect(currentScreenWidth - 260, currentScreenHeight - 45, 32, 32),
            QRect(0, 0, 1, 1),
            QRect(currentScreenWidth - 200, currentScreenHeight - 157, 42, 48),     //1.5
            QRect(currentScreenWidth - 200, currentScreenHeight - 185, 42, 48),     //2
            QRect(currentScreenWidth - 200, currentScreenHeight - 129, 42, 48),     //1
            QRect(currentScreenWidth - 200, currentScreenHeight - 101, 42, 48),     //0
            QRect(5, currentScreenHeight - 45, 32, 32),                             //playButton
    };
    qDebug().nospace()<<"[" << __FILE__ << "]" <<"("<<__LINE__<<"):"<<__FUNCTION__<< "max_rect size:" << sizeof(max_rect)/sizeof(QRect);
    qDebug().nospace()<<"[" << __FILE__ << "]" <<"("<<__LINE__<<"):"<<__FUNCTION__<< "full_rect size:" << sizeof(full_rect)/sizeof(QRect);
    bool max_hide[] =//最大化时的隐藏状态和初始状态,注意一一对应
    {
            false, false, true, false,
            false, false, false, false,
            false, true, false, false,
            true, true, false, false,
            false, false, false, false,
            false, false, false, false,
            false, false, false, false,
            true, true, true, true, true,false,
    };
    bool full_hide[] =//全屏时的隐藏状态和初始状态,注意一一对应
    {
            false, false, false, true,
            false, false, false, false,
            false, true, true, true,
            true, true, true, true,
            true, true, true, true,
            true, true, false, false,
            false, false, false, false,
            true, true, true, true, true, false
    };
    int auto_hide_status[] =  //全屏时是否自动隐藏0 不隐藏 1 隐藏 2 不参与
    {
            0, 0, 1, 2,
            1, 1, 1, 1,
            1, 1, 2, 2,
            2, 2, 2, 2,
            2, 2, 2, 2,
            2, 2, 1, 1,
            1, 1, 1, 1,
            1, 2, 2, 2, 2, 1
    };
    for(int i = 0; it != list.end(); it++, i++) //更新默认尺寸
    {
        QWidget* widget = (QWidget*)(*it);  //it是当前窗口的所有子控件列表指针，负责遍历所有子控件，也就是当前窗口的所有ui
        QString name = widget->objectName();
        helper.init_size_info(widget);              //初始化控件大小
        helper.set_full_rect(name, full_rect[i]);   //设置全屏时尺寸
        helper.set_max_rect(name, max_rect[i]);     //设置最大化时尺寸
        helper.set_full_hide(name, full_hide[i]);   //设置全屏时隐藏状态
        helper.set_max_hide(name, max_hide[i]);     //设置最大化时隐藏状态
        helper.set_auto_hide(name, auto_hide_status[i]); //全屏时是否自动隐藏(比如音量条)
        qDebug().nospace()<<"[" << __FILE__ << "]" <<"("<<__LINE__<<"):"<<__FUNCTION__<<
            "name:" << (*it)->objectName() << full_rect[i] << max_rect[i];
    }
}

void Widget::setSlider(QSlider* slider, int nMin, int nMax, int nStep)
{   //设置滚动条的最大最小值，但是似乎没用上
    slider->setMinimum(nMin);
    slider->setMaximum(nMax);
    slider->setSingleStep(nStep);
}

void Widget::setTime(int hour, int minute, int seconds)
{   //设置播放时间，就是在进度条下面那个已播放时间
    QString s;
    QTextStream out(&s);
    out.setFieldWidth(2);
    out.setPadChar('0');
    out << hour ;
    out.setFieldWidth(1);
    out << ":";
    out.setFieldWidth(2);
    out << minute;
    out.setFieldWidth(1);
    out << ":";
    out.setFieldWidth(2);
    out << seconds;
    //qDebug() << "time:" << s;
    ui->timeLb->setText(s);
}

void Widget::setTime(int64_t tm)
{
    tm /= 1000;//得到秒数
    int seconds = tm % 60;
    int minute = (tm / 60) % 60;
    int hour = tm / 3600;
    setTime(hour, minute, seconds);
}

void Widget::setTime2(int hour, int minute, int seconds)
{ //设置播放时间，这个是总时间，表现形式是：已播放时间/总时间 /两边都是时分秒的形式
    QString s;
    QTextStream out(&s);
    out << "/";
    out.setFieldWidth(2);
    out.setPadChar('0');
    out << hour ;
    out.setFieldWidth(1);
    out << ":";
    out.setFieldWidth(2);
    out << minute;
    out.setFieldWidth(1);
    out << ":";
    out.setFieldWidth(2);
    out << seconds;
    //qDebug() << "time:" << s;
    ui->time2Lb->setText(s);
}

void Widget::setTime2(int64_t tm)
{
    tm /= 1000;//得到秒数
    int seconds = tm % 60;
    int minute = (tm / 60) % 60;
    int hour = tm / 3600;
    setTime2(hour, minute, seconds);
}

void Widget::paintLine()
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true); // 反锯齿
    painter.save();    //保存画笔状态
    QLinearGradient linearGradient(0, 40, width(), 40);  //设置颜色渐变，参数为渐变的起止范围
    linearGradient.setColorAt(0, QColor(61, 163, 241));  //渐变色的初始颜色  浅蓝色
    linearGradient.setColorAt(1, QColor(36, 95, 207));   //渐变色的终止颜色  稍微深一点的蓝色
    QPen pen = painter.pen();
    pen.setBrush(linearGradient);  //设置笔刷直接为渐变色笔刷
    pen.setWidth(1);
    painter.setPen(pen);           //painter拿到笔刷
    painter.drawLine(0, 40, width(), 40); //painter执行绘画行为
    painter.restore(); //恢复到之前保存的状态
}

void Widget::init_media()
{
    connect(this, SIGNAL(open(const QUrl&)),
            ui->media, SLOT(open(const QUrl&)));            //打开媒体源
    connect(this, SIGNAL(play()),
            ui->media, SLOT(play()));                       //开始播放媒体源
    connect(this, SIGNAL(pause()),
            ui->media, SLOT(pause()));                      //暂停
    connect(this, SIGNAL(stop()),
            ui->media, SLOT(stop()));
    connect(this, SIGNAL(close_media()),                    //关闭当前源
            ui->media, SLOT(close()));
    connect(this, SIGNAL(seek(double)),
            ui->media, SLOT(seek(double)));                 //重定位，拖动进度条改变
    connect(this, SIGNAL(set_scale(float)),
            ui->media, SLOT(set_scale(float)));             //设置播放倍率
    connect(this, SIGNAL(pick_frame(QImage&, int64_t)),
            ui->media, SLOT(pick_frame(QImage&, int64_t))); //截屏当封面用,实际并未使用,甚至没有实现
    connect(this, SIGNAL(set_size(const QSize&)),
            ui->media, SLOT(set_size(const QSize&)));       //设置大小
    connect(this, SIGNAL(volume(int)),
            ui->media, SLOT(volume(int)));                  //设置音量
    connect(this, SIGNAL(silence()),
            ui->media, SLOT(silence()));                    //静音
    //TODO:↓↓↓这个逻辑最好在登录之后处理↓↓↓
    ui->media->set_float_text("用户ID：root");
}

void Widget::on_show(const QString& nick, const QByteArray& /*head*/)
{
    show();             //登录成功，隐藏掉登录窗口之后要显示当前窗口
    info.show();        //登录成功之后跳脸的一个消息通知窗口
    ui->loginstatus->setText(u8"已登录");
    ui->media->set_float_text(nick);
    //this->nick = nick;
    this->nick = "测试账号";
    if(LOGIN_STATUS && this->isHidden() == false)
    {
        check_thread[0].start();  //检查登录状态的线程启动
    }
}

void Widget::slot_connect_clicked()
{   //登陆成功之后跳脸的那个消息窗口的按钮跳转功能
    QString strAddress;
    strAddress = QString("https://www.bh3.com/");  //跳转崩崩崩官网，因为按钮是"为世界上一切的美好而战"

    LPCWSTR wcharAddress = reinterpret_cast<const wchar_t*>(strAddress.utf16());  //reinterpret_cast强制转型，如int*转char*，这里是QString转utf16之后转wchar_t
    ShellExecute(0, L"open", wcharAddress, L"", L"", SW_SHOW );  //调用Shell去执行,来自Win32的API
    check_thread[1].start();  //再启动一个登陆状态检查线程
}

void Widget::on_preButton_clicked()
{
    if(mediaList.size() <= 0)
    {
        return;    //如果没有视频，则啥也不干
    }
    current_play--;
    if(current_play < 0)
    {
        current_play = mediaList.size() - 1; //如果上一个到头部了，则从列表尾部开始
    }
    emit open(mediaList.at(current_play));    //发射“打开媒体”的信号
    QString filename = mediaList.at(current_play).fileName();
    if(filename.size() > 12)
    {
        filename.replace(12, filename.size() - 12, u8"…");  //列表宽最高只能显示12个字，多余的显示为...
    }
    ui->curplay->setText(filename);
    check_thread[2].start();  //每一个功能都要启动一个登录状态检查线程
}

/*
 * 播放按钮逻辑：
 * 1 当前没有播放，按钮显示播放，点击成功后显示暂停
 * 2 当前正在播放，按钮显示暂停，点击成功后显示播放
 * 3 当前正在播放，切换了歌曲，点击成功后显示暂停
 *
*/
void Widget::on_playButton_clicked()
{
    int count = ui->listWidget->count();  //待播放列表的播放源数量
    if(count <= 0) //没有资源，则什么也不做
    {
        return;
    }
    QList<QListWidgetItem*> selectedItems = ui->listWidget->selectedItems();
    bool isplaying = ui->media->is_playing();
    int index = 0;
    if(selectedItems.size() > 0) //从选中选中视频开始播放
    {
        index = ui->listWidget->currentRow();
    }
    if(isplaying == false && (ui->media->is_paused() == false)) //没有播放，没有暂停，列表有资源，此时按照index顺序进行播放
    {
        emit open(mediaList.at(index));  //发射打开的信号
        emit play();  //发射开始播放的信号
        current_play = index;  //设置当前播放序号
        ui->playButton->setStyleSheet(PAUSE_STYLESHEET);  //开始播放之后，原本的"播放按钮"要变成"暂停按钮"
        tick = 0;
        QString filename = mediaList.at(current_play).fileName();
        if(filename.size() > 12)
        {
            filename.replace(12, filename.size() - 12, u8"…");
        }
        ui->curplay->setText(filename);
        return;//返回
    }
    if(ui->media->is_paused())
    {   //这个是开始播放按钮的槽函数，所以如果现在是暂停状态就要恢复播放
        //暂停恢复
        emit play();
        return;
    }
    //现在的情况是: 有资源，正在播放，需要检测逻辑2和3
    if(index == this->current_play)
    {
        emit pause(); //资源没有切换，那么执行暂停
        ui->playButton->setStyleSheet(PLAY_STYLESHEET);  //正在播放时按钮是"暂停按钮"暂停之后要变成"播放按钮"
        return;
    }
    //现在的情况是: 有资源，切换了选中资源，需要播放新的内容
    emit stop();//先停止播放
    emit open(mediaList.at(index));   //发射信号，打开新的媒体源
    emit play();
    current_play = index;
    QString filename = mediaList.at(current_play).fileName();
    if(filename.size() > 12)
    {
        filename.replace(12, filename.size() - 12, u8"…");
    }
    ui->curplay->setText(filename);
    check_thread[3].start();  //每个功能都要启动一个线程去检测登录状态，安全
}

void Widget::on_nextButton_clicked()
{
    if(mediaList.size() <= 0)
    {
        return;    //如果没有视频，则啥也不干
    }
    current_play++;
    if(current_play >= mediaList.size())
    {
        current_play = 0;//如果下一个到尾部了，则回到列表开头重新开始
    }
    emit open(mediaList.at(current_play));  //"下一个按钮",动作是打开下一个媒体源
    emit play();
    QString filename = mediaList.at(current_play).fileName();
    if(filename.size() > 12)
    {
        filename.replace(12, filename.size() - 12, u8"…");
    }
    ui->curplay->setText(filename);
    check_thread[4].start();  //一样，每一个动作都启动安全检测线程
}

void Widget::on_volumeButton_clicked()
{
    if(!is_silence)
    { //当前状态，没有静音
        ui->volumeButton->setStyleSheet(SILENCE_STYLESHEET); //设置为静音的样式
        is_silence = true; //是否静音: true
    }
    else
    { //状态：已经静音，需要恢复
        ui->volumeButton->setStyleSheet(VOICE_STYLESHEET); //设置为音量的样式
        is_silence = false;
    }
    emit silence();  //单击音量按钮，实现静音功能,功能由widget发出信号，由QMediaPlayer.cpp接收，由vlc执行
}

void Widget::on_scaleButton_clicked()
{   //倍率按钮
    if(timesID == -1)  //计时器是负责按钮自动隐藏的, =-1说明刚打开，需要开始计时，计时结束隐藏
    {
        timesID = startTimer(200);
        ui->time0_5->setVisible(true);
        ui->time1->setVisible(true);
        ui->time1_5->setVisible(true);
        ui->time2->setVisible(true);
        timesCount = -20;
        return;
    }
    else
    {   //计时结束，清除上一轮的计时器，隐藏所有按钮
        killTimer(timesID);
        timesID = -1;
        //播放倍数
        ui->time0_5->setVisible(false);
        ui->time1->setVisible(false);
        ui->time1_5->setVisible(false);
        ui->time2->setVisible(false);
    }
}

//进度条处理
void Widget::on_horizontalSlider_sliderPressed()
{
    slider_pressed = true;    //点击进度条
    //qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):";
    //qDebug() << "is_playing:" << ui->media->is_playing();
}

void Widget::on_horizontalSlider_sliderReleased()
{
    //qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):";
    //qDebug() << "is_playing:" << ui->media->is_playing();
    //else
    {
        //qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):";
    }
    slider_pressed = false;     //鼠标离开进度条
}

void Widget::on_horizontalSlider_rangeChanged(int /*min*/, int /*max*/)
{//进度条的范围改变，也就是视频长度变了，这能行？
    //qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):";
    //qDebug() << "is_playing:" << ui->media->is_playing();
}

void Widget::on_horizontalSlider_valueChanged(int value)
{
    if(slider_pressed)  //鼠标按下进度条的状态，说明在拖动进度条，需要进行进度条值的改变
    {
        //qDebug().nospace() << __FILE__ << "(" << __LINE__ << "): value=" << value;
        // qDebug() << "is_playing:" << ui->media->is_playing();
        if(ui->media->has_media_player()) //保证媒体源media存在，播放器player存在
        {
            int max = ui->horizontalSlider->maximum();
            int min = ui->horizontalSlider->minimum();
            double cur = (value - min) * 1.0 / (max - min);  //计算的是百分比
            //qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):cur=" << cur << " value=" << value;
            //qDebug() << "min=" << min << " max=" << max;
            emit update_pos(cur);
            setTime(ui->media->get_duration()*cur);
        }
    }
    else
    {
        //qDebug().nospace() << __FILE__ << "(" << __LINE__ << "): value=" << value;
    }
}

void Widget::on_media_position(double pos)
{
    if(slider_pressed == false)  //鼠标松开进度条的状态，说明已经拖动过了，需要同步改变一下播放时间显示
    {
        int max = ui->horizontalSlider->maximum();
        int min = ui->horizontalSlider->minimum();
        int value = min + pos * (max - min);
        ui->horizontalSlider->setValue(value);
        setTime(ui->media->get_duration()*pos);
        setTime2(ui->media->get_duration());
    }
}

void Widget::on_media_media_status(QMediaPlayer::PlayerStatus s)
{   //根据媒体源的状态更改播放按钮的图标显示：开始的三角形和暂停的两道杠
    switch(s)
    {
    case QMediaPlayer::MP_OPEN:
        ui->playButton->setStyleSheet(PLAY_STYLESHEET);
        break;
    case QMediaPlayer::MP_PLAY:
        ui->playButton->setStyleSheet(PAUSE_STYLESHEET);
        break;
    case QMediaPlayer::MP_PAUSE:
        ui->playButton->setStyleSheet(PLAY_STYLESHEET);
        break;
    case QMediaPlayer::MP_CLOSE:
        ui->playButton->setStyleSheet(PLAY_STYLESHEET);
        break;
    default:
        ui->playButton->setStyleSheet(PLAY_STYLESHEET);
        break;
    }
}

void Widget::slots_login_request_finshed(QNetworkReply* reply)
{
    this->setEnabled(true);      //点击登录的那个
    bool login_success = false;  //变量表示登录成功，默认为false，成功之后为true
    if(reply->error() != QNetworkReply::NoError) //不等于NoError，说明有错误
    {
        info.set_text(u8"用户不能为空\r\n" + reply->errorString(), u8"确认").show();  //消息提示框，上面的label显示的是提示信息，按钮显示"确认"
        return;
    }
    QByteArray data = reply->readAll(); //读取从服务端获得的网络请求的返回信息（JSON）
    //qDebug() << data;
    QJsonParseError json_error;
    QJsonDocument doucment = QJsonDocument::fromJson(data, &json_error);
    if (json_error.error == QJsonParseError::NoError)  //JSON无错误
    {
        if (doucment.isObject())  //读取到的JSON数据是一个对象
        {
            const QJsonObject obj = doucment.object();
            if (obj.contains("status") && obj.contains("message"))
            {
                QJsonValue status = obj.value("status");   //从JSON中解析出来status和message两部分信息
                QJsonValue message = obj.value("message");
            }
        }
    }
    else
    {
        //qDebug() << "json error:" << json_error.errorString();
        info.set_text(u8"登录失败\r\n无法解析服务器应答！", u8"确认").show(); //读取JSON出现错误，要么服务器返回数据有问题，要么服务器没开
    }

    if(!login_success) //登录失败
    {
        info.set_text(u8"登录失败\r\n用户名或者密码错误！", u8"确认").show();
    }
    reply->deleteLater();     //返回信息，用完就删掉
    check_thread[4].start();  //安全措施，时刻检查登录状态
}

void Widget::dragEnterEvent(QDragEnterEvent* event)
{
    static int i = 0;
    if(i++ > 20)
    {
        i = 0;
        if(this->isHidden() == false)  //当前窗口是隐藏的状态，让它自己去进行安全性检测
        {
            check_thread[5].start();
        }
    }
    event->acceptProposedAction(); //允许拖拽
}
void Widget::dropEvent(QDropEvent* event)
{
    auto files = event->mimeData()->urls();  //向右侧播放列表加入内容
    for (int i = 0; i < files.size() ; i++ )
    {
        QUrl url = files.at(i);
        //qDebug() << url;
        //qDebug() << url.fileName();
        //qDebug() << url.path();
        ui->listWidget->addItem(url.fileName());
        //应该允许存在重复的内容
        mediaList.append(url);
    }
    check_thread[6].start();  //惯例安全性检测
}

void Widget::handleTimeout(int /*nTimerID*/)
{  //不做实现
}

void Widget::mouseMoveEvent(QMouseEvent* event)
{
    //鼠标移动
    if (helper.cur_status() == 0)
    {
        move(event->globalPos() - position);
        helper.modify_mouse_cousor(event->globalPos());
    }
    //qDebug() << __FUNCTION__ << " fullScreenTimerID:" << fullScreenTimerID;
    static int s = 0;
    if(s++ > 50)
    {
        s = 0;
        if(this->isHidden() == false)
        {
            check_thread[7].start();
        }
    }
}

void Widget::mousePressEvent(QMouseEvent* event)
{
    if(helper.type() == 0 && (helper.cur_status() == 0))//只有窗口状态才能移动窗口
    {
        //鼠标按下
        position = event->globalPos() - this->pos();
    }
    //左上，顶，右上，右，右下，底，左下，左
    else if(helper.type() && (helper.cur_status() == 0))
    {
        helper.press(this->pos());
    }
    //qDebug() << __FUNCTION__ << " pos:" << event->globalPos();
    static int s = 0;
    if(s++ > 10)
    {
        s = 0;
        if(this->isHidden() == false)
        {
            check_thread[8].start();
        }
    }
}

void Widget::mouseReleaseEvent(QMouseEvent* /*event*/)
{
    //鼠标释放
    helper.release();
    if(isFullScreen())
    {
        if((helper.cur_status() == 2) && (fullScreenTimerID == -1))
        {
            fullScreenTimerID = startTimer(1500);
            helper.auto_hide(false);
            full_head->show();
            //qDebug() << __FILE__ << "(" << __LINE__ << "):";
        }
    }
    static int s = 0;
    if(s++ > 20)
    {
        s = 0;
        if(this->isHidden() == false)
        {
            check_thread[9].start();
        }
    }
}

bool Widget::eventFilter(QObject* watched, QEvent* event)
{
    if(watched == ui->horizontalSlider)
    {
        if(QEvent::MouseButtonPress == event->type())
        {
            slider_pressed = true;
        }
        else if(QEvent::MouseButtonRelease == event->type())
        {
            slider_pressed = false;
        }
    }
    if(watched == this)
    {
        if(event->type() == QEvent::Paint)
        {
            bool ret = QWidget::eventFilter(watched, event);
            paintLine();  //画那条渐变线
            return ret;
        }
        if(event->type() == QEvent::MouseButtonDblClick)  //双击最大化
        {
            //qDebug() << __FUNCTION__ << "(" << __LINE__ << "):" << event->type();
            if(isMaximized())
            {
                //最大化改为恢复
                on_fangdaButton_clicked();
            }
            else if(isFullScreen() == false)
            {
                //不是最大化也不是全屏，那么就是最小化和普通。但是最小化不会有双击消息，只能是普通状态
                on_fangdaButton_clicked();
            }
        }
    }
    if((event->type() == QEvent::UpdateRequest)
            || (event->type() == QEvent::Paint)
            || (event->type() == QEvent::Timer)
            )
    {
        helper.modify_mouse_cousor(QCursor::pos());
    }
    if(watched == ui->scaleButton)
    {
        if(event->type() == QEvent::HoverEnter)
        {
        }
    }
    if(watched == ui->volumeButton)
    {
        if(event->type() == QEvent::HoverEnter)
        {
            //qDebug() << "enter";
            ui->volumeSlider->setVisible(true);
            if(volumeSliderTimerID == -1)//显示音量，1秒后自动关闭
            {
                volumeSliderTimerID = startTimer(200);
            }
        }
    }
    if(watched == ui->volumeSlider)
    {
        //qDebug() << event->type();
        if((event->type() == QEvent::HoverMove) ||
                (event->type() == QEvent::MouseMove) ||
                (event->type() == QEvent::Wheel))
        {
            volumeCount = 0;
            //qDebug() << volumeCount;
        }
    }
    static int ss = 0;
    if(ss++ > 100)
    {
        ss = 0;
        if(this->isHidden() == false)
        {
            check_thread[GetTickCount()%32].start();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void Widget::keyPressEvent(QKeyEvent* event)
{
    //qDebug() << __FILE__ << "(" << __LINE__ << "):" << __FUNCTION__ << " key:" << event->key();
    //qDebug() << __FILE__ << "(" << __LINE__ << "):" << __FUNCTION__ << isFullScreen();
    if(event->key() == Qt::Key_Escape)
    {
        if(isFullScreen())//全屏状态按下ESC 退出全屏
        {
            on_fullscreenBtn_clicked();
        }
    }
}

void Widget::on_listWidget_itemDoubleClicked(QListWidgetItem*)
{   //在列表上双击，开始播放
    if(ui->listWidget->currentRow() < mediaList.size())
    {
        //qDebug() << __FILE__ << "(" << __LINE__ << "):";
        //qDebug() << ui->listWidget->currentRow();
        int index = ui->listWidget->currentRow();
        QUrl url = mediaList.at(index);
        //qDebug() << url;
        //qDebug() << __FILE__ << "(" << __LINE__ << "):";
        emit stop();
        //qDebug() << __FILE__ << "(" << __LINE__ << "):";
        emit open(url);
        //qDebug() << __FILE__ << "(" << __LINE__ << "):";
        emit play();
        //qDebug() << __FILE__ << "(" << __LINE__ << "):";
        current_play = index;//设置当前播放序号
        QString filename = url.fileName();
        if(filename.size() > 13)
        {
            filename.replace(13, filename.size() - 12, u8"…");
        }
        ui->curplay->setText(filename);
    }
}

void Widget::on_slowButton_clicked()
{
    rate -= 0.25f;
    emit set_scale(rate);
}

void Widget::on_fastButton_clicked()
{
    rate += 0.25f;
    emit set_scale(rate);
}

void Widget::on_volumeSlider_sliderReleased()
{
    emit volume(ui->volumeSlider->value());
}

void Widget::on_volumeSlider_valueChanged(int value)
{
    //0~100
    emit volume(value % 101);
}

void Widget::on_showhideList_pressed()
{
    //隐藏列表和展示列表，同时调整播放区域的宽度和高度
    if(ui->listWidget->isHidden() == false)
    {
        //隐藏
        ui->listWidget->hide();//列表隐藏
        ui->curplay->hide();//隐藏列表上面的信息栏
        ui->playInfoIco->hide();//隐藏列表上面的信息栏图标
        ui->playInfo->hide();//隐藏列表上面的信息栏背景
        ui->showhideList->setStyleSheet(LIST_HIDE);  //showhideList 就是最右侧那个小箭头
        QPoint pt = ui->showhideList->pos();         //右侧列表隐藏之后，这个小箭头需要向右边移动列表宽的位置,体现出一种列表隐藏进了右侧的视觉感受
        int w = ui->listWidget->width();
        ui->showhideList->move(pt.x() + w, pt.y());
        QRect rect = ui->media->frameGeometry();
        ui->media->move(rect.x() + 150, rect.y());
        rect = ui->horizontalSlider->frameGeometry();
        ui->horizontalSlider->setGeometry(rect.left(), rect.top(), rect.width() + 300, rect.height());
    }
    else
    {
        //显示
        ui->listWidget->show();//显示列表
        ui->curplay->show();//显示列表上面的信息栏
        ui->playInfoIco->show();//显示列表上面的信息栏图标
        ui->playInfo->show();//显示列表上面的信息栏背景
        ui->showhideList->setStyleSheet(LIST_SHOW);
        QPoint pt = ui->showhideList->pos();        //显示列表跟上面的移动过程相反，除此之外基本相同
        int w = ui->listWidget->width();
        ui->showhideList->move(pt.x() - w, pt.y());
        QRect rect = ui->media->frameGeometry();
        ui->media->move(rect.x() - 150, rect.y());
        rect = ui->horizontalSlider->frameGeometry();
        ui->horizontalSlider->setGeometry(rect.left(), rect.top(), rect.width() - 300, rect.height());
    }
}

Widget::SizeHelper::SizeHelper(Widget* ui)
{
    curent_coursor = 0;
    isabled = true;             //这个是用来控制窗口大小时鼠标光标的，就是鼠标移动到窗口边缘时会改变鼠标样式
    Qt::CursorShape cursor_type[9] =
    {
        Qt::ArrowCursor, Qt::SizeFDiagCursor, Qt::SizeVerCursor,
        Qt::SizeBDiagCursor, Qt::SizeHorCursor, Qt::SizeFDiagCursor,
        Qt::SizeVerCursor, Qt::SizeBDiagCursor, Qt::SizeHorCursor
    };
    for(int i = 0; i < 9; i++)
    {
        cursors[i] = new QCursor(cursor_type[i]);
    }
    index = 0;
    pressed = false;
    this->ui = ui;
    status = 0;
}

Widget::SizeHelper::~SizeHelper()
{
    //qDebug() << __FILE__ << "(" << __LINE__ << "):";
    for(int i = 0; i < 9; i++)
    {
        delete cursors[i];  //析构时上面的几个全都要一一释放掉
        cursors[i] = NULL;
    }
    //qDebug() << __FILE__ << "(" << __LINE__ << "):";
}

void Widget::SizeHelper::update(int nWidth, int nHeight)
{
    int width = nWidth * 0.01;
    int height = nHeight * 0.01;
    int x_[] =
    {
        //左上，顶，右上，右，右下，底，左下，左
        0/*左上*/, width * 3/*顶上*/, nWidth - 5/*右上*/,
        nWidth - width/*右*/, nWidth - width/*右下*/, width * 3/*底*/,
        0/*左下*/, 0/*左*/
    };
    int y_[] =
    {
        0, 0, 0,//左上，顶，右上
        height * 3, nHeight - 10, nHeight - 10,//右，右下，底
        nHeight - height, height * 3//左下，左
    };
    int w[] =
    {
        width * 2, width * 14, width * 2,
        width * 2, width * 2, width * 14,
        width * 2, width * 2
    };
    int h[] =
    {
        height, height, height,
        height * 14, height, height,
        height, height * 14
    };
    for(int i = 0; i < 8; i++)
    {
        size_rect[i].setX(x_[i]);
        size_rect[i].setY(y_[i]);
        size_rect[i].setWidth(w[i]);
        size_rect[i].setHeight(h[i]);
    }
}

void Widget::SizeHelper::modify_mouse_cousor(const QPoint& point)
{   //更改鼠标光标
    if(pressed == false)
    {
        QPoint pt = point - ui->pos();
        for(int i = 0; i < 8; i++)
        {
            if(size_rect[i].contains(pt))
            {
                if(i + 1 != index)
                {
                    ui->setCursor(*cursors[i + 1]);
                    index = i + 1;
                }
                return;
            }
        }
        if(index != 0)
        {
            ui->setCursor(*cursors[0]);
            index = 0;
        }
    }
}

void Widget::SizeHelper::set_enable(bool isable)
{
    this->isabled = isable;
}

void Widget::SizeHelper::press(const QPoint& point)
{
    if(pressed == false)
    {
        pressed = true;
        this->point = point;
    }
}

void Widget::SizeHelper::move(const QPoint& /*point*/)
{
    //TODO:绘制调整大小的框，不能小于800×600
}

void Widget::SizeHelper::release()
{
    if(pressed)
    {
        pressed = false;
        point.setX(-1);
        point.setY(-1);
    }
}

void Widget::SizeHelper::init_size_info(QWidget* widget)
{
    SizeInfo info;
    info.widget = widget;
    info.org_rect = widget->frameGeometry();
    info.last_rect = widget->frameGeometry();
    //qDebug() << "rect:" << info.org_rect;
    sub_item_size.insert(widget->objectName(), info);
}

void Widget::SizeHelper::set_full_rect(const QString& name, const QRect& rect)
{
    auto it = sub_item_size.find(name);     //设置全屏时的样式
    if(it != sub_item_size.end())
    {
        sub_item_size[name].full_rect = rect;
    }
}

void Widget::SizeHelper::set_max_rect(const QString& name, const QRect& rect)
{
    auto it = sub_item_size.find(name);     //设置最大化时的样式
    if(it != sub_item_size.end())
    {
        sub_item_size[name].max_rect = rect;
    }
}

void Widget::SizeHelper::set_full_hide(const QString& name, bool is_hide)
{
    auto it = sub_item_size.find(name);     //设置全屏时隐藏
    if(it != sub_item_size.end())
    {
        sub_item_size[name].is_full_hide = is_hide;
    }
}

void Widget::SizeHelper::set_max_hide(const QString& name, bool is_hide)
{
    auto it = sub_item_size.find(name);     //设置最大化时隐藏
    if(it != sub_item_size.end())
    {
        sub_item_size[name].is_max_hide = is_hide;
    }
}

void Widget::SizeHelper::set_auto_hide(const QString& name, int hide_status)
{
    auto it = sub_item_size.find(name);     //设置自动隐藏
    if(it != sub_item_size.end())
    {
        sub_item_size[name].auto_hide_status = hide_status;
    }
}

void Widget::SizeHelper::full_size()
{
    status = 2;
    for(auto it = sub_item_size.begin(); it != sub_item_size.end(); it++)
    {
        qDebug() << (*it).widget->objectName() << (*it).max_rect << (*it).full_rect;
        if((*it).full_rect.width() > 0)
        {
            (*it).widget->setGeometry((*it).full_rect);
            //qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):" << (*it).full_rect;
        }
        (*it).widget->setHidden((*it).is_full_hide);
    }
}

void Widget::SizeHelper::org_size()
{
    status = 0;
    for(auto it = sub_item_size.begin(); it != sub_item_size.end(); it++)
    {
        (*it).widget->setGeometry((*it).org_rect);
        if((*it).widget->objectName() == "screentop") //顶部半透明框，要隐藏
        {
            (*it).widget->setHidden(true);
        }
        if((*it).widget->objectName() == "volumeSlider") //音量条，要隐藏
        {
            (*it).widget->setHidden(true);
        }
        if((*it).widget->objectName() == "fangdaButton") //放大按钮
        {
            (*it).widget->setStyleSheet(SCREEN_MAX_STYLE);
        }
    }
}

void Widget::SizeHelper::max_size()
{
    status = 1;
    for(auto it = sub_item_size.begin(); it != sub_item_size.end(); it++)
    {
        qDebug()<<(*it).widget->objectName()<<(*it).max_rect<<(*it).full_rect;
        if((*it).max_rect.width() > 0)
        {
            (*it).widget->setGeometry((*it).max_rect);
        }
        (*it).widget->setHidden((*it).is_max_hide);
        if((*it).widget->objectName() == "screentop")
        {
            (*it).widget->setHidden(true);
        }
        else if((*it).widget->objectName() == "fangdaButton")  //基本同上
        {
            (*it).widget->setStyleSheet(SCREEN_RESTORE_STYLE);
        }
        else if((*it).widget->objectName() == "volumeSlider")
        {
            (*it).widget->setHidden(true);
        }
    }
}

int Widget::SizeHelper::cur_status() const
{
    return status;
}

void Widget::SizeHelper::auto_hide(bool hidden)
{
    //qDebug() << __FILE__ << "(" << __LINE__ << "):status=" << status;
    //qDebug() << __FILE__ << "(" << __LINE__ << "):hidden=" << hidden;

    if(status == 2)     //全屏的时候才起效
    {
        for(auto it = sub_item_size.begin(); it != sub_item_size.end(); it++)
        {
            //qDebug() << __FILE__ << "(" << __LINE__ << "):object=" << (*it).widget->objectName();
            //qDebug() << __FILE__ << "(" << __LINE__ << "):auto_hide_status=" << (*it).auto_hide_status;
            if((*it).widget->objectName() == "volumeSlider")
            {
                (*it).widget->setHidden(true);
            }
            else if((*it).auto_hide_status == 1)
            {
                (*it).widget->setHidden(hidden);
                //qDebug() << __FILE__ << "(" << __LINE__ << "):hidden=" << hidden;
            }
        }
    }
}

void Widget::on_closeButton_released()
{
    emit close();
}


void Widget::on_fangdaButton_clicked()
{
    /*
     * 1 修改图标
     * 2 隐藏列表
     * 3 开启定时，1.5秒后隐藏进度等控件
    */
    if(isMaximized() == false)
    {
        //从正常尺寸转移到最大化尺寸
        qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):";
        showMaximized();                            //Widget的函数，显示最大化(自定义ui得自己跳)

        qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):";
        helper.max_size();                          //自己调整最大化时ui的大小

        qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):";
        ui->showhideList->setStyleSheet(LIST_SHOW); //切换全屏的时候，隐藏列表按钮状态设置为初始状态

        qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):";
        ui->listWidget->setHidden(false);           //最大化时播放列表不隐藏

        qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):";
    }
    else
    {
        //从最大化尺寸转移到正常尺寸
        qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):";
        showNormal();                               //Widget的函数，窗口显示正常大小(自定义ui得自己调整)

        qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):";
        helper.org_size();                          //自定义ui，显示原始大小

        qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):";
        ui->listWidget->setHidden(false);           //恢复原大小，播放列表不隐藏

        qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):";
        ui->showhideList->setStyleSheet(LIST_SHOW); //切换全屏的时候，隐藏列表按钮状态设置为初始状态

        qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):";
        ui->userInfo->setHidden(false);             //顶部用户信息也不隐藏
    }
}

void Widget::on_fullscreenBtn_clicked()
{
    if(isFullScreen() == false) //全屏按钮，关键在于该隐藏的全隐藏掉
    {
        QString style = "QPushButton{border-image: url(:/UI/images/tuichuquanping.png);}\n";
        style += "QPushButton:hover{border-image:url(:/UI/images/tuichuquanping-hover.png);}\n";
        style += "QPushButton:pressed{border-image: url(:/UI/images/tuichuquanping.png);}";
        ui->fullscreenBtn->setStyleSheet(style);
        showFullScreen();                           //Widget的函数，窗口显示全屏化(自定义ui得自己调整)

        qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):";
        helper.full_size();                         //调整自定义ui的大小为全屏样式

        qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):";
        ui->showhideList->setStyleSheet(LIST_SHOW); //切换全屏的时候，隐藏列表按钮状态设置为初始状态

        qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):";
        full_head->show();                          //全屏时顶部版透明狂要隐藏

        qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):" << current_play;
        QString filename = current_play>=0 ? mediaList.at(current_play).fileName() : "";
        qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):";

        if(filename.size() > 12)
        {
            filename.replace(12, filename.size() - 12, u8"…");
        }
        qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):";
        full_head->setText(filename);

        //QToolTip::showText(QPoint(10, 10), ui->curplay->text(), nullptr, QRect(0, 0, 210, 30), 2950);
        fullScreenTimerID = startTimer(1500);       //1.5秒后隐藏可以隐藏的控件
        screentopTimerID = startTimer(50);
        qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):";
    }
    else
    {
        qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):";
        //从最大化尺寸转移到正常尺寸
        QString style = "QPushButton{border-image: url(:/UI/images/quanping.png);}\n";
        style += "QPushButton:hover{border-image:url(:/UI/images/quanping-hover.png);}\n";
        style += "QPushButton:pressed{border-image: url(:/UI/images/quanping.png);}";
        ui->fullscreenBtn->setStyleSheet(style);
        showMaximized();                            //转移到正常尺寸，默认返回到最大化状态，也就是从全屏->最大化
        qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):";
        helper.max_size();
        qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):";
        full_head->setHidden(true);
        killTimer(fullScreenTimerID);
        fullScreenTimerID = -1;
        killTimer(screentopTimerID);
        screentopTimerID = -1;
    }
}

void Widget::on_stopButton_clicked()
{
    emit stop();
    setTime(0, 0, 0);
    ui->horizontalSlider->setValue(0);
}

void Widget::on_time2_clicked()
{
    ui->media->set_scale(2.0);
    if(timesID >= 0)
    {
        killTimer(timesID);
        timesID = -1;
        //播放倍数
        ui->time0_5->setVisible(false);
        ui->time1->setVisible(false);
        ui->time1_5->setVisible(false);
        ui->time2->setVisible(false);
    }
}

void Widget::on_time1_5_clicked()
{
    ui->media->set_scale(1.5);
    if(timesID >= 0)
    {
        killTimer(timesID);
        timesID = -1;
        //播放倍数
        ui->time0_5->setVisible(false);
        ui->time1->setVisible(false);
        ui->time1_5->setVisible(false);
        ui->time2->setVisible(false);
    }
}

void Widget::on_time1_clicked()
{
    ui->media->set_scale(1.0);
    if(timesID >= 0)
    {
        killTimer(timesID);
        timesID = -1;
        //播放倍数
        ui->time0_5->setVisible(false);
        ui->time1->setVisible(false);
        ui->time1_5->setVisible(false);
        ui->time2->setVisible(false);
    }
}

void Widget::on_time0_5_clicked()
{
    ui->media->set_scale(0.5);
    if(timesID >= 0)
    {
        killTimer(timesID);
        timesID = -1;
        //播放倍数
        ui->time0_5->setVisible(false);
        ui->time1->setVisible(false);
        ui->time1_5->setVisible(false);
        ui->time2->setVisible(false);
    }
}
QString getTime();
bool Widget::keep_activity()
{
    QCryptographicHash md5(QCryptographicHash::Md5);                            //这里有一个加密, 但是，好像没用？
    QNetworkRequest request;
    QString url = QString(HOST) + "/keep?";
    QString salt = QString::number(QRandomGenerator::global()->bounded(10000, 99999));
    QString time = getTime();
    md5.addData((time + MD5_KEY + nick + salt).toUtf8());
    QString sign = md5.result().toHex();
    url += "time=" + time + "&";
    url += "salt=" + salt + "&";
    url += "user=" + nick + "&";
    url += "sign=" + sign;
    qDebug() << __FILE__ << (__LINE__) << url;
    request.setUrl(QUrl(url));
    net->get(request);
    return false;
    /*
     * 猜测这个函数的用处是发送一个心跳请求，由服务端来判断这个客户端还有没有心跳，如果一段时间没发，就可以让他下线
     * 可以合理猜测，api被被人调用了，他没有发心跳请求，判断他不是我们的客户端
     * 但是我记得易播服务器是没有处理这个keep的请求的，只处理了登录，剩下的可以自己扩展
     */
}

void Widget::on_miniButton_clicked()
{
    this->showMinimized();
}
