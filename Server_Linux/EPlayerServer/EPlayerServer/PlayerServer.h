#pragma once
#include "CServer.h"
#include "Logger.h"
#include "HttpParser.h"
#include "Crypto.h"
#include "MysqlClient.h"
#include "jsoncpp/json.h"
#include <map>

DECLARE_TABLE_CLASS(Login_user_mysql, _mysql_table_)
DECLARE_MYSQL_FIELD(TYPE_INT, user_id, NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "INTEGER", "", "", "") //id
DECLARE_MYSQL_FIELD(TYPE_VARCHAR, user_qq, NOT_NULL, "VARCHAR", "(15)", "", "")		//QQ号
DECLARE_MYSQL_FIELD(TYPE_VARCHAR, user_phone, DEFAULT, "VARCHAR", "(11)", "NULL", "")//电话
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_name, NOT_NULL, "TEXT", "", "", "")				//用户名
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_nick, NOT_NULL, "TEXT", "", "", "")				//昵称
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_wechat, DEFAULT, "TEXT", "", "NULL", "")		//微信
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_wechat_id, DEFAULT, "TEXT", "", "NULL", "")		//微信号
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_address, DEFAULT, "TEXT", "", "", "")			//住址
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_province, DEFAULT, "TEXT", "", "", "")			//省份
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_country, DEFAULT, "TEXT", "", "", "")			//国家
DECLARE_MYSQL_FIELD(TYPE_INT, user_age, DEFAULT | CHECK, "INTEGER", "", "18", "")	//年龄
DECLARE_MYSQL_FIELD(TYPE_INT, user_male, DEFAULT, "BOOL", "", "1", "")				//性别
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_flags, DEFAULT, "TEXT", "", "0", "")			//标志
DECLARE_MYSQL_FIELD(TYPE_REAL, user_experience, DEFAULT, "REAL", "", "0.0", "")		//经验值
DECLARE_MYSQL_FIELD(TYPE_INT, user_level, DEFAULT | CHECK, "INTEGER", "", "0", "")	//课程等级
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_class_priority, DEFAULT, "TEXT", "", "", "")	//课程优先级
DECLARE_MYSQL_FIELD(TYPE_REAL, user_time_per_viewer, DEFAULT, "REAL", "", "", "")   //观看时长
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_career, NONE, "TEXT", "", "", "")				//职业
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_password, NOT_NULL, "TEXT", "", "", "")			//密码
DECLARE_MYSQL_FIELD(TYPE_INT, user_birthday, NONE, "DATETIME", "", "", "")			//生日
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_describe, NONE, "TEXT", "", "", "")				//描述信息
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_education, NONE, "TEXT", "", "", "")			//学历
//DECLARE_MYSQL_FIELD(TYPE_TEXT, user_register_time, DEFAULT, "DATETIME", "", "LOCALTIME()", "")
DECLARE_TABLE_CLASS_END()

//有一说一，宏的方式写代码确实很漂亮，但是能不能读的顺，那是另一回事
#define ERR_RETURN(ret, err) if(ret != 0) { TRACEE("ret = %d errno = %d msg = [%s]", ret, errno, strerror(errno)); return err; }

#define WARN_CONTINUE(ret) if(ret != 0) { TRACEW("ret = %d errno = %d msg = [%s]", ret, errno, strerror(errno)); continue; }

class CPlayerServer : public CBusiness
{
public:
	CPlayerServer(unsigned count) : CBusiness() {
		m_count = count; //count用于控制epoll和线程池的数量，参数形式传递进来，后面方便修改
	}
	~CPlayerServer()		//析构函数需要把之前用过的都释放掉，而用掉的地方有很多跨文件跨类的，所以都需要一一去关闭释放
	{						//需要关闭的都是在业务逻辑中用到的，而业务逻辑用到的都在成员变量有声明，所以针对成员变量一个个释放
		m_epoll.Close();	//不同的成员变量有不同的释放方式，需要额外注意，比如下面的map
		m_pool.Close();
		if (m_db)
		{
			CDataBaseClient* db = m_db;
			m_db = NULL;
			db->Close();
			delete db;
		}
		for (auto it : m_mapClients)
		{
			if (it.second)		//second封装的是CSocketBase套接字，并且这个SocketBase还是new出来的，需要手动delete
			{
				delete it.second;
			}
		}
		m_mapClients.clear();	//最后清空一下map集合，清理残留
	}

