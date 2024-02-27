#include "recordfile.h"
#include <QFile>
#include <QDebug>

RecordFile::RecordFile(const QString& path)
{
    QFile file(path);   //构造的时候要确定文件路径，并且要确保文件存在
    m_path = path;
    qDebug() << m_path;
    do
    {
        if(!file.open(QIODevice::ReadOnly))  //以只读方式打开
        {
            break;
        }
        QByteArray data = file.readAll();  //Json，读取的时候以字节数组的方式读取
        if(data.size() <= 0)    //空的情况
        {
            break;
        }
        data = tool.rsaDecode(data);    //ssl解码
        //qDebug() << __FILE__ << "(" << __LINE__ << "):" << data;
        int i = 0;
        for(; i < data.size(); i++)
        {                     //127                       //10
            if((int)data[i] >= (int)0x7F || (int)data[i] < (int)0x0A)
            {
                data.resize(i);
                break;
            }
        }
        //qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):" << data;
        //qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):" << data.size();
        QJsonParseError json_error;     //万一出错的情况下记录错误
        QJsonDocument doucment = QJsonDocument::fromJson(data, &json_error); //读取JSON文件内容
        if (json_error.error == QJsonParseError::NoError)   //无错误的情况下
        {
            if (doucment.isObject())  //JSON存的是个对象
            {
                m_config = doucment.object();  //将对象交给config对象
                //qDebug() << __FILE__ << "(" << __LINE__ << "):" << m_config;
                return;//成功了，直接返回
            }
        }
        else
        {
            qDebug().nospace() << __FILE__ << "(" << __LINE__ << "):" << json_error.errorString() << json_error.error;
        }
    } while(false);

    //读取失败，则设置默认值
    file.close();
    QJsonValue value = QJsonValue();            //JSON内容包括:用户名，密码，是否自动登录，是否记住密码，更新时间
    m_config.insert("user", value);
    m_config.insert("password", value);
    m_config.insert("auto", false);//自动登录
    m_config.insert("remember", false);//记住密码
    m_config.insert("date", "1970-01-01 10:10:10");//updateTime
    //qDebug() << __FILE__ << "(" << __LINE__ << "):" << m_config;
    return;
}

RecordFile::~RecordFile()
{
    //qDebug() << __FILE__ << "(" << __LINE__ << "):";
    save();     //析构的时候要触发保存函数（保存里面有close），因为RecordFile是一个单独的类，用的时候是一个实例化对象，用完就没了，
}               //所以在析构的时候一定要保存，确保之前已经写过的东西能够正常保存成功

QJsonObject& RecordFile::config()
{
    return m_config;  //在外部类需要用到.dat文件(想要写入的)的时候需要能拿到config文件
}

bool RecordFile::save()
{
    QFile file(m_path);         //以重写的方式打开，新的会覆盖掉旧的
    if(file.open(QIODevice::WriteOnly | QIODevice::Truncate) == false)
    {
        //qDebug() << __FILE__ << "(" << __LINE__ << "):";
        return false;
    }
    //qDebug() << __FILE__ << "(" << __LINE__ << "):";
    QJsonDocument doucment = QJsonDocument(m_config);       //按照JSON库的要求，所有写入的东西（如写入对象）都要通过document对象写入
    //qDebug() << __FILE__ << "(" << __LINE__ << "):";
    file.write(tool.rsaEncode(doucment.toJson()));          //加密，ssl支持对json数据进行加密
    //qDebug() << __FILE__ << "(" << __LINE__ << "):";
    file.close();                                           //写完记得关闭文件
    //qDebug() << __FILE__ << "(" << __LINE__ << "):" << m_config;
    return true;
}
