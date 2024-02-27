#pragma once
#include "Epoll.h"
#include "Thread.h"
#include  "Function.h"
#include "Socket.h"

class CThreadPool
{
public:
	CThreadPool()
	{
		m_server = NULL;
		timespec tp = { 0,0 };
		clock_gettime(CLOCK_REALTIME, &tp);
		char* buf = NULL;
		asprintf(&buf, "%d.%d.sock", tp.tv_sec % 100000, tp.tv_nsec % 1000000); //asprintf 不知道需要多大内存，让系统分配
		if (buf != NULL)
		{
			m_path = buf;
			free(buf);
		}
		usleep(1);
	}
	~CThreadPool()
	{
		Close();
	}
	CThreadPool(const CThreadPool&) = delete;
	CThreadPool& operator=(const CThreadPool&) = delete;

public:
	int Start(unsigned count)
	{
		int ret = 0;
		if (m_server != NULL) return -1; //已经初始化过了
		if (m_path.size() == 0) return -2; //构造失败了
		m_server = new CSocket(); //创建服务端套接字
		if (m_server == NULL) return -3;
		
		ret = m_server->Init(CSockParam(m_path, SOCK_ISSERVER));  //初始化服务器
		if (ret != 0) return -4;
		ret = m_epoll.Create(count); //创建epoll
		if (ret != 0) return -5;
		ret = m_epoll.Add(*m_server, EpollData((void*)m_server)); //把服务器添加到epoll的监管下
		if (ret != 0) return -6;

		m_threads.resize(count); //线程数组重设大小到参数大小，就是先准备这么多的空位置
		for (unsigned i = 0; i < count; i++) //然后往这些为止里塞线程进去
		{
			m_threads[i] = new CThread(&CThreadPool::TaskDispatch, this);
			if (m_threads[i] == NULL)
				return -7;
			ret = m_threads[i]->Start(); //线程拿到任务去执行
			if (ret != 0) return -8;
		}
		return 0;
	}
	void Close()
	{
		m_epoll.Close(); //关掉epoll
		if (m_server) //关掉服务器
		{
			CSocketBase* p = m_server;
			m_server = NULL;
			delete p;
		}
		for (auto thread : m_threads)  //线程得一个个关
		{
			if (thread)
				delete thread;
		}
		m_threads.clear(); //最后清空一下数组
		unlink(m_path);
	}

	template<typename _FUNCTION_, typename... _ARGS_>
	int AddTask(_FUNCTION_ func, _ARGS_... args)
	{ //添加任务的函数，因为不知道各个任务需求是多少个参数，所以用函数模板的形式实现
		int ret = 0;
		static thread_local CSocket client;
		if (client == -1)
		{
			ret = client.Init(CSockParam(m_path, 0)); //初始化并连接客户端
			if (ret != 0) return -1;
			ret = client.Link();
			if (ret != 0) return -2;
		}

		CFunctionBase* base = new CFunction<_FUNCTION_, _ARGS_...>(func, args...); //这个是需要处理认为的函数
		if (base == NULL) return -3;
		Buffer data(sizeof(base)); //将这项任务全部转化成一个字符串发送出去，只发送任务请求，任务的处理交给接收端
		memcpy(data, &base, sizeof(base));
		ret = client.Send(data); //发送出去就是成功
		if (ret != 0)
		{
			delete base;
			return -4;
		}
		return 0;
	}

	size_t Size() const { return m_threads.size(); }

private:
	int TaskDispatch()
	{
		while (m_epoll != -1) //保证epoll或者的状态，从EPEvents里面拿事件去处理
		{
			int ret = 0;
			EPEvents events;
			ssize_t esize = m_epoll.WaitEvents(events);
			if (esize > 0)
			{   //epoll事件中有待处理事件，用循环去一个个处理
				for (ssize_t i = 0; i < esize; i++)
				{
					if (events[i].events & EPOLLIN)
					{ //epoll收到的事件是输入
						CSocketBase* pClient = NULL;
						if (events[i].data.ptr == m_server)
						{ //还是服务器的输入请求，那就是有客户端在申请连接
							ret = m_server->Link(&pClient);  //服务端连接客户端
							if (ret != 0) continue;
							ret = m_epoll.Add(*pClient, EpollData((void*)pClient)); //然后将刚连接上的客户端也加到epoll管理下
							if (ret != 0)
							{
								delete pClient; //如果出错得清理释放资源
								continue;
							}
						}
						else
						{ //客户端有输入请求，就是客户端有数据来
							pClient = (CSocketBase*)events[i].data.ptr; //这种情况下直接从epoll里面就能拿到客户端的fd
							if (pClient)
							{
								CFunctionBase* base = NULL;
								Buffer data(sizeof(base));
								ret = pClient->Recv(data); //接收一下刚才打包成字符串形式(91行)的要处理的任务函数
								if (ret <= 0)
								{
									m_epoll.Del(*pClient); //出问题了记得从epoll里摘掉它，不然会一直触发
									delete pClient;
									continue;
								}
								memcpy(&base, (char*)data, sizeof(base));
								if (base != NULL)
								{
									(*base)();
									delete base;
								}
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
	std::vector<CThread*> m_threads; //vector里面的内容必须有构造和拷贝构造(自增长用)，但是线程不能拷贝赋值，所以放指针
	CSocketBase* m_server;
	Buffer m_path;
};