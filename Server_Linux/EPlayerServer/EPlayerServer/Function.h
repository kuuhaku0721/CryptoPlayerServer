#pragma once
#include <unistd.h>
#include <functional>
#include <sys/types.h>

class CSocketBase;  //告诉编译器，这是个类，是啥暂时不用管
class Buffer;       //这样不需要额外包含头文件，进而导致交叉引用的问题
                    //因为下面的类内虚函数用到了但是没有包含他们的头文件

//公共父类，确定类型
class CFunctionBase
{
public:
    virtual ~CFunctionBase() {}             //虚函数和模板函数不能同时使用  但是！模板类可以有虚函数
    virtual int operator()() { return -1; } //纯虚函数，强制所有子类必须实现,即所有子类都需要有重载()
    virtual int operator()(CSocketBase*) { return -1; }
    virtual int operator()(CSocketBase*, Buffer&) { return -1; }
};

//模板类
template<typename _FUNCTION_, typename... _ARGS_>
class CFunction : public CFunctionBase  //用继承体系，是为了在参数不确定的情况下其他的类能够识别父类，然后父类指针指向子类对象
{
public:

    CFunction(_FUNCTION_ func, _ARGS_... args)
        : m_binder(std::forward<_FUNCTION_>(func), std::forward<_ARGS_>(args)...)  //binder对象要初始化
    { }
    //虚析构
    virtual ~CFunction() {} //上有父类虚析构,虚析构用于传递析构函数，因为用的时候都是父类指针指向子类对象
    virtual int operator()()  //重载()是为了匿名对象
    {
        return m_binder();
    }

    typename std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type m_binder;       
        //一个辅助函数，帮助实现bind函数返回值类型推断，即将参数传进去，
        //然后并不确定其返回值类型的情况下可以直接使用binder(上面那样)，binder会自己推断返回值类型，方便直接使用
};

