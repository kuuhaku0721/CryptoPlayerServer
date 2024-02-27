#include "vlchelper.h"
#include <functional>
#include <QDebug>
#include <QTime>
#include <QRandomGenerator>
#include <QImage>
//vlc 播放进度回调
//拖拽进度、播放速度、停止播放
using namespace std;
using namespace std::placeholders;
void libvlc_log_callback(void* data, int level, const libvlc_log_t* ctx,
                         const char* fmt, va_list args)
{   //libvlc的日志回调函数，调试的时候打日志用的
    //qDebug() << "log level:" << level;
}
vlchelper::vlchelper(QWidget* widget)
    : m_logo(":/ico/UI/ico/128-128.png")
{
    //1、设置初始化参数
    const char* const args[] =
    {
        "--sub-filter=logo",
        "--sub-filter=marq"
    };
    qDebug() << __FUNCTION__ << ":" << __LINE__;
    //2、初始化
    m_instance = libvlc_new(2, args);   //m_instance: libvlc_instance_t* libvlc的实例化对象
    if(m_instance != NULL)
    { //初始化libvlc成功
        qDebug() << __FUNCTION__ << ":" << __LINE__;
        m_media = new vlcmedia(m_instance); //m_media: vlcmedia*
    }
    else
    { //初始化失败
        qDebug() << __FUNCTION__ << ":" << __LINE__;
        m_media = NULL;
        throw QString("是否没有安装插件？！！");
    }
    //3、设置日志回调
    libvlc_log_set(m_instance, libvlc_log_callback, this);
    //4、设置剩余参数
    m_player = NULL;    //媒体决定播放器，所以播放器先设置为空
    m_hWnd = (HWND)widget->winId(); //HWND:拿到窗口的句柄
    winHeight = widget->frameGeometry().height(); //高
    winWidth = widget->frameGeometry().width();   //宽
    m_widget = widget;
    qDebug() << "*winWidth:" << winWidth;
    qDebug() << "*winHeight:" << winHeight;
    m_isplaying = false; //是否正在播放: false
    m_ispause = false;   //是否暂停: false
    m_volume = 100;      //音量: 默认最大
}

vlchelper::~vlchelper()
{   //析构函数中，所有new出来的对象全都要对应delete掉，或者调用它自己的free或release函数
    if(m_player != NULL)
    {
        stop(); //播放器需要先停止播放，然后释放掉
        libvlc_media_player_set_hwnd(m_player, NULL);
        libvlc_media_player_release(m_player);
        m_player = NULL;
    }
    if(m_media != NULL)
    {
        m_media->free();
        m_media = NULL;
    }
    if(m_instance != NULL)
    {
        libvlc_release(m_instance);
        m_instance = NULL;
    }
}

int vlchelper::prepare(const QString& strPath)
{
    qDebug() << strPath;
    //m_media = libvlc_media_new_location(
    //              m_instance, strPath.toStdString().c_str());
    *m_media = strPath; //设置媒体源路径
    if(m_media == NULL)
    {   //如果m_media为NULL说明初始化失败，直接退出
        return -1;
    }
    if(m_player != NULL)
    {   //如果m_player 不等于NULL，说明上一个播放源没有正确释放，需要先释放掉上一个然后接着这次使用
        libvlc_media_player_release(m_player);
    }
    m_player = libvlc_media_player_new_from_media(*m_media); //从 媒体源 获得 播放器
    if(m_player == NULL)
    {   //初始化失败，直接退出
        return -2;
    }
    m_duration = libvlc_media_get_duration(*m_media);   //获取播放的媒体（也就是视频）的总时长
    libvlc_media_player_set_hwnd(m_player, m_hWnd);     //设置视频的播放窗口
    libvlc_audio_set_volume(m_player, m_volume);        //设置初始的播放音量
    libvlc_video_set_aspect_ratio(m_player, "16:9");    //设置屏幕比例，一般16:9
    if(text.size() > 0)
    {   //>0 说明有需要显示的漂浮文字(就是那个root),调用对应函数让它显示出来
        set_float_text();
    }
    m_ispause = false;  //初始化暂停状态
    m_isplaying = false;//初始化播放状态
    m_issilence = false;//初始是无静音状态
    if(m_widget->frameGeometry().height() != winHeight) //规范化屏幕大小
    {
        winHeight = m_widget->frameGeometry().height();
    }
    if(m_widget->frameGeometry().width() != winWidth)
    {
        winWidth = m_widget->frameGeometry().width();
    }
    qDebug() << "*winWidth:" << winWidth;
    qDebug() << "*winHeight:" << winHeight;
    return 0;
}

