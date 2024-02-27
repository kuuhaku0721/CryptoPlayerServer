#pragma once
#include <unistd.h>
#include <sys/epoll.h>
#include <vector>
#include <errno.h>
#include <sys/signal.h>
#include <memory.h>
#define EVENT_SIZE 128

using EPEvents = std::vector<epoll_event>; //vector 用来封装epoll事件的

class EpollData
{ //将epoll相关的数据封装 
public:
	EpollData() { m_data.u64 = 0; }
	//3、一般在传epoll_event 或者说传epoll_data_t的时候是传入了fd(socket)，但有了这个ptr，就可以在注册socket的时候传入想要的参数
	//然后在wait出来的时候用自己的函数去处理
	EpollData(void* ptr) { m_data.ptr = ptr; } //2、void类型的指针，在fd之外，还可以利用这个ptr传参，比如后面就传进去了m_server或pClient
	explicit EpollData(int fd) { m_data.fd = fd; }; //explicit不允许隐式转换
	//1、从这里的fd切入
	//在此之前，向epoll中装入fd是 epoll_event event.events.fd 而epoll_event内含的就是一个epoll_data_t
	//而epoll_data_t内有fd， 或者说 epoll_data_t是一个联合体，其中一部分是fd，对epoll_data_t进行封装之后就可以直接对其中的fd写入
	//而且可以使用epoll_data_t的其他参数 如ptr指针
	explicit EpollData(uint32_t u32) { m_data.u32 = u32; }
	explicit EpollData(uint64_t u64) { m_data.u64 = u64; }
	EpollData(const EpollData& data) { m_data.u64 = data.m_data.u64; } //拷贝构造，只需要传最大的那个，因为公用一块内存，最大的拷贝过去其他的也过去了

public:
	EpollData& operator=(const EpollData& data)
	{
		if (this != &data)
			m_data.u64 = data.m_data.u64;
		return *this;
	}
	EpollData& operator=(void* data)
	{
		m_data.ptr = data;
		return *this;
	}
	EpollData& operator=(int fd)
	{
		m_data.fd = fd;
		return *this;
	}
	EpollData& operator=(uint32_t u32)
	{
		m_data.u32 = u32;
		return *this;
	}
	EpollData& operator=(uint64_t u64)
	{
		m_data.u64 = u64;
		return *this;
	}
	operator epoll_data_t() { return m_data; }  //一系列的运算符重载，为了能够方便的拿到m_data数据，它是私有的
	operator epoll_data_t() const { return m_data; }  //多个不同的运算符重载，便于在任何时候任何情况下需要这个参数都能够直接拿到m_data的值
	operator epoll_data_t* () { return &m_data; }
	operator const epoll_data_t* ()const { return &m_data; }
private:
	epoll_data_t m_data; //联合体，共用一个内存空间
};

class CEpoll
{
public:
	CEpoll()
	{
		m_epoll = -1;
	}
	~CEpoll()
	{
		Close();
	}
	CEpoll(const CEpoll&) = delete;  //禁止拷贝
	CEpoll& operator=(const CEpoll&) = delete; //禁止赋值=，epoll独一份
	operator int()const { return m_epoll; }

public:
	//Epoll主要用于进程间通信
	int Create(unsigned count) //创建epoll，参数为epoll需要监听的数量,当然，莫得用
	{
		if (m_epoll != -1) return -1; //已存在
		m_epoll = epoll_create(count); //创建
		if (m_epoll == -1) return -2; //创建失败
		return 0;
	}
	ssize_t WaitEvents(EPEvents& events, int timeout = 10) //这里的events是个vector,在全局
	{ //小于0表示错误 等于0表示没有事件发生 大于0表示成功拿到事件
		if (m_epoll == -1) return -1;
		EPEvents evs(EVENT_SIZE);      //这是个vector，()是初始化，vector会自增长
		int ret = epoll_wait(m_epoll, evs.data(), (int)evs.size(), timeout); //等待事件的产生
		//参数(epoll句柄，事件集 在vector里，事件集大小 可以通过vector函数得到，超时时间)
		if (ret == -1)
		{
			if ((errno == EINTR) || (errno == EAGAIN)) //中断和again是正常情况可以不用管
				return 0;
			//其他的就是异常情况了
			return -2;
		}
		if (ret > (int)events.size())
		{	//如果等待的事件的数量超出了原本准备的事件集的大小需要重设事件集大小
			events.resize(ret);
		}
		memcpy(events.data(), evs.data(), sizeof(epoll_event) * ret); //把事件集从临时的vector转到参数events里去，别的还要用
		return ret;
	}

	int Add(int fd, const EpollData& data = EpollData((void*)0), uint32_t events = EPOLLIN) //默认参数，定义和声明只能写一份，不能同时声明
	{
		if (m_epoll == -1) return -1;
		epoll_event ev = { events, data }; //初始化参数，data有运算符重载
		//注册需要监听的事件，先拿到事件(上面)后传入ctl函数(下面)
		//参数（epoll句柄 create返回值， 操作 有宏， 监听的fd， 监听的事件）
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_ADD, fd, &ev);
		if (ret == -1) return -2;
		return 0;
	}

	int Modify(int fd, uint32_t events, const EpollData& data = EpollData((void*)0))
	{
		if (m_epoll == -1) return -1;
		epoll_event ev = { events, data };
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_MOD, fd, &ev); //修改的逻辑同注册
		if (ret == -1) return -2;
		return 0;
	}

	int Del(int fd)
	{
		if (m_epoll == -1) return -1;
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_DEL, fd, NULL); //从epoll中将fd删除
		if (ret == -1) return -2;
		return 0;
	}

	void Close()
	{
		if (m_epoll != -1)
		{
			int fd = m_epoll; //先[汇编]move转移
			m_epoll = -1; //move比close快，保证在另一个线程好巧不巧进来的时候epoll已经是-1
			close(fd); //close是需要时间的，这一段时间内还有可能有线程进来
		}
	}

private:
	int m_epoll;
};

