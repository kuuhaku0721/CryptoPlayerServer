#pragma once
#include "Public.h" //一个公共的字符串类
#include "DataBaseHelper.h"  //公共基类，两种数据库 sqlite3和MySQL，因为大同小异，只注释一下mysql
#include <mysql/mysql.h> //记得在VC++里面配置路径


class CMysqlClient : public CDataBaseClient
{
public:
	CMysqlClient(const CMysqlClient&) = delete;		//数据库就一个 禁止拷贝禁止赋值
	CMysqlClient& operator=(const CMysqlClient&) = delete;
public:
	CMysqlClient() 
	{
		bzero(&m_db, sizeof(m_db)); //构造时先把将来会存数据库的成员变量清零，不然init都过不去
		m_bInit = false; //是否初始化为false，用作后面的判断条件
	}
	virtual ~CMysqlClient()
	{
		Close(); //析构记得关闭链接，别的不需要特意去干
	}
	

public:
	//连接数据库 sqlite mysql
	virtual int Connect(const KeyValue& args); 
	//执行sql语句
	virtual int Exec(const Buffer& sql);
	//执行查询类sql语句
	virtual int Exec(const Buffer& sql, Result& result, const _Table_& table); 
	//开启事务
	virtual int StartTransaction(); 
	//提交事务
	virtual int CommitTransaction(); 
	//事务回滚
	virtual int RollbackTransaction(); 
	//关闭连接
	virtual int Close(); 
	//是否已连接
	virtual bool isConnected(); 

private:
	MYSQL m_db;	   //用于存储数据库的成员变量
	bool m_bInit; //默认false：没有初始化 true：已连接

private:
	class ExecParam		//执行sql时的参数的封装
	{
	public:
		ExecParam(CMysqlClient* obj, Result& result, const _Table_& table)
			:obj(obj), result(result), table(table)
		{}

		CMysqlClient* obj;  //mysql客户端本体，通过客户端去调用connect exec等等函数
		Result& result;     //存储结果的表
		const _Table_& table; //用到的参数表
	};
};


class _mysql_table_ : public _Table_
{
public:
	_mysql_table_() : _Table_() {}
	_mysql_table_(const _mysql_table_& table);
	virtual ~_mysql_table_();
	//创建和删除
	virtual Buffer Create(); //根据用户信息返回sql语句，执行在Exec，下面一样
	virtual Buffer Drop();
	//增删改查
	virtual Buffer Insert(const _Table_& values);
	virtual Buffer Delete(const _Table_& values);
	virtual Buffer Modify(const _Table_& values); //TODO: 参数优化
	virtual Buffer Query(const Buffer& condition = "");
	//创建基于表的对象
	virtual PTable Copy() const;
	//获取表全名
	virtual operator const Buffer() const;
	//清除使用状态
	virtual void ClearFieldUsed();
};


class _mysql_field_ : public _Field_
{
public:
	_mysql_field_();
	_mysql_field_(SqlType ntype, const Buffer& name, unsigned attr, const Buffer& type,
		const Buffer& size, const Buffer& default_, const Buffer& check);
	_mysql_field_(const _mysql_field_& field);
	virtual ~_mysql_field_(); //因为用到了new，所以析构需要干活了

public:
	virtual Buffer Create();
	virtual void LoadFromStr(const Buffer& str);
	virtual Buffer toEqualExp() const;
	virtual Buffer toSqlStr() const;
	virtual operator const Buffer() const; //列的全名

private:
	Buffer Str2Hex(const Buffer& data) const;	//转换为十六进制字符串
};

//宏定义的sql语句形式
#define DECLARE_TABLE_CLASS(name, base) class name:public base \
{ \
public: \
virtual PTable Copy() const {return PTable(new name(*this));} \
name():base() { Name = #name;

#define DECLARE_MYSQL_FIELD(ntype, name, attr, type, size, default_, check) \
{ PField field(new _mysql_field_(ntype, #name, attr, type, size, default_, check)); FieldDefine.push_back(field); Fields[#name] = field; }

#define DECLARE_TABLE_CLASS_END() }};