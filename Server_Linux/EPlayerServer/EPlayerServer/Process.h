#pragma once
#include <memory.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include "Function.h"

//进程类 决定入口函数 最终入口函数的去向在创建子进程之后
class CProcess
{
public:
    CProcess()
    { //默认构造，初始化
        m_func = NULL;
        memset(pipes, 0, sizeof(pipes)); //记住这个的头文件是memory
    }
    ~CProcess()
    { //析构函数，释放内存，记得判断条件
        if (m_func != NULL)
        {
            delete m_func;
            m_func = NULL;
        }
    }

    //进程入口函数        函数类型            可变参数类型
    template<typename _FUNCTION_, typename... _ARGS_> //在不确定参数类型和个数的情况下使用模板参数可以提高复用性
    int SetEntryFunction(_FUNCTION_ func, _ARGS_... args)
    {
        m_func = new CFunction<_FUNCTION_, _ARGS_...>(func, args...); //设置入口函数，即以传入的参数为基准赋值给m_func对象
        //之后m_func对象就是需要的入口函数

        return 0;
    }
    //找到了：为什么要用辅助数据(附属数据)去传递fd：要传递的fd，本质上传递的是文件描述符在内核中对应的地址，
    //而辅助数据的内容是由内核提供的，因此要将fd封装进辅助数据中，要传送的数据是msg，再将辅助数据cmsg封装进msg中
    // 最终通过msg进行父进程和子进程之间的消息传递
//创建子进程
    int CreateSubProcess()
    {
        if (m_func == NULL) return -1;
        //            Linux下一般填local 套接字类型   协议一般0  存储套接字的管道，提前定义       by the way 该函数是阻塞的
        int ret = socketpair(AF_LOCAL, SOCK_STREAM, 0, pipes);      //socketpair 只能用于有亲缘关系的父子进程间，而且只能用于Linux环境下
        //socketpair用于创建一对无名的，相互连接的套接字，套接字来自最后一个参数pipes(管道)[0][1]两个
        //在相互链接的两个套接字直接传递信息，一个读一个写，一个设置为读之后其写入会阻塞，另一个同理，由此达成一种单向通信
        //双向通信就再来一个pipe，读写反着再来一遍，发送和接收消息用 sendmsg 和 recvmsg 负责(在下面)

        if (ret == -1) return -2;

        pid_t pid = fork(); //创建子进程
        if (pid == -1) return -3; //创建失败返回-1
        if (pid == 0)
        { //主进程
            close(pipes[1]); //关闭: 写  子进程关闭写留下读
            pipes[1] = 0; //置空
            ret = (*m_func)(); //*解引用，m_func成员是指针，*解引用后是个类对象，()是重载操作符
            //返回值拿到的是一个binder对象，重载()返回值为binder对象
            exit(0); //上面一步创建完对象之后直接退出
        }
        //子进程
        close(pipes[0]); //关闭: 读 主进程关闭读留下写
        pipes[0] = 0; //置空
        m_pid = pid; //记录子进程pid
        return 0;
    }

