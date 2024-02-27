#include "loginform.h"
#include "ui_loginform.h"
#include "widget.h"             //登录成功之后要显示主窗口Widget，要用的上Widget
#include <time.h>               //计时器
#include <QPixmap>              //绘图使用
#include <QMessageBox>          //消息提示框，较常用
#include <QRandomGenerator>     //随机数生成器
#include <QCryptographicHash>   //加密算法
//y%dcunyd6x202lmfm9=-y7#bd-w(ro4*(9u$0i-3#$txwbkzg$
const char* MD5_KEY = "*&^%$#@b.v+h-b*g/h@n!h#n$d^ssx,.kl<kl";      //加密字符串，脸滚键盘获得，服务端和客户端别错了就行
const char* HOST = "http://192.168.65.130:10721";                   //服务器的ip
//const char* HOST = "http://127.0.0.1:19527";
bool LOGIN_STATUS = false;  //登录状态，默认为false

LoginForm::LoginForm(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::LoginForm),      //new一个ui界面，在构造时就创建好界面，启动程序后先显示登陆界面，之后才显示主界面
    auto_login_id(-1)//定时器id
{
    record = new RecordFile("edoyun.dat");      //自动登录或者保存密码使用的dat文件，RecordFile是自定义的文件类
    ui->setupUi(this);
    this->setWindowFlag(Qt::FramelessWindowHint);  //setWindowFlag设置窗口的属性 FramelessWindowHint无边框的窗口
    connect(ui->btnExit, SIGNAL(pressed()), this, SLOT(on_btnExit_pressed()));

    //头像进行缩放
    ui->nameEdit->setPlaceholderText(u8"用户名/手机号/邮箱");  //nameEdit是QLabel u8为了跨平台，设置字符集为utf8，避免中文乱码
    ui->nameEdit->setFrame(false); //无边框
    ui->nameEdit->installEventFilter(this); //添加事件过滤，控制输入格式
    ui->pwdEdit->setPlaceholderText(u8"填写密码");
    ui->pwdEdit->setFrame(false);
    ui->pwdEdit->setEchoMode(QLineEdit::Password); //设置密码框的输入格式为密码格式，即输入的数字会变成····
    ui->pwdEdit->installEventFilter(this); //同样安装事件过滤控制输入格式
    ui->forget->installEventFilter(this);  //忘记密码，莫得功能，可以跳转到外部页面
    net = new QNetworkAccessManager(this); //实例化一个网络管理器的对象负责整个登录窗口的网络连接API,即连接服务器进行登录验证等操作
    connect(net, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(slots_login_request_finshed(QNetworkReply*)));  //连接finished信号和自定义槽函数，用于获取网络请求后的返回数据，即服务器的返回状态
    info.setWindowFlag(Qt::FramelessWindowHint); //信息提示窗口也是无边框模式
    info.setWindowModality(Qt::ApplicationModal); //设置程序为模态窗口
                                                  //Qt::NonModal 非模态：正常模式
                                                  //Qt::WindowModal 半模态：窗口级模态对话框，阻塞父窗口、父窗口的父窗口及兄弟窗口。
                                                  //Qt::ApplicationModal 模态：应用程序级模态对话框，阻塞整个应用程序的所有窗口
    QSize sz = size(); //获取当前窗口的大小
    info.move((sz.width() - info.width()) / 2, (sz.height() - info.height()) / 2); //用当前窗口的size大小来计算信息提示窗口的显示未知
    load_config();  //如果dat文件内用账密信息就载入，没有就是空(有一个小bug，没有点记住密码仍然保存了数据)                                        //TODO...
}

LoginForm::~LoginForm()
{   //析构函数的作用就是将所有new出来的对象全部delete掉
    //qDebug() << __FILE__ << "(" << __LINE__ << "):";
    delete record;
    delete ui;
    delete net;
    //qDebug() << __FILE__ << "(" << __LINE__ << "):";
}

