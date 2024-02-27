#pragma once
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <fcntl.h>
#include "Public.h"

//关于枚举类型设置为2的n次方：方便复数类型时进行或运算
//比如1：001  2：010  3：100  当一个元素既有1也有2的属性时，就可以1|2  这样结果就是011，也就是能达到类型相加的效果
//具体一点就是 如果我既是服务器还是UDP类型，枚举类型就可以是 SOCK_ISSERVER | SOCK_ISUDP 其结果是 001 | 100 = 101
enum SockAttr
{ //套接字属性枚举类型 = 2的n次方
	SOCK_ISSERVER = 1,	//是否服务器 1：服务器 0：客户端
	SOCK_ISNOBLOCK = 2, //是否非阻塞 1：非阻塞 0：阻塞
	SOCK_ISUDP = 4,		//是否为UDP 1：UDP 0：TCP
	SOCK_ISIP = 8,		//是否为IP 1：IP 0:本地套接字
	SOCK_ISREUSE = 16	//是否重用地址
};

class CSockParam
{
public:
	CSockParam()
	{ //默认构造
		bzero(&addr_in, sizeof(addr_in)); //清空
		bzero(&addr_un, sizeof(addr_un)); //凡是使用套接字地址之前都要记得清空
		port = -1;
		attr = 0; //默认为 客户端 阻塞 TCP（就是enum默认为0）
	}
	CSockParam(const Buffer& ip, short port, int attr)
	{ //网络套接字
		this->ip = ip;
		this->port = port;
		this->attr = attr;
		addr_in.sin_family = AF_INET;
		addr_in.sin_port = htons(port);
		addr_in.sin_addr.s_addr = inet_addr(ip); //有运算符重载因此无需强转类型
	}
	CSockParam(const Buffer& path, int attr)
	{ //本地套接字
		this->ip = path;	//本地套接字没有端口，ip为本地文件的路径地址
		this->attr = attr;
		addr_un.sun_family = AF_UNIX;
		strcpy(addr_un.sun_path, path); //有运算符重载直接转成char*
	}
	CSockParam(const sockaddr_in* addrin, int attr)
	{
		this->ip = ip;
		this->port = port;
		this->attr = attr;
		memcpy(&addr_in, addrin, sizeof(addr_in));
	}
	CSockParam(const CSockParam& param)
	{ //拷贝构造
		this->ip = param.ip;
		this->port = param.port;
		this->attr = param.attr;
		memcpy(&addr_in, &param.addr_in, sizeof(addr_in));
		memcpy(&addr_un, &param.addr_un, sizeof(addr_un));
	}
	~CSockParam() {}

	CSockParam& operator=(const CSockParam& param)
	{ //赋值运算符。 拷贝和赋值 虽然不一定会用，但是得有
		if (this != &param)
		{
			this->ip = param.ip;
			this->port = param.port;
			this->attr = param.attr;
			memcpy(&addr_in, &param.addr_in, sizeof(addr_in));
			memcpy(&addr_un, &param.addr_un, sizeof(addr_un));
		}
		return *this;
	}
	sockaddr* addrin() { return (sockaddr*)&addr_in; }
	sockaddr* addrun() { return (sockaddr*)&addr_un; }
public:
	//地址
	sockaddr_in addr_in; //网络地址
	sockaddr_un addr_un; //本地socket地址
	//ip
	Buffer ip;
	//端口
	short port;
	//向上寻找 SockAttr
	int attr;
};

class CSocketBase
{
public:
	//基类的构造函数，子类会调用父类的构造函数，所以需要有一个构造
	CSocketBase()
	{
		m_socket = -1;
		m_status = 0;
	}
	//虚析构函数，传递析构操作
	virtual ~CSocketBase()
	{
		Close();
	}

	//初始化 服务器：套接字创建、bind、listen  客户端：套接字创建
	virtual int Init(const CSockParam& param) = 0;
	//链接 服务器：accept  客户端：connect		udp省略链接操作直接返回成功
	virtual int Link(CSocketBase** pClient = NULL) = 0;
	//发送数据
	virtual int Send(const Buffer& data) = 0;
	//接收数据
	virtual int Recv(Buffer& data) = 0;
	//关闭链接
	virtual int Close()
	{
		m_status = 3; //完成关闭状态
		if (m_socket != -1)
		{
			if ((m_param.attr & SOCK_ISSERVER) &&		//服务器 
				((m_param.attr & SOCK_ISIP) == 0))	//非IP
				unlink(m_param.ip);
			int fd = m_socket;
			m_socket = -1;
			close(fd);
		}
		return 0;
	}

	virtual operator int() { return m_socket; }
	virtual operator int() const { return m_socket; }
	virtual operator sockaddr_in* () { return &m_param.addr_in; }
	virtual operator const sockaddr_in* () const { return &m_param.addr_in; }

protected:
	//套接字文件描述符 默认-1
	int m_socket;
	//状态，以防某些情况下无法从套接字本体获取状态
	int m_status; //0：未完成初始化 1：完成初始化 2：完成链接 3：完成关闭 
	//初始化参数
	CSockParam m_param;
};


class CSocket : public CSocketBase
{
public:
	CSocket() : CSocketBase() {}
	CSocket(int sock) : CSocketBase()
	{ //客户端调用的构造函数
		m_socket = sock;
	}
	virtual ~CSocket() { Close(); }

