#include "qstatuscheck.h"
#include <Windows.h>
#include <QDebug>

HMODULE //代表载入的库的地址                    //HWND   是线程相关的，你可以通过HWND找到该窗口所属进程和线程。
(*MyLoadLibraryA)(                          //HANDLE 是代表系统的内核对象，如文件句柄，线程句柄，进程句柄
    _In_ LPCSTR lpLibFileName               //HINSTANCE 的本质是模块基地址，他仅仅在同一进程中才有意义，跨进程的HINSTANCE是没有意义
) = NULL;                                   //HMODULE 是代表应用程序载入的模块，win32系统下通常是被载入模块的线性地址。
//LPCSTR是Win32和VC++所使用的一种字符串数据类型。LPCSTR被定义成是一个指向以'\0'结尾的 常量字符 的指针。
//LPWSTR是wchar_t字符串
//LPCWSTR是一个指向unicode编码字符串的32位指针，所指向字符串是wchar型，而不是char型。

FARPROC                                         //声明了一个函数指针，变量名是MyGetProcAddress
(*MyGetProcAddress)(                            //函数返回的是FARPROC，FARPROC也是一个函数指针的类型
    _In_ HMODULE hModule,                       //MyGetProcAddress这个函数的作用是，获取dll里的指定函数名的函数地址
    _In_ LPCSTR lpProcName
);  //_In_ 的作用是声明这个参数是一个入参，函数会读取这个参数，但不会修改这个参数,这是WindowsAPI里的惯用手法，其实他就是一个宏，只是提示人们这个参数的类型

VOID (* myExitProcess)(UINT uExitCode);

static
class StaticData
{
public:
    StaticData()
    {
        MyLoadLibraryA = reinterpret_cast<HMODULE(*)(LPCSTR)>(LoadLibraryA);    //reinterpret_cast 强制类型转换，将一种类型转换为另一种类型，比如int*转为char*
        MyGetProcAddress = reinterpret_cast<FARPROC(*)(HMODULE, LPCSTR)>(GetProcAddress);
    }
} _;

QStatusCheck::QStatusCheck(QObject* parent) : QThread(parent)                       //TODO: 这俩函数，都看不懂
{                                                                                   //所以，是为了跨平台还是为了保证能在32位机器上运行？
    //             ExitProcess
    //             +-+-+-+-+-+
    //             BA987654321
    char data[] = "PnrlWlt_hqt";    //"ExitProcess";
    //            Kernel32.dll
    //            +-+-+-+-+-+-
    //            CBA987654321
    char dll[] = "WZ|eme9-2ank";

    for(int i = 0; i < 11; i++)
    {
        //还原ExitProcess     //PnrlWlt_hqt --> ExitProcess
        if(i % 2 == 0)
        {
            data[i] -= (11 - i);
        }
        else
        {
            data[i] += (11 - i);
        }
    }
    for(int i = 0; i < 12; i++)
    {
        //还原Kernel32.dll   //WZ|eme9-2ank --> Kernel32.dll
        if(i % 2 == 0)
        {
            dll[i] -= (12 - i);
        }
        else
        {
            dll[i] += (12 - i);
        }
    }
    HMODULE hdll = MyLoadLibraryA(dll);
    myExitProcess = (VOID(*)(UINT))MyGetProcAddress(hdll, data);
}
/*
 * Kernel32.dll是一个Windows内核模块，而内核是操作系统的核心部分，
 * 它执行基本操作，包括内存管理、输入/输出操作和中断。
 * Kernel32.dll也是Windows 操作系统中使用的32 位动态链接库文件，可以确保Windows程序正常运行。
*/

extern bool LOGIN_STATUS;
void QStatusCheck::run()        //线程函数：如果状态m_status是flase，就执行退出进程      emmmm, so?
{
    m_status = LOGIN_STATUS;
    if(m_status == false)
    {
        myExitProcess(0);
        //qDebug() << __FILE__ << __LINE__ << LOGIN_STATUS << m_status;
        abort();
        exit(0);
    }
}