bool LoginForm::eventFilter(QObject* watched, QEvent* event)
{   //事件过滤器可以实现在窗体中监视全部控件的不同事件，方便实现功能扩展。需要用到的控件都安装eventFilter，可以方便监控所有安装了的控件,在鼠标到达前对事件进行响应
    if(ui->pwdEdit == watched) //watched为当前需要响应事件的控件
    {
        if(event->type() == QEvent::FocusIn) //判断应该响应的事件的类型，FocusIn获得焦点，FocusOut失去焦点
        {
            ui->pwdEdit->setStyleSheet("color: rgb(251, 251, 251);background-color: transparent;"); //color字体颜色 transparent背景透明
        }                                       //白色，输入时的字体颜色
        else if(event->type() == QEvent::FocusOut)  //鼠标离开，失去焦点
        {
            if(ui->pwdEdit->text().size() == 0)  //鼠标离开时还是空的，说明没有输入
            {
                ui->pwdEdit->setStyleSheet("color: rgb(71, 75, 94);background-color: transparent;");
            }                                       //一个偏灰的字体，显示的是"填写密码"的默认值
        }
    }
    else if(ui->nameEdit == watched)
    {
        if(event->type() == QEvent::FocusIn)
        {
            ui->nameEdit->setStyleSheet("color: rgb(251, 251, 251);background-color: transparent;");
        }
        else if(event->type() == QEvent::FocusOut)
        {
            if(ui->nameEdit->text().size() == 0)
            {
                ui->nameEdit->setStyleSheet("color: rgb(71, 75, 94);background-color: transparent;");
            }
        }
    }
    if((ui->forget == watched) && (event->type() == QEvent::MouseButtonPress)) //将要执行事件的是forget的label，并且是鼠标按下的情况
    {                                                                          //label没有buttonPress一样的槽函数，所以需要通过这种方式达到一种类似buttonPress的效果
        //QDesktopServices::openUrl(QUrl(QString(HOST) + "/forget"));       //打开网页，主机ip加forget域名，但是这个网页没做
        QDesktopServices::openUrl(QUrl(QString("https://cn.bing.com/#!"))); //打开一下bing凑合一下
    }
    return QWidget::eventFilter(watched, event);  //不能关闭，需要交给父类继续处理剩余事件，举例：点击完账号框继续点密码框，而不是输完账号就结束了
}

void LoginForm::timerEvent(QTimerEvent* event)
{   //给自动登录用的
    if(event->timerId() == auto_login_id) //当前计时器的id和自动登录id相同就启动自动登录程序,一个客户端会有一个自动登录id，客户端启动会伴随一个定时器，如果定时跟id对应上就会开始计时
    {   //timerID，如果有多个定时器激活，可以通过id判断是哪一个定时器                                             (在自动登录框勾选上的状态下)   计时结束用户没操作就会启动自动登录
        killTimer(auto_login_id); //实现只触发一次的定时器，用完就kill掉，登录也只需一次
        QJsonObject& root = record->config(); //从本地的RcordFile创建的dat文件中读取保存的账号密码信息 格式为Json
        QString user = root["user"].toString();    //从Json数据中解析账号密码
        QString pwd = root["password"].toString();
        check_login(user, pwd);                    //然后交给登录检查程序直接进行登录
    }
}

void LoginForm::on_logoButton_released()            //一个小问题，放着不管时间长了数据库自己会断开连接...
{
    if(ui->logoButton->text() == u8"取消自动登录") //当用户选择了自动登录，再次启动客户端时登录按钮会变成取消自动登录状态，给用户一个反应机会
    {
        killTimer(auto_login_id); //登录还是一次性的计时器
        auto_login_id = -1;       //重新赋值为-1
        ui->logoButton->setText(u8"登录"); //取消自动登录后按钮重新变回登录状态
    }
    else  //这种情况下说明用户没有输账户或密码但是仍然选择了自动登录
    {
        QString user = ui->nameEdit->text();
        //检查用户名的有效性
        if(user.size() == 0 || user == u8"用户名/手机号/邮箱")
        {
            info.set_text(u8"用户不能为空\r\n请输入用户名", u8"确认").show();  //设置消息提示框的文字并显示
            ui->nameEdit->setFocus();  //设置焦点，以提示用户输入账号信息
            return;
        }
        //检查密码的有效性
        QString pwd = ui->pwdEdit->text();   //同上
        if(pwd.size() == 0 || pwd == u8"填写密码")
        {
            info.set_text(u8"密码不能为空\r\n请输入密码", u8"确认").show();
            ui->pwdEdit->setFocus();
            return;
        }
        //运行到这里，说明1、用户取消了自动登录的过程 2、账号密码都输了，该检查对不对了，下一步：链接服务器检查登录
        check_login(user, pwd);
    }
}