	//初始化 服务器：套接字创建、bind、listen  客户端：套接字创建
	virtual int Init(const CSockParam& param)
	{
		if (m_status != 0) return -1;	//如果status不是0说明初始化过了，直接返回-1

		m_param = param;	//因为套接字创建和参数是分开的，所以要先拿到参数
		int type = (m_param.attr & SOCK_ISUDP) ? SOCK_DGRAM : SOCK_STREAM;  //根据枚举类型判断是哪种协议来决定type参数
		if (m_socket == -1)
		{
			if(param.attr & SOCK_ISIP)  //是ip说明是网络套接字
				m_socket = socket(PF_INET, type, 0);
			else                        //不是ip，说明是本地文件路径path，为本地套接字
				m_socket = socket(PF_LOCAL, type, 0);
		}
		else
			m_status = 2; //已经处于连接状态
		if (m_socket == -1) return -2;  //创建完了还是-1，说明创建失败了，就没必要继续下去了

		int ret = 0;
		if (m_param.attr & SOCK_ISREUSE)  //如果是重利用的话需要进行特殊设置，重利用：短时间内多次开启就会触发这个情况
		{
			int option = 1;
			ret = setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));  //通过手动设置让它重利用地址,免得出错
			if (ret == -1) return -7;
		}

		if (m_param.attr & SOCK_ISSERVER)  //这一段是只有服务器才走，服务器需要bind和listen，在这一部分客户端不需要干啥
		{ //服务端 有bind和listen
			if (param.attr & SOCK_ISIP)
				ret = bind(m_socket, m_param.addrin(), sizeof(sockaddr_in));  //是ip的话，网络套接字
			else
				ret = bind(m_socket, m_param.addrun(), sizeof(sockaddr_un)); //不是ip，是本地路径path，本地套接字
			if (ret == -1) return -3;

			ret = listen(m_socket, 32); //listen，排队队列数量暂定32，测试环境，不需要太多
			if (ret == -1) return -4;
		}
		if (m_param.attr & SOCK_ISNOBLOCK)    //本地套接字是非阻塞的，往文件内写日志的时候不影响主程序的运行，我跑我的，它写它的
		{
			int option = fcntl(m_socket, F_GETFL);
			if (option == -1) return -5;
			option |= O_NONBLOCK;
			ret = fcntl(m_socket, F_SETFL, option);
			if (ret == -1) return -6;
		}

		if (m_status == 0)  //其实一开始已经判断过了，但以防万一，再判断一次
			m_status = 1;
		return 0;
	}
	//链接 服务器：accept  客户端：connect		udp省略链接操作直接返回成功
	virtual int Link(CSocketBase** pClient = NULL)
	{
		if (m_status <= 0 || m_socket == -1) return -1;
		int ret = 0;
		if (m_param.attr & SOCK_ISSERVER) //确定是服务端才进行后面的逻辑
		{ //服务端  
			if (pClient == NULL) return -2; //服务端的accept需要传进来的客户端的支持
			CSockParam param;  //accept的时候是需要用的地址参数的，所以这里需要实例化一个param对象
			int fd = -1;
			socklen_t len = 0;
			if (m_param.attr & SOCK_ISIP) //网络套接字
			{
				param.attr |= SOCK_ISIP;
				len = sizeof(sockaddr_in);
				fd = accept(m_socket, param.addrin(), &len);
			}
			else
			{ //本地套接字，传进来的是一个本地文件，尽管如此也需要accpet，日志服务器对应的客户端就是本地文件
				len = sizeof(sockaddr_un);
				fd = accept(m_socket, param.addrun(), &len);
			}
			if (fd == -1) return -3;
			*pClient = new CSocket(fd); //如果是网络套接字，fd就是accept成功的套接字文件描述符，如果是本地套接字，fd就是本地文件描述符
			if (*pClient == NULL) return -4;
			ret = (*pClient)->Init(param);  //参数已经拿到了，调用客户端的初始化
			if (ret != 0)
			{
				delete (*pClient); //万一初始化失败要释放资源
				*pClient = NULL;
				return -5;
			}
		}
		else
		{ //客户端
			if (m_param.attr & SOCK_ISIP)
				ret = connect(m_socket, m_param.addrin(), sizeof(sockaddr_in));
			else
				ret = connect(m_socket, m_param.addrun(), sizeof(sockaddr_un));
			
			if (ret != 0) 
				return -6;
		}

		//链接成功
		m_status = 2;
		return 0;
	}
	//发送数据
	virtual int Send(const Buffer& data)
	{
		if ((m_status < 2) || (m_socket == -1)) return -1;
		
		ssize_t index = 0;
		while (index < (ssize_t)data.size())
		{				//单纯的 向套接字中写入信息
			ssize_t len = write(m_socket, (char*)data + index, data.size() - index); //用index控制长度 循环写入
			if (len == 0) return -2;
			if (len < 0) return -3;
			index += len;
		}

		return 0;
	}
	//接收数据	大于0接收成功 小于等于0接收失败
	virtual int Recv(Buffer& data)
	{
		if ((m_status < 2) || (m_socket == -1)) return -1;
		data.resize(1024 * 1024); //一次接收1MB大小的数据
		ssize_t len = read(m_socket, data, data.size());  //单纯的 从套接字读消息
		if (len > 0)
		{ //读成功的时候，返回值len是读取到的长度
			data.resize(len);  //读到多少就设为多大的大小，节省内存
			return (int)len;
		}
		data.clear();
		if (len < 0)
		{ //读失败的情况
			if (errno == EINTR || errno == EAGAIN)
			{ //没有接收到数据但套接字还在，比如还没发
				data.clear();
				return 0;
			}
			return -2; //出错
		}

		//套接字被关闭，接收失败
		return -3;
	}
	//关闭链接
	virtual int Close() { return CSocketBase::Close(); }  //关闭时无特殊操作，调用父类的close关闭即可
};