int vlchelper::play()
{
    if(m_player == NULL)
    { //依旧是为空就直接退出，初始化就已经失败了就没必要继续下去了
        return -1;
    }
    if(m_ispause)//如果是暂停，则直接使用play来恢复
    {
        int ret = libvlc_media_player_play(m_player);   //在licvlc中，控制的主体是player，也就是主体是播放器
        if(ret == 0)//如果执行成功，则改变暂停状态
        {
            m_ispause = false; //没暂停
            m_isplaying = true;//正在播放
        }
        return ret;
    }
    if((m_player == NULL) ||//没有设置媒体
            (m_media->path().size() <= 0))  //或媒体的路径出错(就是没有选择媒体源)
    {
        m_ispause = false;  //没暂停
        m_isplaying = false;//没播放
        return -2; //出现这种情况就是出错了，就没必要继续下去了
    }
    m_isplaying = true;
    libvlc_video_set_mouse_input(m_player, 0); //使得vlc不处理鼠标交互，方便qt处理
    libvlc_video_set_key_input(m_player, 0); //使得vlc不处理键盘交互，方便qt处理
    libvlc_set_fullscreen(m_player, 1);             //这里三个处理，注意参数，仍然是以播放器为控制主体
    return libvlc_media_player_play(m_player);
}
int vlchelper::pause()
{
    if(m_player == NULL)
    {   //为空就是有错，就不用继续下去了，一个管理性的安全性判断
        return -1;
    }
    libvlc_media_player_pause(m_player); //使用libvlc的控制函数暂停播放
    m_ispause = true;
    m_isplaying = false;
    return 0;
}
int vlchelper::stop()
{
    if(m_player != NULL)
    {   //停止跟暂停不一样，如果播放器不为空需要先调用stop然后再退出
        libvlc_media_player_stop(m_player);
        m_isplaying = false;
        return 0;
    }
    return -1;
}
int vlchelper::volume(int vol)
{
    if(m_player == NULL)
    { //安全性判断
        return -1;
    }
    if(vol == -1)
    { //音量为-1，说明定义音量的时候出错了，直接退出
        return m_volume;
    }
    int ret = libvlc_audio_set_volume(m_player, vol); //调用libvlc的音量控制函数
    if(ret == 0)
    {
        m_volume = vol; //正常情况，设置音量后，参数也跟着变化一下
        return m_volume; //返回音量值，也就是不为-1
    }
    return ret; //有问题就返回ret
}

int vlchelper::silence()
{
    if(m_player == NULL)
    { //惯例安全性判断
        return -1;
    }
    if(m_issilence)
    { //已经是静音状态
        //恢复
        libvlc_audio_set_mute(m_player, 0);
        m_issilence = false;
    }
    else
    {
        //静音
        m_issilence = true;
        libvlc_audio_set_mute(m_player, 1);
    }
    return m_issilence; //返回是否已经静音的状态
}

bool vlchelper::isplaying()
{
    return m_isplaying;
}

bool vlchelper::ismute()
{
    if(m_player && m_isplaying) //播放器存在并且正在播放
    {
        return libvlc_audio_get_mute(m_player) == 1;
    }
    return false;
}

libvlc_time_t vlchelper::gettime()
{
    if(m_player == NULL)
    {
        return -1;
    }
    return libvlc_media_player_get_time(m_player); //调用libvlc的函数 获取播放时间
}

libvlc_time_t vlchelper::getduration()
{
    if(m_media == NULL)
    {
        return -1;
    }
    if(m_duration == -1)
    { //进度条长度为-1，出错或者没有成功获取进度条
        m_duration = libvlc_media_get_duration(*m_media); //调用libvlc的函数获取进度条时长
    }
    return m_duration;
}

int vlchelper::settime(libvlc_time_t time)
{
    if(m_player == NULL)
    {
        return -1;
    }
    libvlc_media_player_set_time(m_player, time); //libvlc的函数 设置播放时间，也就是拖动进度条
    return 0;
}

int vlchelper::set_play_rate(float rate)
{
    if(m_player == NULL)
    {
        return -1;
    }
    return libvlc_media_player_set_rate(m_player, rate); //libvlc的函数 设置播放速率
}

float vlchelper::get_play_rate()
{
    if(m_player == NULL)
    {
        return -1.0;
    }
    return libvlc_media_player_get_rate(m_player); //libvlc的函数 获取播放速率    以上这些注意参数，都是player "播放器"
}

