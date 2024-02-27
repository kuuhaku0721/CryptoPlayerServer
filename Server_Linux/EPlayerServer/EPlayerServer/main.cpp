#include <cstdio>
#include "PlayerServer.h"
#include "HttpParser.h"
#include "Sqlite3Client.h"
#include "MysqlClient.h"
#include "Crypto.h"

int CreateLogServer(CProcess* proc)
{ //日志服务器
    //printf("%s(%d):<%s> pid = %d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
    CLoggerServer server;
    int ret = server.Start();
    if (ret != 0)
    {
        printf("%s(%d):<%s> pid = %d errno: %d msg: %s ret = %d\n", 
            __FILE__, __LINE__, __FUNCTION__, getpid(), errno, strerror(errno), ret);
    }
    int fd = 0;
	while (true)
	{
		ret = proc->RecvFD(fd);
        printf("%s(%d):<%s> fd = %d\n", __FILE__, __LINE__, __FUNCTION__, fd);
		if (fd <= 0) break;
	}
    ret = server.Close();
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    return 0;
}

int CreateClientServer(CProcess* proc)
{ //客户端服务器
    //printf("%s(%d):<%s> pid = %d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
    int fd = -1;
    int ret = proc->RecvFD(fd);
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    printf("%s(%d):<%s> fd = %d\n", __FILE__, __LINE__, __FUNCTION__, fd);
    sleep(1);
    //如果接收成功是可以读的
    char buf[10] = "";
    lseek(fd, 0, SEEK_SET);   //打开的每一个文件都有一个文件偏移量，用以度量从文件开始处计算的字节数，通常读写都从文件偏移量处开始
        //lseek函数就是设置偏移量的 参数：文件描述符 偏移量(正数向后负数向前)第三个：
        //SET：文件指针偏移到传入字节处 文件头  CUR：当前位置 END:文件末尾，作为拓展作用需再执行一次写操作
    read(fd, buf, sizeof(buf));
    printf("%s(%d):<%s> buf = %s\n", __FILE__, __LINE__, __FUNCTION__, buf);
    close(fd);

    return 0;
}

int Main()
{
    int ret = 0;
    CProcess proclog;
    ret = proclog.SetEntryFunction(CreateLogServer, &proclog);
    ERR_RETURN(ret, -1);
    ret = proclog.CreateSubProcess();
    ERR_RETURN(ret, -2);
    CPlayerServer business(2);
    CServer server;
    ret = server.Init(&business);
    ERR_RETURN(ret, -3);
    ret = server.Run();
    ERR_RETURN(ret, -4);
    return 0;
}

DECLARE_TABLE_CLASS(user_test, _sqlite3_table_)
DECLARE_FIELD(TYPE_INT, user_id, NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "INTEGER", "", "", "")
DECLARE_FIELD(TYPE_VARCHAR, user_qq, NOT_NULL, "VARCHAR", "(15)", "2574189654", "")
DECLARE_TABLE_CLASS_END()

//class user_test : public _sqlite3_table_
//{
//public:
//    virtual PTable Copy() const
//    {
//        return PTable(new user_test(*this));
//    }
//    user_test() : _sqlite3_table_()
//    {
//        Name = "user_test";
//        { //第一列
//            PField field(new _sqlite3_field_(TYPE_INT, "user_id", NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "INT", "", "", ""));
//            FieldDefine.push_back(field);
//            Fields["user_id"] = field;
//        }
//        { //第二列
//            PField field(new _sqlite3_field_(TYPE_VARCHAR, "user_qq", NOT_NULL, "VARCHAR", "(15)", "", ""));
//            FieldDefine.push_back(field);
//            Fields["user_qq"] = field;
//        }
//    }
//};
int sqlTest()
{
    user_test test, value;
    printf("create: %s\n", (char*)test.Create());  
    printf("delete: %s\n", (char*)test.Delete(test));
    value.Fields["user_qq"]->LoadFromStr("9657851472");
    value.Fields["user_qq"]->Condition = SQL_INSERT;
    printf("insert: %s\n", (char*)test.Insert(value));
    value.Fields["user_qq"]->LoadFromStr("1452873618");
    value.Fields["user_qq"]->Condition = SQL_MODIFY;
    printf("modify: %s\n", (char*)test.Modify(value));
    printf("query: %s\n", (char*)test.Query());
    printf("drop: %s\n", (char*)test.Drop());

    getchar();
    CDataBaseClient* pClient = new CSqlite3Client();
    KeyValue args;
    args["host"] = "test.db";
    int ret = 0;
    ret = pClient->Connect(args);
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ret = pClient->Exec(test.Create());
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ret = pClient->Exec(test.Delete(value));
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    value.Fields["user_qq"]->LoadFromStr("9657851472");
    value.Fields["user_qq"]->Condition = SQL_INSERT;
    ret = pClient->Exec(test.Insert(value));
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    value.Fields["user_qq"]->LoadFromStr("1452873618");
    value.Fields["user_qq"]->Condition = SQL_MODIFY;
    ret = pClient->Exec(test.Modify(value));
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    Result result;
    ret = pClient->Exec(test.Query(), result, test);
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ret = pClient->Exec(test.Drop());
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ret = pClient->Close();
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);

    return 0;
}

