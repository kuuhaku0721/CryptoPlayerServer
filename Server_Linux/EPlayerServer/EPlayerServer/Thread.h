#pragma once
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <signal.h>
#include <map>
#include "Function.h"

class CThread
{
public:
	CThread() 
	{
		m_function = NULL;
		m_thread = 0; //非0 所有bit置1
		m_paused = false;
	}

	template<typename _FUNCTION_, typename... _ARGS_>
	CThread(_FUNCTION_ func, _ARGS_... args)
		:m_function(new CFunction<_FUNCTION_, _ARGS_...>(func, args...))
	{
		m_thread = 0;
		m_paused = false;
	}
	~CThread() {}
	CThread(const CThread&) = delete;
	CThread operator=(const CThread&) = delete;

public:
	template<typename _FUNCTION_, typename... _ARGS_>
	int SetThreadFunc(_FUNCTION_ func, _ARGS_... args)
	{
		m_function = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);
		if (m_function == NULL) return -1;  //对应new失败的情况，但是，new失败..内存不够...这情况很少见
		return 0;
	}

	int Start()
	{ //启动线程
		int ret = 0;
		pthread_attr_t attr; //线程参数联合体,初始化线程时用的，不用管啥用处，反正必须得有
		pthread_attr_init(&attr); //init初始化，配对使用destroy
		if (ret != 0) return -1;
		ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE); //detach分离状态,设置线程参数为分离状态,然后跟上创建create
		if (ret != 0) return -2;
		//ret = pthread_attr_setscope(&attr, PTHREAD_SCOPE_PROCESS); //竞争范围，进程内竞争  这个是默认的
		//if (ret != 0) return -3;
		ret = pthread_create(&m_thread, &attr, &CThread::ThreadEntry, this); //创建时的thread函数必须是全局能访问的， 并且创建时就要指定入口函数
					//通过一个全局中转 最终效果：create的全局thread函数调用private的真正的thread函数
		if (ret != 0) return -4;

		m_mapThread[m_thread] = this;  //创建完毕线程之后加入map映射表

		pthread_attr_destroy(&attr);
		return 0;
	}

	int Pause()
	{ //暂停
		if (m_thread != 0) return -1;
		if (m_paused)
		{
			m_paused = false;
			return 0;
		}
		m_paused = true;
		int ret = pthread_kill(m_thread, SIGUSR1); //用到两个信号量，一个代表暂停，一个代表停止，信号量绑定到行为，由信号量去触发行为(在下面)
		if (ret != 0)							   //虽然调用的是kill，但并不是当场就杀死，具体的行为要看给信号量绑定的行为是什么
		{
			m_paused = false;
			return -2;
		}
		return 0;
	}

	int Stop()
	{ //停止线程
		if (m_thread != 0)
		{
			pthread_t thread = m_thread;
			m_thread = 0;
			timespec ts;  //设置时间间隔的,当选择让线程停止之后不是立即就挂掉而是等待一段时间之后才挂掉，以防当时还有正在处理的程序
			ts.tv_sec = 0; //0s
			ts.tv_nsec = 100 * 1000000; //100ms
			int ret = pthread_timedjoin_np(thread, NULL, &ts); //线程挂起ts的时间间隔
			if (ret == ETIMEDOUT) //计时结束，可以挂了
			{
				pthread_detach(thread);  //先设置它的状态为分离状态（这个状态在create创建之前也出现过）
				pthread_kill(thread, SIGUSR2); //kill掉线程，同样用一个信号去触发，因为无论暂停还是杀死都是kill，所以需要用信号量去区分不同的行为
			}
		}
		return 0;
	}

	bool isValid() const { return m_thread != 0; } //就是简单的判断线程还在不在，活没活着

private:
	//__stdcall
	static void* ThreadEntry(void* arg)
	{ //静态方法 私有 引入thread function的入口 中转站
		CThread* thiz = (CThread*)arg; //static 不能用this 所以用thiz意指
		struct sigaction act = { 0 };
		sigemptyset(&act.sa_mask);
		act.sa_flags = SA_SIGINFO;
		act.sa_sigaction = &CThread::Sigaction; //设置信号量响应行为的对应的函数
		sigaction(SIGUSR1, &act, NULL); //设置两个信号量用
		sigaction(SIGUSR2, &act, NULL);

		thiz->EnterThread(); //由静态函数去调用私有函数，通过一个中转去调用真正的入口函数

		if (thiz->m_thread != 0) thiz->m_thread = 0;
		pthread_t thread = pthread_self(); //并非冗余，有可能stop把m_thread给清零
		auto it = m_mapThread.find(thread);
		if (it != m_mapThread.end())
			m_mapThread[thread] = NULL;
		pthread_detach(thread);   //在这里之所以让线程挂掉是因为：上面已经让线程进入它对应的线程函数去了，再运行到这里来说明线程函数已经处理完毕了
		pthread_exit(NULL);		  //既然这次任务已经完成了，那么这个线程的任务就完成哩，它就没有继续呆在这里的必要了，就可以光荣殉职了.
	}

	static void Sigaction(int signo, siginfo_t* info, void* context) //信号量所触发的行为的函数
	{
		if (signo == SIGUSR1) //一个信号量对应一个行为
		{ //留给pause用
			pthread_t thread = pthread_self();
			auto it = m_mapThread.find(thread);
			if (it != m_mapThread.end()) //线程存在，下一步
			{
				if (it->second) //线程对应包装类存在，下一步
				{
					while (it->second->m_paused) //直到触发它的pause暂停为止，进入循环
					{
						if (it->second->m_thread == 0) //如果包装类里面的thread本体为0，那肯定是出错了，不然它怎么绕过前面检验运行到这里来的
						{
							pthread_exit(NULL); //出错就直接清理退出线程
						}
						usleep(1000); //这里是暂停的正常逻辑  线程睡眠1ms 以此来达到暂停的效果
					}
				}
			}
		}
		if (signo == SIGUSR2)
		{ //线程退出
			pthread_exit(NULL);
		}
	}

	//__thiscall
	void EnterThread()
	{ //真正的线程函数，通过中转可以使用成员属性
		if (m_function != NULL)
		{
			int ret = (*m_function)();  //真正的线程函数，话虽如此...这个函数使用模板类实现的，所以这里直接一个简单的匿名对象调用即可
			if (ret != 0)
			{
				printf("%s(%d):[%s]ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
			}
		}
	}

private:
	CFunctionBase* m_function;  //公共函数基类，用类模板实现的，用于传入线程入口函数
	pthread_t m_thread;	//线程本体
	bool m_paused; //true暂停 false运行
	static std::map<pthread_t, CThread*> m_mapThread;  //线程本体和它的包装类的map映射表
};