void vlchelper::init_logo()
{
    //libvlc_video_set_logo_int(m_player, libvlc_logo_file, m_logo.handle());
    libvlc_video_set_logo_string(m_player, libvlc_logo_file, "128-128.png"); //Logo 文件名
    libvlc_video_set_logo_int(m_player, libvlc_logo_x, 0); //logo的 X 坐标。
    //libvlc_video_set_logo_int(m_player, libvlc_logo_y, 0); // logo的 Y 坐标。
    libvlc_video_set_logo_int(m_player, libvlc_logo_delay, 100);//标志的间隔图像时间为毫秒,图像显示间隔时间 0 - 60000 毫秒。
    libvlc_video_set_logo_int(m_player, libvlc_logo_repeat, -1); // 标志logo的循环,  标志动画的循环数量。-1 = 继续, 0 = 关闭
    libvlc_video_set_logo_int(m_player, libvlc_logo_opacity, 122); //logo的不透明度 logo 透明度 (数值介于 0(完全透明) 与 255(完全不透明)
    libvlc_video_set_logo_int(m_player, libvlc_logo_position, 5);
    //1 (左), 2 (右), 4 (顶部), 8 (底部), 5 (左上), 6 (右上), 9 (左下), 10 (右下),您也可以混合使用这些值，例如 6=4+2    表示右上)。
    libvlc_video_set_logo_int(m_player, libvlc_logo_enable, 1); //设置允许添加logo
}

void vlchelper::init_text(const QString& text)
{
    this->text = text; //初始化浮动文字，也就是设置一下文字参数
}

void vlchelper::update_logo()
{ //更新logo,比如全屏或者最大化的时候，logo的位置需要变化
    static int alpha = 0;
    //static int pos[] = {1, 5, 4, 6, 2, 10, 8, 9};
    int height = QRandomGenerator::global()->bounded(20, winHeight - 20);
    libvlc_video_set_logo_int(m_player, libvlc_logo_y, height); // logo的 Y 坐标。
    int width = QRandomGenerator::global()->bounded(20, winWidth - 20);
    libvlc_video_set_logo_int(m_player, libvlc_logo_x, width); //logo的 X 坐标。
    libvlc_video_set_logo_int(m_player, libvlc_logo_opacity, (alpha++) % 80 + 20); //透明度
    //libvlc_video_set_logo_int(m_player, libvlc_logo_position, pos[alpha % 8]);
}

void vlchelper::update_text()
{ //更新文字显示：浮动文字显示会在屏幕上乱窜，每换一次位置就是一次更新
    static int alpha = 0;
    if(m_player) //保证在播放器存在的前提下进行
    {
        int color = QRandomGenerator::global()->bounded(0x30, 0x60);//R
        color = color << 8 | QRandomGenerator::global()->bounded(0x30, 0x60);//G
        color = color << 8 | QRandomGenerator::global()->bounded(0x30, 0x60);//B
        libvlc_video_set_marquee_int(m_player, libvlc_marquee_Color, color);//颜色,在一定范围内随机
        int x = QRandomGenerator::global()->bounded(20, winHeight - 20);//随机位置
        int y = QRandomGenerator::global()->bounded(20, winWidth - 20);
        libvlc_video_set_marquee_int(m_player, libvlc_marquee_X, x); //负责括着水印的框框的位置，保持和水印一样
        libvlc_video_set_marquee_int(m_player, libvlc_marquee_Y, y);
        libvlc_video_set_marquee_int(m_player, libvlc_marquee_Opacity, (alpha++ % 60) + 10); //随机透明度
    }
}

bool vlchelper::is_text_enable()
{
    if(m_player == NULL)
    {
        return false;
    }
    return libvlc_video_get_marquee_int(m_player, libvlc_marquee_Enable) == 1; //决定水印能不能点
}

bool vlchelper::has_media_player()
{
    return m_player != NULL && (m_media != NULL); //播放器是否健在，媒体源是否健在
}

void vlchelper::set_float_text()
{ //这个是第一次设置，先有第一次显示，然后每次更新，后面水印乱窜用的是update_text
    if(m_player)
    { //保证在播放器存在的前提下进行
        libvlc_video_set_marquee_string(m_player, libvlc_marquee_Text, text.toStdString().c_str()); //设置水印要显示的文字
        libvlc_video_set_marquee_int(m_player, libvlc_marquee_Color, 0x404040); //颜色
        libvlc_video_set_marquee_int(m_player, libvlc_marquee_X, 0); //位置
        libvlc_video_set_marquee_int(m_player, libvlc_marquee_Y, 0);
        libvlc_video_set_marquee_int(m_player, libvlc_marquee_Opacity, 100); //不透明度
        //libvlc_video_set_marquee_int(m_player, libvlc_marquee_Timeout, 100);
        libvlc_video_set_marquee_int(m_player, libvlc_marquee_Position, 5); //框框位置
        libvlc_video_set_marquee_int(m_player, libvlc_marquee_Size, 14); //大小
        libvlc_video_set_marquee_int(m_player, libvlc_marquee_Enable, 1); //水印是否能点
    }
}