DECLARE_TABLE_CLASS(user_test_mysql, _mysql_table_)
DECLARE_MYSQL_FIELD(TYPE_INT, user_id, NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "INTEGER", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_VARCHAR, user_qq, NOT_NULL, "VARCHAR", "(15)", "2574189654", "")
DECLARE_TABLE_CLASS_END()
int mysql_test()
{
    user_test_mysql test, value;
    printf("create: %s\n", (char*)test.Create());
    printf("delete: %s\n", (char*)test.Delete(test));
    value.Fields["user_qq"]->LoadFromStr("9657851472");
    value.Fields["user_qq"]->Condition = SQL_INSERT;
    printf("insert: %s\n", (char*)test.Insert(value));
    value.Fields["user_qq"]->LoadFromStr("1452873618");
    value.Fields["user_qq"]->Condition = SQL_MODIFY;
    printf("modify: %s\n", (char*)test.Modify(value));
    printf("query: %s\n", (char*)test.Query());
    printf("drop: %s\n", (char*)test.Drop());

    getchar();
    CDataBaseClient* pClient = new CMysqlClient();
    KeyValue args;
    args["host"] = "192.168.65.130";
    args["user"] = "kuuhaku";
    args["password"] = "002016";
    args["port"] = 3306;
    args["db"] = "eplayer";
    int ret = 0;
    ret = pClient->Connect(args);
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ret = pClient->Exec(test.Create());
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ret = pClient->Exec(test.Delete(value));
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    value.Fields["user_qq"]->LoadFromStr("9657851472");
    value.Fields["user_qq"]->Condition = SQL_INSERT;
    ret = pClient->Exec(test.Insert(value));
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    value.Fields["user_qq"]->LoadFromStr("1452873618");
    value.Fields["user_qq"]->Condition = SQL_MODIFY;
    ret = pClient->Exec(test.Modify(value));
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    Result result;
    ret = pClient->Exec(test.Query(), result, test);
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ret = pClient->Exec(test.Drop());
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ret = pClient->Close();
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);

    return 0;
}

int httpTest()
{
    //正确的示例
    Buffer str = "GET /favicon.ico HTTP/1.1\r\n"
        "Host: 0.0.0.0=5000\r\n"
        "User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9) Gecko/2008061015 Firefox/3.0\r\n"
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*; q = 0.8\r\n"
        "Accept-Language: en-us,en;q=0.5\r\n"
        "Accept-Encoding: gzip,deflate\r\n"
        "Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7\r\n"
        "Keep-Alive: 300\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";             //共368字节
    CHttpParser parser;
    size_t size = parser.Parser(str);
    if (parser.Errno() != 0)
    {
        printf("errno %d\n", parser.Errno());
        return -1;
    }
    if (size != 368)
    {
        printf("size error:%lld  str.size: %lld\n", size, str.size());
        return -2;
    }
    printf("method: %d url: %s\n", parser.Method(), (char*)parser.Url());

    //错误的示例
    str = "GET /favicon.ico HTTP/1.1\r\n"
        "Host: 0.0.0.0=5000\r\n"
        "User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9) Gecko/2008061015 Firefox/3.0\r\n"
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
    size = parser.Parser(str);
    printf("errno: %d size: %lld\n", parser.Errno(), size);
    if (parser.Errno() != 0x7F)     //0x7F char类型的-1
        return -3;
    if (size != 0)
        return -4;

    UrlParser url1("https://www.baidu.com/s?ie=utf8&oe=utf8&wd=httplib&tn=98010089_dg&ch=3");
    int ret = url1.Parser();
    if (ret != 0)
    {
        printf("urlparser1 failed: %d\n", ret);
        return -5;
    }
    printf("ie = %s except:utf8\n", (char*)url1["ie"]);
    printf("oe = %s except:utf8\n", (char*)url1["oe"]);
    printf("wd = %s except:httplib\n", (char*)url1["wd"]);
    printf("tn = %s except:98010089_dg\n", (char*)url1["tn"]);
    printf("ch = %s except:3\n", (char*)url1["ch"]);

    UrlParser url2("http://127.0.0.1:19811/?time=144000&salt=9527&user=test&sign=1234567890abcdef");
    ret = url2.Parser();
    if (ret != 0)
    {
        printf("urlparser2 failed: %d\n", ret);
        return -6;
    }
    printf("time = %s except:144000\n", (char*)url2["time"]);
    printf("salt = %s except:9527\n", (char*)url2["salt"]);
    printf("user = %s except:test\n", (char*)url2["user"]);
    printf("sign = %s except:1234567890abcdef\n", (char*)url2["sign"]);
    printf("host: %s port: %d\n", (char*)url2.Host(), url2.Port());

    return 0;
}