	virtual int BusinessProcess(CProcess* proc)		//业务处理进程，处理业务用的，而具体是哪个进程需要处理业务，由传入的参数决定
	{
		using namespace std::placeholders;
		int ret = 0;
		//连接数据库
		m_db = new CMysqlClient(); //创建数据库对象
		if (m_db == NULL)
		{
			TRACEE("no more memory");
			return -1;
		}
		KeyValue args;		//数据库信息
		args["host"] = "192.168.65.130";
		args["user"] = "kuuhaku";
		args["password"] = "002016";
		args["port"] = 3306;
		args["db"] = "eplayer";
		ret = m_db->Connect(args); //连接数据库
		ERR_RETURN(ret, -2);
		Login_user_mysql user;
		ret = m_db->Exec(user.Create()); //表不存在就创建，存在直接返回
		ERR_RETURN(ret, -3);

		//示例: setConnectedCallback(&CPlayerServer::Connected, this, std::placeholders::_1);
		ret = setConnectedCallback(&CPlayerServer::Connected, this, _1);  //占位符，现在不知道参数是啥，只知道有一个参数，所以放一个占位符
		ERR_RETURN(ret, -4);											  //等到下面知道参数的时候再去传入参数
		ret = setRecvCallback(&CPlayerServer::Received, this, _1, _2);	  //setCallback来自business父类，在父类封装的函数，有不同的函数体
		ERR_RETURN(ret, -5);
										// 1、业务层一定有的: epoll的创建 线程池的创建，有业务处理就需要线程池，有线程池就需要有客户端接入，有客户端就要有epoll监督
		ret = m_epoll.Create(m_count); //测试阶段放两个看一下效果
		ERR_RETURN(ret, -6);
		ret = m_pool.Start(m_count);	// 2、因为在它们创建的时候都要提前指定数量，所以参数传入创建时的数量
		ERR_RETURN(ret, -7);

		for (unsigned i = 0; i < m_count; i++)
		{
			ret = m_pool.AddTask(&CPlayerServer::ThreadFunc, this);  // 3、线程池创建完毕就是给线程池添加任务，逻辑看线程池部分
			ERR_RETURN(ret, -8);
		}

		int sock = 0;
		sockaddr_in addrin;
		while (m_epoll != -1)						// 4、线程池添加完任务就该epoll添加监管对象
		{											//    而epoll接管监督对象需要先拿到客户端，所以需要先接收客户端，有客户端之后就是添加epoll监管
			ret = proc->RecvSock(sock, &addrin);	// 4.1 接收消息拿到客户端套接字fd
			TRACEI("RecvSock ret=%d", ret);
			if((ret < 0) || (sock == 0)) break;
			CSocketBase* pClient = new CSocket(sock);	// 4.2 new一个socket对象并创建客户端
			if (pClient == NULL) continue;
			ret = pClient->Init(CSockParam(&addrin, SOCK_ISIP));	//4.3 客户端初始化
			WARN_CONTINUE(ret);
			ret = m_epoll.Add(sock, EpollData((void*)pClient));		//4.4 加入到epoll的监管之下
			if (m_connectedCallback)	//4.5 新增的 连接回调测试
			{
				(*m_connectedCallback)(pClient);				/* !!!! 上述1234是惯用逻辑 */
			}
			WARN_CONTINUE(ret);
		}
		return 0;
	}

private:
	int Connected(CSocketBase* pClient)
	{
		//TODO:客户端连接处理,简单打印客户端信息
		sockaddr_in* paddr = *pClient;
		TRACEI("client connected addr %s port:%d", inet_ntoa(paddr->sin_addr), paddr->sin_port);
		return 0;
	}