void LoginForm::on_remberPwd_stateChanged(int state)    //记住密码的复选框
{
    //记住密码状态改变
    //qDebug() << __FILE__ << "(" << __LINE__ << "):";
    record->config()["remember"] = state == Qt::Checked;  //在配置文件中同步修改
    if(state != Qt::Checked)
    {
        //qDebug() << __FILE__ << "(" << __LINE__ << "):";
        //ui->autoLoginCheck->setChecked(false);//关闭记住密码，则取消自动登录
    }
}

void LoginForm::slots_autoLoginCheck_stateChange(int state)
{
    //qDebug() << __FILE__ << "(" << __LINE__ << "):";
    record->config()["auto"] = state == Qt::Checked;  //同步配置文件的状态
    if(state == Qt::Checked)
    {
        record->config()["remember"] = true;
        ui->remberPwd->setChecked(true);    //自动登录会开启记住密码
        //ui->remberPwd->setCheckable(false);//禁止修改状态
        //qDebug() << __FILE__ << "(" << __LINE__ << "):";
    }
    else
    {
        ui->remberPwd->setCheckable(true);//启动修改状态
        //qDebug() << __FILE__ << "(" << __LINE__ << "):";
    }
    //qDebug() << __FILE__ << "(" << __LINE__ << "):";
}

void LoginForm::slots_login_request_finshed(QNetworkReply* reply)  //对应构造函数里的connect，负责获取登录请求的响应结果
{
    this->setEnabled(true);
    bool login_success = false;   //登录成功的状态标识，默认为false
    if(reply->error() != QNetworkReply::NoError)  //获得登录请求时发生一些莫名其妙的错误等，比如：服务器没开,会卡死
    { //响应NoError状态需要一定的时间
        info.set_text(u8"登录发生错误\r\n" + reply->errorString(), u8"确认").show();
        return;
    }

    QByteArray data = reply->readAll(); //reply中存储了服务端发回来的数据信息，用reeadAll读取全部，用字节数组存储
    qDebug() << data;  //打印到控制台上显示
    QJsonParseError json_error;    //Json解析器，用于判断是否解析出错
    QJsonDocument doucment = QJsonDocument::fromJson(data, &json_error);  //Json解析出的文本，参数传入错误表示，data为解析前字节数组，doucment为解析后的Json数据
    qDebug() << "json error = "<<json_error.error;
    if (json_error.error == QJsonParseError::NoError) //解析过程莫得出错
    {
        if (doucment.isObject())  //如果服务端返回的是一个对象（Json是可以保存对象的）(在设计服务器的时候确实返回的是一个对象)
        {
            const QJsonObject obj = doucment.object();  //转换为Json对象，Json库里封装的有JsonObject这个类
            if (obj.contains("status") && obj.contains("message"))  //应该包含status和message两个状态信息
            {
                QJsonValue status = obj.value("status");  //拿到这两个值，但是也是Json值，所以用JsonValue来保存
                QJsonValue message = obj.value("message");
                if(status.toInt(-1) == 0) //登录成功,能看到服务器的success
                {
                    //TODO:token 要保存并传递widget 用于保持在线状态
                    LOGIN_STATUS = status.toInt(-1) == 0;
                    emit login(record->config()["user"].toString(), QByteArray());  //发射信号，告知哪个user登录成功,应该是用于传递给widget保持在线状态的，但实际并没有connect去实现
                        //emit的作用：A类的信号于B类的槽函数的链接和使用，需要用connect进行链接
                    hide();  //登录成功隐藏登录页面，显示主窗口（在main里面）
                    login_success = true;  //登录成功标识

                    char tm[64] = "";
                    time_t t;
                    time(&t);
                    strftime(tm, sizeof(tm), "%Y-%m-%d %H:%M:%S", localtime(&t));
                    record->config()["date"] = QString(tm);//更新登录时间,并保存在配置文件里

                    record->save(); //保存文件,里面有关闭文件file->close
                }
            }
        }
    }
    else
    {
        //qDebug() << "json error:" << json_error.errorString();
        info.set_text(u8"登录失败\r\n无法解析服务器应答！", u8"确认").show();
    }
    if(!login_success)
    {
        info.set_text(u8"登录失败\r\n用户名或者密码错误！", u8"确认").show();
    }

    reply->deleteLater();   //QNetworkReply对象需要手动在最后释放掉，释放方式为:deleteLater()
}

