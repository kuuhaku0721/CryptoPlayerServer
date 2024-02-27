#pragma once
#include "Thread.h"
#include "Epoll.h"
#include "Socket.h"
#include <map>
#include <sys/timeb.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <sstream>


enum LogLevel		// 枚举，用来标志日志类型：信息，调试，警告，错误，严重错误
{
	LOG_INFO,
	LOG_DEBUG,
	LOG_WARNING,
	LOG_ERROR,
	LOG_FATAL
};

class LogInfo
{
public:			// 不同类型构造函数，分别给不同类型的日志消息用，前面都是公共头部分，不同的在参数列表最后面几个
	LogInfo(const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int level, const char* fmt, ...);
	
	LogInfo(const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int level);
	
	LogInfo(const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int level, void* pData, size_t nSize);
	
	~LogInfo();
	
	operator Buffer() const	//重载调用运算符()，方便拿到m_buf 这部分有Functor，Callable这些，侯捷有讲
	{
		return m_buf;
	}

	template<typename T>
	LogInfo& operator<<(const T& data)  // 重载流式传输运算符，用于手动输出日志信息，模板函数，以便能够传入各种信息
	{
		std::stringstream stream;		//string流，简单理解为，全部转换为字符串输出 到流
		stream << data;											//然后从流里写入m_buf或者打印输出
		printf("%s(%d):[%s] data = %s\n", __FILE__, __LINE__, __FUNCTION__, stream.str().c_str());
		m_buf += stream.str();
		return *this;	// return自己，方便连续使用 << 1 << 2 << 3 << 4
	}

private:
	bool bAuto; //自动发送，默认false，如果是流式日志( << )则为true
	Buffer m_buf;  //用来承载日志信息的缓冲区,没有简单使用string而是用封装的buffer类，buffer类里面有各种函数
};

