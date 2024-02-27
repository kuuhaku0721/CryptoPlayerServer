#include "CServer.h"
#include "Logger.h"

CServer::CServer()
{   //默认构造需要给用到的成员变量，该初始化的初始化，该置空的置空
    m_server = NULL;
    m_business = NULL;
}

int CServer::Init(CBusiness* business, const Buffer& ip, short port)
{
    int ret = 0;
    if (business == NULL) return -1; //business是一个实例化对象，只要构造函数执行成功它就不会是NULL
    m_business = business;                  //因为是服务端的初始化，服务端承担着创建子进程的工作，所以需要先将需要让子进程处理的业务明确好
    ret = m_process.SetEntryFunction(&CBusiness::BusinessProcess, m_business, &m_process);  //然后设置子进程将要处理的业务逻辑的入口函数
    if (ret != 0) return -2;
    ret = m_process.CreateSubProcess(); //服务端创建子进程，让子进程去处理业务模块
    if (ret != 0) return -3;
                                    //业务模块交给子进程处理了，然后就是5步走了
    ret = m_pool.Start(2);          //1、创建线程池
    if (ret != 0) return -4;
    ret = m_epoll.Create(2);        //2、创建epoll
    if (ret != 0) return -5;
    m_server = new CSocket();       //3、epoll添加监控的socket，的之前，先初始化将要监管的socket，这里是服务端，所以初始化服务端
    if (m_server == NULL) return -6;
    ret = m_server->Init(CSockParam(ip, port, SOCK_ISSERVER | SOCK_ISIP | SOCK_ISREUSE));  //3、1调用服务端自己的初始化函数让它去自己初始化
    if (ret != 0) return -7;
    ret = m_epoll.Add(*m_server, EpollData((void*)m_server));   //4、添加epoll的监管
    if (ret != 0) return -8;

    for (size_t i = 0; i < m_pool.Size(); i++)          //5、给线程池添加线程需要处理的任务
    {
        ret = m_pool.AddTask(&CServer::ThreadFunc, this);       /* 基本用到epoll和线程池的地方都是这么个逻辑，一些细小的区别就具体情况具体分析 */
        if (ret != 0) return -9;
    }

    return 0;
}

int CServer::Run()
{
    while (m_server != NULL)
    {
        usleep(10); //服务端运行，进程与进程之间休眠10微妙，免得撞车，其他的不需要额外的处理，它的任务在创建子进程的时候就已经完成了
    }
    return 0;
}

int CServer::Close()
{
    if (m_server)       //析构时调用close函数，也是根据成员变量该停的停该关的关
    {
        CSocketBase* sock = m_server;   //关闭套接字的时候需要特殊处理一下，先置空，让运行停掉，然后再删除，置空比delete要快
        m_server = NULL;                //所以先置空，要的就是一个快速反应，然后反正也不运行处理了，就可以慢慢delete了
        m_epoll.Del(*sock); //记得从epoll里面删除掉这个socket
        delete sock;
    }
    m_epoll.Close();        //close掉epoll
    m_process.SendFD(-1);   //进程发送一个-1代表停止
    m_pool.Close();         //线程池也关闭掉

    return 0;
}

int CServer::ThreadFunc()   //线程池的任务函数，线程池就是处理这个的,而一般线程处理的任务也就是跟epoll监管的socket相关，跟epoll相关的就是固定几步走
{
    TRACEI("epoll: %d server: %p", (int)m_epoll, m_server);
    int ret = 0;
    EPEvents events;            //0、先定义一下变量 和循环退出条件
    while ((m_epoll != -1) && (m_server != NULL))       
    {
        ssize_t size = m_epoll.WaitEvents(events, 500);      //1、拿到epoll监管的事件集 总数
        if (size < 0) break;
        if (size > 0)
        {
            TRACEI("size = %d, event %08X", size, events[0].events);
            for (ssize_t i = 0; i < size; i++)      //2、进入循环，遍历每一个epoll事件
            {
                if (events[i].events & EPOLLERR)    //3、判断条件分辨是ERR还是IN（有时IN的输入事件需要区分服务端输入还是客户端输入，ERR就是错了）
                {
                    break;
                }
                else if (events[i].events & EPOLLIN)
                {
                    if (m_server)       //3.1 代表服务端有输入 而服务端有输入只有一种情况：有客户端申请连接（客户端输入就是要接消息了）
                    {                   //4、根据输入信息的不同去处理业务逻辑
                        CSocketBase* pClient = NULL;   //4.1因为要接入客户端，所以要先准备一个承接客户端的变量
                        ret = m_server->Link(&pClient); //4.2 调用服务端socket的函数连接客户端
                        if (ret != 0) continue;
                        ret = m_process.SendSock(*pClient, *pClient);   //4.3通过进程将客户端发送出去
                        TRACEI("SendSock %d", ret);
                        int s = *pClient;
                        delete pClient;                                 //发送出去就不用管了，因为服务端的线程模块只负责发，不负责业务处理，业务处理有专门的函数负责
                        if (ret != 0)                                   //会有专门的Recv函数来接收这个已经连接了的客户端，比如业务处理进程
                        {                                               //接收是在PlayerServer里面，也就是客户端处理模块里面
                            TRACEE("send client: %d failed", s);
                            continue;
                        }
                        
                    }
                }
            }
        }
    }
    TRACEI("服务器终止");
    return 0;
}
