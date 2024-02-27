#pragma once
#include "Public.h"
#include "DataBaseHelper.h"
#include "sqlite3/sqlite3.h"

class CSqlite3Client : public CDataBaseClient
{
public:
	CSqlite3Client(const CSqlite3Client&) = delete;
	CSqlite3Client& operator=(const CSqlite3Client&) = delete;
public:
	CSqlite3Client() 
	{
		m_db = NULL;
		m_stmt = NULL;
	}
	virtual ~CSqlite3Client() 
	{
		Close();
	}

public:
	virtual int Connect(const KeyValue& args); //连接数据库 sqlite mysql
	virtual int Exec(const Buffer& sql); //执行sql语句
	virtual int Exec(const Buffer& sql, Result& result, const _Table_& table); //执行查询类sql语句
	virtual int StartTransaction(); //开启事务
	virtual int CommitTransaction(); //提交事务
	virtual int RollbackTransaction(); //事务回滚
	virtual int Close(); //关闭连接
	virtual bool isConnected(); //是否已连接

private:
	static int ExecCallback(void* arg, int count, char** names, char** values);
	int ExecCallback(Result& result, const _Table_& table, int count, char** names, char** values);

private:
	sqlite3_stmt* m_stmt;
	sqlite3* m_db;

private:
	class ExecParam
	{
	public:
		ExecParam(CSqlite3Client* obj, Result& result, const _Table_& table) 
			:obj(obj), result(result),table(table)
		{}

		CSqlite3Client* obj;
		Result& result;
		const _Table_& table;
	};
};


class _sqlite3_table_ : public _Table_
{
public:
	_sqlite3_table_() : _Table_() {}
	_sqlite3_table_(const _sqlite3_table_& table);
	virtual ~_sqlite3_table_() {}
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


class _sqlite3_field_ : public _Field_
{
public:
	_sqlite3_field_();
	_sqlite3_field_(SqlType ntype, const Buffer& name, unsigned attr, const Buffer& type,
		const Buffer& size, const Buffer& default_, const Buffer& check);
	_sqlite3_field_(const _sqlite3_field_& field);

public:
	virtual ~_sqlite3_field_(); //因为用到了new，所以析构需要干活了
	virtual Buffer Create();
	virtual void LoadFromStr(const Buffer& str);
	virtual Buffer toEqualExp() const;
	virtual Buffer toSqlStr() const;
	virtual operator const Buffer() const; //列的全名

private:
	Buffer Str2Hex(const Buffer& data) const;
};


#define DECLARE_TABLE_CLASS(name, base) class name:public base \
{ \
public: \
virtual PTable Copy() const {return PTable(new name(*this));} \
name():base() { Name = #name;

#define DECLARE_FIELD(ntype, name, attr, type, size, default_, check) \
{ PField field(new _sqlite3_field_(ntype, #name, attr, type, size, default_, check)); FieldDefine.push_back(field); Fields[#name] = field; }

#define DECLARE_TABLE_CLASS_END() }};