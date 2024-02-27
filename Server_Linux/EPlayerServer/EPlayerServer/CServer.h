#pragma once
#include "Socket.h"
#include "ThreadPool.h"
#include "Epoll.h"
#include "Process.h"


template<typename _FUNCTION_, typename... _ARGS_>
class CConnectedFunction : public CFunctionBase				//用模板函数是为了能够接收任意类型的函数当作参数
{															//connect和receive分开写是为了避免在调用时因为重载不知道调哪个
public:														//最终需要的只是一个客户端和数据的绑定体
															//不要想，去感受
	CConnectedFunction(_FUNCTION_ func, _ARGS_... args)
		: m_binder(std::forward<_FUNCTION_>(func), std::forward<_ARGS_>(args)...)
	{ }
	//虚析构
	virtual ~CConnectedFunction() {}

	virtual int operator()(CSocketBase* pClient)
	{
		return m_binder(pClient);
	}

	typename std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type m_binder;
};

template<typename _FUNCTION_, typename... _ARGS_>
class CReceivedFunction : public CFunctionBase
{
public:

	CReceivedFunction(_FUNCTION_ func, _ARGS_... args)
		: m_binder(std::forward<_FUNCTION_>(func), std::forward<_ARGS_>(args)...)
	{ }
	//虚析构
	virtual ~CReceivedFunction() {}

	virtual int operator()(CSocketBase* pClient, Buffer& data)
	{
		return m_binder(pClient, data);
	}

	typename std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type m_binder;
};


class CBusiness				//业务处理的父类，提供两个回调参数，作用.....暂时不知道，后面再回来补充
{
public:
	CBusiness() : m_connectedCallback(NULL), m_recvCallback(NULL) {}
	virtual int BusinessProcess(CProcess* proc) = 0;

	template<typename _FUNCTION_, typename... _ARGS_>
	int setConnectedCallback(_FUNCTION_ func, _ARGS_... args) 
	{
		m_connectedCallback = new CConnectedFunction<_FUNCTION_, _ARGS_...>(func, args...);
		if (m_connectedCallback == NULL) return -1;

		return 0;
		
	}

	template<typename _FUNCTION_, typename... _ARGS_>
	int setRecvCallback(_FUNCTION_ func, _ARGS_... args)
	{
		m_recvCallback = new CReceivedFunction<_FUNCTION_, _ARGS_...>(func, args...);
		if (m_recvCallback == NULL) return -1;

		return 0;
	}

protected:
	CFunctionBase* m_connectedCallback;
	CFunctionBase* m_recvCallback;
};

class CServer		// 详情见对应.cpp文件
{
public:
	CServer();
	~CServer() { Close(); }
	CServer(const CServer&) = delete;
	CServer& operator=(const CServer&) = delete;

public:
	int Init(CBusiness* business, const Buffer& ip = "0.0.0.0", short port = 10721);
	int Run();
	int Close();

private:
	int ThreadFunc();

private:
	CThreadPool m_pool;
	CSocketBase* m_server;
	CEpoll m_epoll;				//成员变量部分，万变不离其宗的是:一定有epoll和线程池
	CProcess m_process;
	CBusiness* m_business; //业务模块
};

