#pragma once
#include "Public.h"
#include <map>
#include <list>
#include <vector>
#include <memory>

class _Table_;	//一个声明，因为现在要用到，但是暂时又没有定义，所以先做一个声明，告诉编译器这是个类，等会用到自己去找
using PTable = std::shared_ptr<_Table_>;	//指代一张表，用于查询等		智能指针的好处：Effective C++
using KeyValue = std::map<Buffer, Buffer>;  //map键值对，两个buffer(string)对应的：属性名-属性值 如：“host” = 192.168.1.1
using Result = std::list<PTable>;			//存储结果的链表，存储的是一张结果表,用于查询结果等


class CDataBaseClient
{
public:
	CDataBaseClient(const CDataBaseClient&) = delete;		//数据库基类，数据库就一个，不能拷贝不能赋值,凡是不能的就要明确禁止
	CDataBaseClient& operator=(const CDataBaseClient&) = delete;
public:
	CDataBaseClient() {}
	virtual ~CDataBaseClient() {}

public:
	//连接数据库 sqlite mysql				//基类，无需实现，只定义接口，实现交给子类
	virtual int Connect(const KeyValue& args) = 0; 
	//执行sql语句
	virtual int Exec(const Buffer& sql) = 0; 
	//执行查询类sql语句
	virtual int Exec(const Buffer& sql, Result& result, const _Table_& table) = 0; 
	//开启事务
	virtual int StartTransaction() = 0; 
	//提交事务
	virtual int CommitTransaction() = 0; 
	//事务回滚
	virtual int RollbackTransaction() = 0; 
	//关闭连接
	virtual int Close() = 0; 
	//是否已连接
	virtual bool isConnected() = 0; 
};


class _Field_;  //列的基类，表的一列，也是先声明，因为列还没定义，但是下面的表基类要用
using PField = std::shared_ptr<_Field_>;	//智能指针指向一列值de指针对象
using FieldArray = std::vector<PField>;     //列数组，创建表(或者插入之类的)需要定义多项属性，一个属性就是一列 
using FieldMap = std::map<Buffer, PField>;  //列的键值对，用于查找列用，存储： 列名的字符串-列的指针对象,    列名是给数据库用户看的，指针对象是给操作人员看的

//设置基类是因为有两种数据库：sqlite3和MySQL，两种数据库在语法上有些许差异但结构相同，所以设置公共基类
class _Table_
{ //表的基类
public:
	_Table_() {}
	virtual ~_Table_() {}
	//创建和删除			根据用户信息返回sql语句，执行在Exec，下面一样
	virtual Buffer Create() = 0;
	virtual Buffer Drop() = 0;
	//增删改查
	virtual Buffer Insert(const _Table_& values) = 0;
	virtual Buffer Delete(const _Table_& values) = 0;
	virtual Buffer Modify(const _Table_& values) = 0;
	virtual Buffer Query(const Buffer& condition = "") = 0;
	//创建基于表的对象
	virtual PTable Copy() const = 0;
	//获取表全名
	virtual operator const Buffer() const = 0;
	//清除表使用状态
	virtual void ClearFieldUsed() = 0;

public:
	Buffer Database; //表所属DB的名称
	Buffer Name; //表名
	FieldArray FieldDefine; //列的定义，存储查询结果,即所有列属性值
	FieldMap Fields; //列的定义映射表,用于搜索列或者存储 map存储内容：列名字符串-列的指针对象
};

enum {
	SQL_INSERT = 1,		//插入的列
	SQL_MODIFY = 2,		//修改的列
	SQL_CONDITION = 4	//查询的条件
};
	//创建表时列所可以指定的属性
enum {
	NONE = 0,
	NOT_NULL = 1,
	DEFAULT = 2,
	UNIQUE = 4,
	PRIMARY_KEY = 8,
	CHECK = 16,
	AUTOINCREMENT = 32
};
	//sql语句执行的数据类型
enum SqlType {
	TYPE_NULL = 0,
	TYPE_BOOL = 1,
	TYPE_INT = 2,
	TYPE_DATETIME = 4,
	TYPE_REAL = 8,
	TYPE_VARCHAR = 16,
	TYPE_TEXT = 32,
	TYPE_BLOB = 64
};

class _Field_
{ //列的基类
public:
	_Field_() {}
	virtual ~_Field_() {}
	_Field_(const _Field_& field)
	{
		Name = field.Name;
		Type = field.Type;
		Size = field.Size;
		Attr = field.Attr;
		Default = field.Default;
		Check = field.Check;
	}
	virtual _Field_& operator=(const _Field_& field)
	{
		if (this != &field)
		{
			Name = field.Name;
			Type = field.Type;
			Size = field.Size;
			Attr = field.Attr;
			Default = field.Default;
			Check = field.Check;
		}
		return *this;
	}

public:
	//创建一列
	virtual Buffer Create() = 0;
	//从字符串加载 即字符串转sql语句
	virtual void LoadFromStr(const Buffer& str) = 0;
	//转换为=语句
	virtual Buffer toEqualExp() const = 0;
	//转换为sql语句
	virtual Buffer toSqlStr() const = 0;
	//获取列的全名的操作符重载，作用是使用单纯的类名的时候返回的结果是列名
	virtual operator const Buffer() const = 0; 

public:
	Buffer Name; //列名
	Buffer Type; //类型
	Buffer Size; //大小 如varchar需要指定的大小
	unsigned Attr; //参数
	Buffer Default; //默认值
	Buffer Check; //约束条件
	unsigned Condition; //where条件
	union {
		bool Bool;
		int Integer;
		double Double;
		Buffer* String;
	}Value;
	SqlType nType; //列的类型
};
