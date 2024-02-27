#include "Logger.h"

//日志的构造函数 参数：文件路径(这里用的绝对路径), 当前行号，函数，进程号，线程号，日志级别(是个枚举用来区分类型), 可变参数信息
LogInfo::LogInfo(const char* file, int line, 
	const char* func, pid_t pid, pthread_t tid, 
	int level, const char* fmt, ...)
{ //define TRACEI	trace 跟踪，追踪
	const char sLevel[][8] = {			//枚举: 信息，调试，警告，错误，严重错误
		"INFO", "DEBUG", "WARNING", "ERROR", "FATAL"
	};
	char* buf = NULL;		//保存日志信息的缓冲区，首先声明的是用来写入公共头的，就是下面这个
	bAuto = false; //不是自动为false 就是自动的意思 在测试函数里可以看到，自动提交的只需要指定参数，非自动的有 <<
	int count = asprintf(&buf, "%s(%d):[%s][%s]<%d-%d>(%s) ",	//公共头信息，需要在这里指定类型
		file, line, sLevel[level],
		(char*)CLoggerServer::GetTimeStr(), pid, tid, func);
	if (count > 0)		//大于0说明写入成功，返回的写入的字节数
	{
		m_buf = buf;	//m_buf存的是最终的日志信息，这里先把公共头写进去，然后就可以释放buf了，以防后面写入别的信息后错乱
		free(buf);
	}
	else
		return;	

//关于可变参数，在参数列表里声明可变参数后，运用时一般是三部分，va_list:拿到参数列表 
//va_start:拿到第一个参数的地址 va_end:清空可变参数列表
//在使用是首先声明list参数列表，用start获取第一个参数地址，然后执行可变参数的行为，最后end，业务在start和end包裹范围之内执行

	va_list ap;	//用来控制可变参数的
	va_start(ap, fmt);  //start(list, param1)
	count = vasprintf(&buf, fmt, ap);
	if (count > 0)
	{
		m_buf += buf;	//用+=的形式继续写入
		free(buf); //然后清空这次，为下次做准备
	}
	m_buf += "\n"; //最后一个换行以区分不同日志语句
	va_end(ap); //在start和end包裹内，list会自增长
}

LogInfo::LogInfo(const char* file, int line, 
	const char* func, pid_t pid, pthread_t tid, int level)
{ //define LOGI 需要自己主动发  流式日志
	const char sLevel[][8] = {
		"INFO", "DEBUG", "WARNING", "ERROR", "FATAL"
	};
	char* buf = NULL;			//需要自己手动发送的日志只需要提前指定公共头，剩下的交给重载 <<
	bAuto = true; //非自动为true 说明需要手动
	int count = asprintf(&buf, "%s(%d):[%s][%s]<%d-%d>(%s) ",
		file, line, sLevel[level],
		(char*)CLoggerServer::GetTimeStr(), pid, tid, func);
	if (count > 0)
	{
		m_buf = buf;
		free(buf);
	}
}

LogInfo::LogInfo(const char* file, int line, 
	const char* func, pid_t pid, pthread_t tid, 
	int level, void* pData, size_t nSize)		//传入的是字符数组,和大小
{ //define DUMPI		dump转储(动态转为静态存储)
	const char sLevel[][8] = {
		"INFO", "DEBUG", "WARNING", "ERROR", "FATAL"
	};
	char* buf = NULL;
	bAuto = false;
	int count = asprintf(&buf, "%s(%d):[%s][%s]<%d-%d>(%s)\n",
		file, line, sLevel[level],
		(char*)CLoggerServer::GetTimeStr(), pid, tid, func);
	if (count > 0)
	{
		m_buf = buf;
		free(buf);
	}
	else
		return;

	//Buffer out;
	char* Data = (char*)pData; //传进来的是void* 需要做一次类型转换
	size_t i = 0;
	for (; i < nSize; i++)
	{
		char buf[16] = "";
		snprintf(buf, sizeof(buf), "%02X ", Data[i] & 0xFF); //字符转为整数会有位扩展，高位全是FFFF，所以与0xFF将高位置0
		m_buf += buf;
		//在这个循环内，上半部分写的是字节信息，最终效果就是 AF A8 65 7E 3A D9...这样的字节数据
		//下半部分展示的是对应的字符，以ASCII码能显示的为主，；分隔开，所以有一个%16的判断
		//前面的字节数够16个了就显示其对应的ASCII字母(对应的abc123),然后换行到下一行
		if (0 == ((i + 1) % 16))
		{
			m_buf += "\t; ";	//用于分隔的
			char buf[17] = "";  //准备一个缓冲区来接收后面的字符，
			memcpy(buf, Data + i - 15, 16); //直接将刚才已经打印过字节的信息复制到buf里去，
			// -15是因为i在外部，刚才输出了16个了已经，0~15 一共16个 减去15，再从刚才16个的第一个开始输出它所对应的字母
			for (int j = 0; j < 16; j++)
			{ //循环遍历一遍buf，将一些控制类ASICII码(或者无法打印的)换成 . 
				if (buf[j] < 32 && (buf[j] >= 0))
					buf[j] = '.';
			}
			m_buf += buf; //全部处理完之后以追加方式写入到m_buf，也就是最终的日志信息里去
			/*for (size_t j = i - 15; j <= i; j++)
			{
				if (((Data[j] & 0xFF) > 31) && ((Data[j] & 0xFF) < 0x7F))
				{
					m_buf += Data[i];
				}
				else
				{
					m_buf += ".";
				}
			}*/
			m_buf += "\n"; //最后追加一个换行
		}
	}
	//万一最后剩下的不够16个，也就是余数，需要用下面的来处理一下余数
	size_t k = i % 16;
	if (k != 0)
	{
		for (size_t j = 0; j < 16 - k; j++) m_buf += "   ";
		m_buf += "\t; ";
		for (size_t j = i - k; j <= i; j++)
		{
			if (((Data[j] & 0xFF) > 31) && ((Data[j] & 0xFF) < 0x7F))
			{
				m_buf += Data[i];
			}
			else
			{
				m_buf += ".";
			}
		}
		m_buf += "\n";
	}
}

LogInfo::~LogInfo()
{
	if (bAuto) //非自动为true 说明是手动，手动的需要追加一个换行，自动的在m_buf里已经追加过了
	{
		m_buf += "\n";
		CLoggerServer::Trace(*this); // 跟踪日志信息，跟踪自己是因为这里只是把日志信息写进了m_buf，还没有发送出去
		// trace是静态函数 全局有效，虽然也是Logger文件，但是这里是cpp，分开cpp是因为在Logger的逻辑里有循环调用
		// 如果写在一个文件里，按照代码由上至下的调用顺序，会出现下调上上转下的递归现象，所以分开来写
		// 之后在trace里有send
	}
}