bool vlchelper::is_logo_enable()
{
    if(m_player == NULL)
    {
        return false;
    }
    qDebug() << __FUNCTION__ << " logo enable:" << libvlc_video_get_logo_int(m_player, libvlc_logo_enable);
    return libvlc_video_get_logo_int(m_player, libvlc_logo_enable) == 1; //决定logo能不能点
}

//隶属于vlcmedia的范畴 media是播放的媒体源，但是要注意vlc控制的主体是播放器player
vlcmedia::vlcmedia(libvlc_instance_t* ins)
    : instance(ins)
{
    media = NULL;
    media_instance = new MediaMP4(); //实例化一个MediaMP4的对象,也就是要播放的源文件是MP4
}

vlcmedia::~vlcmedia()
{ //在析构函数中所有new出来的对象都要析构掉
    if(media)
    {
        free();
    }
    if(media_instance)
    {
        delete media_instance;
        media_instance = NULL;
    }
}

int vlcmedia::open(void* thiz, void** infile, uint64_t* fsize)
{ //打开
    vlcmedia* _this = (vlcmedia*)thiz;
    return _this->open(infile, fsize);
}

ssize_t vlcmedia::read(void* thiz, uint8_t* buffer, size_t length)
{ //读取
    vlcmedia* _this = (vlcmedia*)thiz;
    return _this->read(buffer, length);
}

int vlcmedia::seek(void* thiz, uint64_t offset)
{ //定位
    vlcmedia* _this = (vlcmedia*)thiz;
    return _this->seek(offset);
}

void vlcmedia::close(void* thiz)
{ //关闭
    vlcmedia* _this = (vlcmedia*)thiz;
    _this->close();
}

vlcmedia& vlcmedia::operator=(const QString& str)
{
    if(media)
    {
        free();
    }
    //libvlc_media_read_cb
    strPath = str;
    media = libvlc_media_new_callbacks(
                instance,
                &vlcmedia::open,
                &vlcmedia::read,
                &vlcmedia::seek,
                &vlcmedia::close,
                this);
    return *this;
}

void vlcmedia::free()
{
    if(media != NULL)
    {
        libvlc_media_release(media); //free释放直接调用libvlc的release函数
    }
}

QString vlcmedia::path()
{
    return strPath; //媒体源的路径
}

int vlcmedia::open(void** infile, uint64_t* fsize)
{
    //"file:///"
    if(media_instance) //media_instance: 一个实例化的MediaMP4对象
    {
        *infile = this;
        int ret = media_instance->open(strPath, fsize); //打开文件
        media_size = *fsize; //保存文件大小
        return ret;
    }
    this->infile.open(strPath.toStdString().c_str() + 8, ios::binary | ios::in);  //以二进制的形式打开文件(视频或者说媒体文件都是二进制保存的)
    this->infile.seekg(0, ios::end);  //直接定位到最后, 没法直接获取大小的时候就直接定位到末尾，末尾减开头就是大小
    *fsize = (uint64_t)this->infile.tellg(); //获取从头到尾的大小
    media_size = *fsize; //保存文件大小
    this->infile.seekg(0);
    *infile = this;
    return 0;
}

ssize_t vlcmedia::read(uint8_t* buffer, size_t length)
{
    if(media_instance)
    { //MediaMP4的对象存在，就调用它自己的read函数去读取
        return media_instance->read(buffer, length);
    }
    //qDebug() << __FUNCTION__ << " length:" << length;
    uint64_t pos = (uint64_t)infile.tellg(); //当前的位置
    //qDebug() << __FUNCTION__ << " positon:" << pos;
    if(media_size - pos < length) //大小减去当前位置小于长度，就是现在的位置不在一开始
    {
        length = media_size - pos; //长度更新，更新成大小减去当前位置，也就是从当前位置到末尾为新的长度
    }
    infile.read((char*)buffer, length); //读取到buffer里
    return infile.gcount();
}

int vlcmedia::seek(uint64_t offset)                                     //:关于这里 是vlcmedia还是MediaMP4的问题，播放时选择的是哪一个
{                                                                       //用的是MediaMP4
    if(media_instance)
    { //如果是MediaMP4对象，就调用它自己的定位seek函数
        return media_instance->seek(offset);
    }
    //qDebug() << __FUNCTION__ << ":" << offset;
    infile.clear();         //先清空
    infile.seekg(offset);   //再定位
    return 0;
}

void vlcmedia::close()
{
    if(media_instance)
    {
        return media_instance->close();
    }
    //qDebug() << __FUNCTION__;    //不管哪个都是直接调用close函数即可，具体操作交给close去做
    infile.close();
}

vlcmedia::operator libvlc_media_t* ()
{
    return media;
}