int LogTest()
{
    char buffer[] = "hello kuuhaku! 琪亚娜";
    usleep(100 * 1000); //100ms
    //写入第一条 [INFO]
    TRACEI("here is log %d %c %f %g %s 这里是中文测试", 10, 'A', 1.0f, 2.0, buffer);
    //写入第二条 [DEBUG]
    DUMPD((void*)buffer, (size_t)sizeof(buffer));
    //写入第三条 [ERROR]
    LOGE << 100 << " " << 'B' << " " << 0.1234f << " " << 1.2345676 << " " << buffer << "日志测试";

    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, 0);
    return 0;
}

int playServerTest()
{
    int ret = 0;
    CProcess procLog;
    ret = procLog.SetEntryFunction(CreateLogServer, &procLog);
    ERR_RETURN(ret, -1);
    ret = procLog.CreateSubProcess();
    ERR_RETURN(ret, -2);
    CPlayerServer business(2);
    CServer server;
    ret = server.Init(&business);
    ERR_RETURN(ret, -3);
    ret = server.Run();
    ERR_RETURN(ret, -4);

    return 0;
}

int oldTest()
{
    printf("%s 向你问好!\n", "EPlayerServer");
    //调用这个函数转到守护进程
    //CProcess::SwitchDeamon(); 

    CProcess procLog, procClients; //CProcess，用类封装入口函数
    //printf("%s(%d):<%s> pid = %d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
    procLog.SetEntryFunction(CreateLogServer, &procLog); //设置两个server的入口函数
    int ret = procLog.CreateSubProcess(); //创建子进程
    if (ret != 0)
    {
        return -1;
    }

    LogTest();      //测试日志
    printf("%s(%d):<%s> pid = %d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
    //线程测试
    CThread thread(LogTest);
    thread.Start();


    //printf("%s(%d):<%s> pid = %d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
    procClients.SetEntryFunction(CreateClientServer, &procClients);
    ret = procClients.CreateSubProcess();
    if (ret != 0)
    {
        printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
        return -2;
    }

    //测试文件描述符传递准备一个文件
    int fd = open("./test.txt", O_RDWR | O_CREAT | O_APPEND); //读写 不存在创建 追加方式写入
    printf("%s(%d):<%s> fd = %d\n", __FILE__, __LINE__, __FUNCTION__, fd);
    if (fd == -1) return -3;
    ret = procClients.SendFD(fd);
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    if (ret != 0) printf("%s(%d):<%s> error:%d-->msg: %s\n", __FILE__, __LINE__, __FUNCTION__, errno, strerror(errno));
    write(fd, "kuuhaku", 7);
    close(fd);

    CThreadPool pool;
    ret = pool.Start(4);
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ret = pool.AddTask(LogTest);
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ret = pool.AddTask(LogTest);
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ret = pool.AddTask(LogTest);
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ret = pool.AddTask(LogTest);
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    pool.Close();

    (void)getchar();
    printf("这个错误不用管, 错的地方是send了一个-1，没有用到\n");
    procLog.SendFD(-1);

    return 0;
}

int md5_test()
{
    Buffer data = "abcdef";
    data = Crypto::MD5(data);
    printf("expect= E80B5017098950FC58AAD83C8C14978E\n data = %s\n", (char*)data);
    return 0;
}

int main()
{
    int ret = 0;
    //oldTest();
    //playServerTest();
    //LogTest();
    //ret = httpTest();
    //ret = sqlTest();
    //ret = mysql_test();
    //ret = md5_test();
    ret = Main();
    printf("ret = %d\n", ret);

    return 0;
}