    int SendFD(int fd) //主进程完成
    {
        struct msghdr msg; //声明一个保存要传送数据的结构体

        iovec iov[2]; //需要传递的是文件描述符，这里传两个数据只是准备缓冲区
        //iov是msg要传送的数据的类型，是要传送的数据而不是附属数据，需要的是附属数据，要传送的随意
        char buf[2][10] = { "eplayer", "kuuhaku" };
        iov[0].iov_base = buf[0]; //随意的传送一下数据，前提是保证结构完整
        iov[0].iov_len = sizeof(buf[0]); //也就是定义完整_base和_len
        iov[1].iov_base = buf[1];
        iov[1].iov_len = sizeof(buf[1]);
        msg.msg_iov = iov; //将要传送的数据的iov封装进msg中
        msg.msg_iovlen = 2; //同时把长度也封装进去

        //下面的才是真正传输的数据 -- 文件描述符
        /*cmsghdr* cmsg = new cmsghdr;
        bzero(cmsg, sizeof(cmsghdr));*/
        cmsghdr* cmsg = (cmsghdr*)calloc(1, CMSG_LEN(sizeof(int))); //cmsg是msg的附属数据的结构体声明
        if (cmsg == NULL) return -1;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int)); //CMSG_LEN 控制附属数据的长度
        cmsg->cmsg_level = SOL_SOCKET; //原始的协议级别
        cmsg->cmsg_type = SCM_RIGHTS; //控制信息的类型
        //CMSG_DATA 指明附属数据的位置，需要的就是这个附属数据，所以传入想要传递的fd
        *(int*)CMSG_DATA(cmsg) = fd; //从这里才传入文件描述符，将文件描述符封装进cmsg中，然后下面
        msg.msg_control = cmsg; //再将cmsg封装进msg中，最终传递的是msg
        msg.msg_controllen = cmsg->cmsg_len; //同时把长度也封装进去

        printf("%s(%d):<%s> pipes[1] = %d\n", __FILE__, __LINE__, __FUNCTION__, pipes[1]);
        printf("%s(%d):<%s> msg = %s\n", __FILE__, __LINE__, __FUNCTION__, msg);
        ssize_t ret = sendmsg(pipes[1], &msg, 0);
        //sendmsg 将数据由指定的socket发送给对方主机，此处是在两个已经建立链接的socket(pipe)之间发送消息
        //通过在fork的子进程后面读写状态的改变使得能够一方发送消息，一方接收消息，发送的消息在msg内
        //发送的目的是将文件描述符fd发送出去，而文件描述符fd就存在msg中
        free(cmsg); //无论成功还是失败都需要释放内存
        if (ret == -1)
        { //ret = -1 说明发送失败
            printf("%s(%d):<%s> error:%d-->msg: %s\n", __FILE__, __LINE__, __FUNCTION__, errno, strerror(errno));
            return -2;
        }

        return 0;
    }
    //如此上下发收两步，最终目的是为了传递文件描述符,传递的msg只是个幌子，准备缓冲区用的，最终的目的是传递DATA体的fd文件描述符信息
    int RecvFD(int& fd)
    {
        msghdr msg; //要发送或接收的消息 的结构体，很大，有很多东西在里面                        //有个问题，为啥fd要放在附属数据里面传递  答案：36行
        iovec iov[2]; //这个是要发送或接收的数据（但其实要的不是这个，要的是附属数据）
        char buf[][10] = { "", "" }; //用一个缓冲区来初始化以下数据
        iov[0].iov_base = buf[0];   //下面四步用来定义一下要发送的数据
        iov[0].iov_len = sizeof(buf[0]);
        iov[1].iov_base = buf[1];
        iov[1].iov_len = sizeof(buf[1]);
        msg.msg_iov = iov; //将iov，也就是要接收的数据封装进msg中
        msg.msg_iovlen = 2; //要接收的数据长度

        //msg 和 cmsg 组合使用 为啥 不知道  ----> msg负责的是消息本体(连带附属消息) cmsg负责的是协议之类的数据，两个结构体负责的数据不同，msg内含cmsg

        cmsghdr* cmsg = (cmsghdr*)calloc(1, CMSG_LEN(sizeof(int))); //msg，要发送或接收的msg 的附属数据 的结构体
        if (cmsg == NULL) return -1;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int)); //附属数据的字节长度，包含了结构头的尺寸，由后面的CMSG_LEN决定
        cmsg->cmsg_level = SOL_SOCKET; //原始的协议级别
        cmsg->cmsg_type = SCM_RIGHTS; //控制信息类型
        msg.msg_control = cmsg; //将附属数据封装进本体的msg中
        msg.msg_controllen = CMSG_LEN(sizeof(int)); //附属数据的长度，也是由CMSG_LEN决定，要与上面的对应

        //然后正式接收数据，接收数据存储到msg中
        ssize_t ret = recvmsg(pipes[0], &msg, 0);
        //从管道接受信息到 msg 接收消息一端，一上面发送消息端对应，接收到的消息存储在msg中
        if (ret == -1)
        {
            free(cmsg); //用malloc申请地空间要用free释放，一一对应
            return -2;
        }

        //最终从msg中取得需要的fd， CMSG_DATA 指明附属数据的位置，也就是取得附属数据
        fd = *(int*)CMSG_DATA(cmsg); //读取文件描述符，上面传递的幌子msg的最终目的是传递DATA中的文件描述符
        //*解引用 (int*)类型强转 CMSG_DATA 携带所需信息的部分

        return 0;
    }
                                            //下面是两个特化的版本，专门用来发sock 和地址 的，上面的只管发fd
    int SendSock(int fd, const sockaddr_in* addrin)
    {
        struct msghdr msg;              //过程：1、定义msg，然后向msg传入想要传输的数据
        iovec iov;                      // 2、定义cmsg，确定协议、长度等信息
        char buf[20] = "";              // 3、将cmsg封装进msg
        bzero(&msg, sizeof(msg));       //4、发送，然后释放内存
        memcpy(buf, addrin, sizeof(sockaddr_in));
        iov.iov_base = buf;             //从这里把addrin传过去
        iov.iov_len = sizeof(buf);
        msg.msg_iov = &iov; 
        msg.msg_iovlen = 1; 
        
        cmsghdr* cmsg = (cmsghdr*)calloc(1, CMSG_LEN(sizeof(int))); 
        if (cmsg == NULL) return -1;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int)); 
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS; 

        *(int*)CMSG_DATA(cmsg) = fd; 
        msg.msg_control = cmsg; 
        msg.msg_controllen = cmsg->cmsg_len;

        ssize_t ret = sendmsg(pipes[1], &msg, 0);
        free(cmsg);
        if (ret == -1)
        {
            printf("%s(%d):<%s> error:%d-->msg: %s\n", __FILE__, __LINE__, __FUNCTION__, errno, strerror(errno));
            return -2;
        }
        return 0;
    }
    
    int RecvSock(int& fd, sockaddr_in* addr_in)
    {
        msghdr msg;
        iovec iov; 
        char buf[20] = ""; 
        bzero(&msg, sizeof(msg));
        iov.iov_base = buf;
        iov.iov_len = sizeof(buf);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        cmsghdr* cmsg = (cmsghdr*)calloc(1, CMSG_LEN(sizeof(int))); 
        if (cmsg == NULL) return -1;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int)); 
        cmsg->cmsg_level = SOL_SOCKET; 
        cmsg->cmsg_type = SCM_RIGHTS; 
        msg.msg_control = cmsg; 
        msg.msg_controllen = CMSG_LEN(sizeof(int)); 

        ssize_t ret = recvmsg(pipes[0], &msg, 0);
        if (ret == -1)
        {
            free(cmsg);
            return -2;
        }
        memcpy(addr_in, buf, sizeof(sockaddr_in));
        fd = *(int*)CMSG_DATA(cmsg); 
        free(cmsg);
        return 0;
    }

    //守护进程函数
    static int SwitchDeamon()
    {
        pid_t pid = fork();
        if (pid == -1) return -1; //出错返回
        if (pid > 0) exit(0); //主进程，直接原地去世
        //子进程
        int ret = setsid();
        if (ret == -1) return -2; //出错返回
        pid = fork();
        if (pid == -1) return -3; //出错返回
        if (pid > 0) exit(0); //子进程，原地去世
        //下面是孙进程,由此进入守护状态
        for (int i = 0; i < 3; i++)
        {
            close(i); //关闭标准输入输出 0输入1输出2错误输出
        }
        umask(0);
        signal(SIGCHLD, SIG_IGN); //屏蔽SIGCHILD信号
        return 0;
    }

private:
    //因为需要使用泛型编程，所以参数没有确定，而参数不确定就没法识别，所以用继承的方法声明一个父类对象，new的时候指向子类的模板类
    CFunctionBase* m_func; //由参数传递进来的server，也就是需要入口函数的本体，用到了继承体系
    pid_t m_pid; //子进程的进程id
    int pipes[2]; //主进程和子进程之间进行通信的管道
    //一方读另一方写，关闭A的读B的写，通信时就只能A写B读A->B，创建子进程copy的时候管道也会copy一份，
    //所以父进程关闭读子进程关闭写(也就是父进程写子进程读)，然后剩下的读和写之间就能形成连接，从而达成父进程向子进程传递信息(fd)
    //socketpair是套接字对，存的两个套接字，关闭时关的套接字
    //如果需要子进程向父进程传递，则需要再来一份管道，关闭的顺序和上面相反，然后令这个管道也建立链接，这样就能与上面反方向地传递信息
};