	int Received(CSocketBase* pClient, const Buffer& data)
	{
		TRACEI("接收到数据");
		//TODO:主要业务处理
		//HTTP解析
		int ret = 0;
		Buffer response = ""; //应答消息
		ret = HttpParser(data);
		TRACEI("HttpParser ret = %d", ret);
		//验证结果的反馈 
		if (ret != 0)  //验证失败
		{
			TRACEE("http parser failed! %d", ret);
		}
		//成功或者失败的应答 取决于ret
		response = MakeResponse(ret);

		ret = pClient->Send(response);
		if (ret != 0)
			TRACEE("http response failed! %d [%s]", ret, (char*)response);
		else
			TRACEI("http response success! %d", ret);
		
		return 0;
	}
	//HTTP解析
	int HttpParser(const Buffer& data)
	{
		int ret = 0;
		CHttpParser parser;
		size_t size = parser.Parser(data);
		if ((size == 0) || (parser.Errno() != 0))
		{
			TRACEE("size %llu errno:%u", size, parser.Errno());
			return -1;
		}
		if (parser.Method() == HTTP_GET)
		{
			//GET 请求处理
			UrlParser url("https://192.168.65.130" + parser.Url());
			ret = url.Parser();
			if (ret != 0)
			{
				TRACEE("ret = %d url:[%s]", ret, "https://192.168.65.130" + parser.Url());
				return -2;
			}
			Buffer uri = url.Uri();
			TRACEI("**** uri = %s", (char*)uri);
			if (uri == "login")
			{
				//处理登录
				Buffer time = url["time"];
				Buffer salt = url["salt"];
				Buffer user = url["user"];
				Buffer sign = url["sign"];
				TRACEI("time %s salt %s user %s sign %s", (char*)time, (char*)salt, (char*)user, (char*)sign);

				//数据库的查询
				Login_user_mysql dbuser;
				Result result;
				Buffer sql = dbuser.Query("user_name=\"" + user + "\"");
				ret = m_db->Exec(sql, result, dbuser);
				if (ret != 0)
				{
					TRACEE("sql = %s ret = %d", (char*)sql, ret);
					return -3;
				}
				if (result.size() == 0)
				{
					TRACEE("no result sql=%s ret=%d", (char*)sql, ret);
					return -4;
				}
				if (result.size() != 1)
				{
					TRACEE("more than one acount sql=%s ret=%d", (char*)sql, ret);
					return -5;
				}
				auto user1 = result.front(); //结果是PTable
				Buffer pwd = *user1->Fields["user_password"]->Value.String;
				TRACEI("password = %s", pwd);

				//登录请求的验证		   
				const char* MD5_KEY = "*&^%$#@b.v+h-b*g/h@n!h#n$d^ssx,.kl<kl";  //客户端与服务端统一
				Buffer md5str = time + MD5_KEY + pwd + salt;
				Buffer md5 = Crypto::MD5(md5str);
				TRACEI("md5 = %s", (char*)md5);
				if (md5 == sign)
				{
					return 0; //验证成功
				}
				return -6; //验证失败
			}
		}
		else if (parser.Method() == HTTP_POST)
		{
			//POST 请求处理
		}

		return 0;
	}

	Buffer MakeResponse(int ret)
	{ //应答函数
		Json::Value root;
		root["status"] = ret;
		if (ret != 0)
		{
			root["message"] = "登陆失败: 用户名或密码错误!";
		}
		else
		{
			root["message"] = "success";
		}
		Buffer json = root.toStyledString();
		//应答消息准备完毕，可以拼接
		Buffer result = "HTTP/1.1 200 OK\r\n";
		time_t t;
		time(&t); //当前服务器时间
		tm* ptm = localtime(&t);
		char temp[64] = "";
		strftime(temp, sizeof(temp), "%a, %d %b %G %T GMT\r\n", ptm);
		Buffer Date = Buffer("Date: ") + temp;
		Buffer Server = "Server: EPlayer/1.0\r\nContent-Type: text/html; charset=utf-8\r\nX-Frame-Options: DENY\r\n";
		snprintf(temp, sizeof(temp), "%d", json.size());
		Buffer Length = Buffer("Content-Length: ") + temp + "\r\n";
		Buffer Stub = "X-Content-Type-Options: nosniff\r\nReferrer-Policy: same-origin\r\n\r\n";
		//最终结果
		result += Date + Server + Length + Stub + json;
		TRACEI("response: %s", (char*)result);

		return result;
	}

private:
	int ThreadFunc()					//方才第三步线程池添加任务的任务处理函数
	{
		int ret = 0;
		EPEvents events;		//epoll事件集，利用wait来得到当前等待处理的事件，然后一个个处理			/* !!!! 这个也是惯用逻辑  */
		while (m_epoll != -1)
		{
			ssize_t size = m_epoll.WaitEvents(events);		// 1、循环体内先拿到wait事件的总数，然后去循环，一个个处理
			if (size < 0) break;
			if (size > 0)
			{
				for (ssize_t i = 0; i < size; i++)
				{
					if (events[i].events & EPOLLERR)	
					{
						break;
					}									//2、循环体内根据EPOLL的状态ERR还是IN来决定处理逻辑（又是可以根据这个判断服务端or客户端）
					else if (events[i].events & EPOLLIN)
					{			// 2、1 客户端有输入请求，说明需要收信息了
						CSocketBase* pClient = (CSocketBase*)events[i].data.ptr;	//2、2那就先从集合里面拿到当前的客户端
						if (pClient) //拿到成功的意思
						{
							Buffer data;				//2、3准备一个缓冲区接收消息
							ret = pClient->Recv(data);	//2、4调用客户端的接收消息的接口正式接收消息
							TRACEI("recv data size %d", ret);
							if (ret <= 0) 
							{ 
								TRACEW("ret = %d errno = %d msg = [%s]", ret, errno, strerror(errno)); 
								m_epoll.Del(*pClient);
								continue; 
							}
							if (m_recvCallback)			//2、5 新增的，一个接收成功的回调函数
							{
								(*m_recvCallback)(pClient, data);
							}
						}
						
					}
				}
			}
		}

		return 0;
	}

private:
	CEpoll m_epoll;
	CThreadPool m_pool;
	std::map<int, CSocketBase*> m_mapClients;
	unsigned m_count;
	CDataBaseClient* m_db;
};