class CLoggerServer
{	// 既然是server就有初始化，连接，关闭等等
public:
	CLoggerServer()
		:m_thread(&CLoggerServer::ThreadFunc, this)
	{
		m_server = NULL;

		char curPath[256] = "";		//日志服务器，用的本地套接字，连接的终端是本地文件，所以需要写入路径信息
		getcwd(curPath, sizeof(curPath));
		m_path = curPath;
		m_path += "/log/" + GetTimeStr() + ".log";	//绝对路径 + 当前时间 + .log = 日志文件
		printf("%s(%d):[%s]path = %s\n", __FILE__, __LINE__, __FUNCTION__, (char*)m_path);
	}
	~CLoggerServer() 
	{
		Close();
	}
	CLoggerServer(const CLoggerServer*) = delete;	//独一份，不允许拷贝赋值
	CLoggerServer& operator=(const CLoggerServer&) = delete;

public:
	int Start() 
	{
		if (m_server != NULL) return -1;
		if (access("log", W_OK | R_OK) != 0)  // access 进入某个路径，W 写许可 R 读许可 没有就是不存在，那就创建这个目录
		{ //			自己读		自己写		组读		组写		其他读
			mkdir("log", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
		}
		m_file = fopen(m_path, "w+");	//写入方式打开这个文件，+：不存在就创建
		if (m_file == NULL) return -2;
									// 文件准备好了，该监控的epoll和本地套接字了
		int ret = m_epoll.Create(1);	//调用epoll的create 创建epoll
		if (ret != 0) return -3;

		m_server = new CSocket();	//创建本地套接字，之后的bind listen accept 服务端的connect等这些在下面的init
		if (m_server == NULL)
		{
			Close();
			return -4;
		}
		CSockParam param("./log/server.sock", (int)SOCK_ISSERVER | SOCK_ISREUSE); //连参数都单独封装传入
		ret = m_server->Init(param);	//初始化日志服务器，的套接字连接
		if (ret != 0)
		{
			//printf("%s(%d):<%s> pid = %d errno: %d msg: %s ret = %d\n",
			//	__FILE__, __LINE__, __FUNCTION__, getpid(), errno, strerror(errno), ret);
			Close();
			return -5;
		}

		ret = m_epoll.Add(*m_server, EpollData((void*)m_server), EPOLLIN | EPOLLERR);	//将这个套接字加入到epoll监控下
		if (ret != 0)
		{
			Close();
			return -6;
		}

		ret = m_thread.Start();	// 开启线程写日志，日志用一个线程去处理，不影响主业务的进行,
		if (ret != 0)			// 最终是让线程去处理写日志这件事，传输用的localSocket，epoll监控这个socket
		{
			printf("%s(%d):<%s> pid = %d errno: %d msg: %s ret = %d\n",
				__FILE__, __LINE__, __FUNCTION__, getpid(), errno, strerror(errno), ret);
			Close();
			return -7;
		}

		return 0;
	}

	int Close() 
	{	//任务完成，该关闭了，刚才分别经历了初始化日志信息，创建或写入文件，创建传输用套接字，创建监控用epoll，创建业务用线程
		// 那么任务完成后刚才创建的东西都需要关闭，其中日志信息有LogInfo负责，剩下socket，epoll，thread
		if (m_server != NULL)
		{
			CSocketBase* p = m_server;	//因为确确实实new了一个对象出来，所以要就地释放
			m_server = NULL;
			delete p;
		}
		m_epoll.Close(); //调用它自己的close方法
		m_thread.Stop(); //调用它自己的stop方法

		return 0;
	}

	static void Trace(const LogInfo& info)	//trace 跟踪日志业务
	{ //给其他非日志进程的进程和线程使用
		static thread_local CSocket client; //一个线程对应一个客户端，上面的是服务端，这里是客户端
		int ret = 0;
		if (client == -1)
		{
			ret = client.Init(CSockParam("./log/server.sock", 0)); //客户端的init，初始化一下参数，参数也有单独封装
			if (ret != 0)
			{
#ifdef _DEBUG
				printf("%s(%d):[%s]ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
#endif
				return;
			}
			printf("%s(%d):[%s]ret=%d client=%d\n", __FILE__, __LINE__, __FUNCTION__, ret, (int)client);
			ret = client.Link(); //参数初始化完毕就需要connect，在这里就是客户端的Link，只有link，也就是connect完成后才能发送消息
			printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
			if(ret != 0) printf("errno:%d msg:%s\n", errno, strerror(errno));
		}
		ret = client.Send(info);	//客户端建立连接完成，可以发送消息
									//这里运行正常说明消息发送成功，能发出去了
		printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	}

	static Buffer GetTimeStr()	//得到当前时间，注意一下格式,
	{    //缓冲区封装一个buffer的好处就在无论什么样的信息都可以直接用buffer返回
		Buffer result(128);
		timeb tmb;
		ftime(&tmb);
		tm* pTm = localtime(&tmb.time);
		int nSize = snprintf(result, result.size(),
			"%04d-%02d-%02d_%02d-%02d-%02d.%03d",
			pTm->tm_year + 1900,
			pTm->tm_mon + 1,
			pTm->tm_mday,
			pTm->tm_hour,
			pTm->tm_min,
			pTm->tm_sec,
			tmb.millitm
		);
		result.resize(nSize);  //先准备一个大的空间，等写入完毕之后再resize到合适大小，动态调整内存大小
		return result;
	}

private:

	int ThreadFunc()	//线程的业务处理函数,在这里写日志
	{
		printf("进入线程...\n");
		EPEvents events; //负责存储所有epoll事件
		std::map<int, CSocketBase*> mapClients;	//负责存储所有客户端
		while ((m_thread.isValid()) && (m_epoll != -1) && (m_server != NULL)) //确保线程、epoll、服务器都活着
		{
			ssize_t ret = m_epoll.WaitEvents(events, 1000); //epoll_wait 等待监听的事件  这里要处理的事件就是每个客户端的日志写入工作
			//printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
			if (ret < 0) break; //小于0就是没有事件需要监听，就是没事，可以返回了
			if (ret > 0) 
			{ // 大于0是ret返回值就是监听的事件数量，用循环来处理每一个
				ssize_t i = 0;
				for (; i < ret; i++)
				{
					if (events[i].events & EPOLLERR)
					{ //当前事件有错误，epoll需要中止，无论服务端还是客户端都直接break
						break;
					}
					else if (events[i].events & EPOLLIN)
					{ //当前事件为EPOLLIN 有输入
						if (events[i].data.ptr == m_server)
						{ //服务端有输入，只有一种情况就是有客户端申请连接
							CSocketBase* pClient = NULL; //那就声明一个变量来承接一下客户端
							int r = m_server->Link(&pClient); //server的link，就是accept
							printf("%s(%d):<%s> r = %d\n", __FILE__, __LINE__, __FUNCTION__, r);
							if (r < 0) continue;

							//连接完成的客户端也添加到epoll的监控下
							r = m_epoll.Add(*pClient, EpollData((void*)pClient), EPOLLIN | EPOLLERR); 
							printf("%s(%d):<%s> r = %d\n", __FILE__, __LINE__, __FUNCTION__, r);
							if (r < 0)
							{ //加入到epoll失败就是有问题，就不需要继续了，释放空间后下一个
								delete pClient;
								continue;
							}
							auto it = mapClients.find(*pClient); //寻找map集合里有没有这个客户端
							if (it != mapClients.end())
							{
								if (it->second) delete it->second; //有就删除掉，重新赋值
							}
							mapClients[*pClient] = pClient; //将这个刚连接好的客户端加入到map集合
						}
						else
						{ //说明是 客户端 有输入，客户端有输入，说明需要写日志了，这里的写日志是从socket接收消息写到文件去
							printf("%s(%d):<%s> \n", __FILE__, __LINE__, __FUNCTION__);
							CSocketBase* pClient = (CSocketBase*)events[i].data.ptr; //先拿到当前需要执行这个写入事件的客户端套接字
							if (pClient != NULL)
							{ //不为空，就是正常，为空就不对了，它怎么过来的
								Buffer data(1024 * 1024); //1MB的缓冲区空间，日志一般不大于1M
								int r = pClient->Recv(data);	//从socket接收日志信息，有发有接
								//printf("%s(%d):<%s> r = %d\n", __FILE__, __LINE__, __FUNCTION__, r);
								if (r <= 0)
								{ //小于0 说明接收失败，这一次事件处理失败，清理这次的内存
									mapClients[*pClient] = NULL; //不要忘了map集合里的也要改
									delete pClient;
								}
								else
								{ //业务正常，可以写入文件了
									//printf("%s(%d):<%s> data = %s\n", __FILE__, __LINE__, __FUNCTION__, (char*)data);
									printf("%s(%d):<%s> data = %s\n", __FILE__, __LINE__, __FUNCTION__, "success");
									WriteLog(data); //data是已经接收成功的日志信息，调用写入日志文件的函数写入
								}
								printf("%s(%d):[%s]ret=%d \n", __FILE__, __LINE__, __FUNCTION__, r);
							}
						}
					}
				}
				if (i != ret)
				{ // i在外部增长，最终i不等于ret返回值的事件总数，说明中间处理过程中出错了，就直接break掉(这外面可是个while)
					break;
				}
			}
		}
		for (auto it = mapClients.begin(); it != mapClients.end(); it++)
		{
			if (it->second)
			{ //如果上面执行完毕，它还是非空，说明上面有问题，需要delete，防止内存泄露
				delete it->second;
			}
		}
		mapClients.clear(); //全部执行完毕，这一次业务处理完成，清空map集合，给下次做准备

		return 0;
	}

	void WriteLog(const Buffer& data)
	{ // 将缓冲区消息写入文件
		if (m_file != NULL)
		{
			FILE* pFile = m_file;
			fwrite((char*)data, 1, data.size(), pFile); //一次1字节，一共写size次
			fflush(pFile);	//刷新文件，刷新之后上次写入的内容就被保存，下次写入会根据写入规则跟在上次写入的内容后面
#ifdef _DEBUG
			printf("%s", (char*)data);
#endif
		}
	}


private:
	CThread m_thread;
	CEpoll m_epoll;
	CSocketBase* m_server;
	Buffer m_path; //日志路径
	FILE* m_file; //日志文件
};

#ifndef TRACE
	//定义几个宏，方便用方便识别 I D W E F
#define TRACEI(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_INFO, __VA_ARGS__))
#define TRACED(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_DEBUG, __VA_ARGS__))
#define TRACEW(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_WARNING, __VA_ARGS__))
#define TRACEE(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_ERROR, __VA_ARGS__))
#define TRACEF(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_FATAL, __VA_ARGS__))

//LOGI << "hello" << "kuuhaku";
#define LOGI LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_INFO)
#define LOGD LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_DEBUG)
#define LOGW LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_WARNING)
#define LOGE LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_ERROR)
#define LOGF LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_FATAL)


//内存导出  00 01 02 65 34; ...a....
#define DUMPI(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_INFO, data, size))
#define DUMPD(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_DEBUG, data, size))
#define DUMPW(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_WARNING, data, size))
#define DUMPE(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_ERROR, data, size))
#define DUMPF(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_FATAL, data, size))
#endif 