void LoginForm::on_btnExit_pressed()
{
    exit(0);  //只是一个触发退出的按钮
}

QString getTime() //获取时间
{
    time_t t = 0;
    time (&t);
    return QString::number(t);
}

bool LoginForm::check_login(const QString& user, const QString& pwd)
{
    QCryptographicHash md5(QCryptographicHash::Md5); //md5加密字符串
    QNetworkRequest request;    //网路请求，就是把账密加密后封装进网络请求内然后发给服务端
    QString url = QString(HOST) + "/login?";   //这个url对应的是服务端的
    QString salt = QString::number(QRandomGenerator::global()->bounded(10000, 99999));  //md5加密时需要的一个盐值，用随机数生成
    QString time = getTime(); //md5加密时需要的时间戳
    qDebug().nospace()<< __FILE__ << "(" << __LINE__ << "):" <<time + MD5_KEY + pwd + salt; //md5加密字符串的参数：时间戳，key，密码，盐值
    md5.addData((time + MD5_KEY + pwd + salt).toUtf8()); //生成加密字符串
    QString sign = md5.result().toHex(); //转换成十六进制
    url += "time=" + time + "&";    //url的key=value值设置为刚才的数据
    url += "salt=" + salt + "&";
    url += "user=" + user + "&";
    url += "sign=" + sign;
    //qDebug() << url;
    request.setUrl(QUrl(url)); //发送请求的url，服务端那边接收到数据是以url的json数据接收的
    record->config()["password"] = ui->pwdEdit->text(); //设置配置文件信息
    record->config()["user"] = ui->nameEdit->text();
    this->setEnabled(false);
    net->get(request);   //没找到get是啥：推测，net是reply，那么get应该是获取请求的返回结果
    return true;
    /*
    LOGIN_STATUS = true;
    emit login(record->config()["user"].toString(), QByteArray());
    hide();
    char tm[64] = "";
    time_t t;
    ::time(&t);
    strftime(tm, sizeof(tm), "%Y-%m-%d %H:%M:%S", localtime(&t));
    record->config()["date"] = QString(tm);//更新登录时间
    record->save();
    return false;*/
}

void LoginForm::load_config()
{
    connect(ui->autoLoginCheck, SIGNAL(stateChanged(int)),
            this, SLOT(slots_autoLoginCheck_stateChange(int)));  //连接自动登录的复选框
    QJsonObject& root = record->config(); //从配置文件读取Json对象
    ui->remberPwd->setChecked(root["remember"].toBool());  //是否选择记住密码
    ui->autoLoginCheck->setChecked(root["auto"].toBool()); //是否选择自动登录
    QString user = root["user"].toString();   //读取user信息
    QString pwd = root["password"].toString(); //读取密码信息
    ui->nameEdit->setText(user); //设置用户名
    ui->pwdEdit->setText(pwd);   //设置密码
    qDebug() << "auto:" << root["auto"].toBool();
    qDebug() << "remember:" << root["remember"].toBool();
    if(root["auto"].toBool()) //如果开启了自动登录，则检查用户名和密码是否ok
    {
        qDebug() << __FILE__ << "(" << __LINE__ << "):user=" << user;
        qDebug() << __FILE__ << "(" << __LINE__ << "):pwd=" << pwd;
        if(user.size() > 0 && pwd.size() > 0)
        {
            ui->logoButton->setText(u8"取消自动登录");
            auto_login_id = startTimer(3000);//给3秒的时间，方便用户终止登录过程
        }
    }
}

void LoginForm::mouseMoveEvent(QMouseEvent* event) //提供窗口拖拽
{
    move(event->globalPos() - position);
}

void LoginForm::mousePressEvent(QMouseEvent* event)
{
    position = event->globalPos() - this->pos();
}

void LoginForm::mouseReleaseEvent(QMouseEvent* /*event*/)
{
}
