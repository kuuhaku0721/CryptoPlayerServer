#include "MysqlClient.h"
#include "Logger.h"
#include <sstream>

int CMysqlClient::Connect(const KeyValue& args)
{ //链接数据库
    if (m_bInit) return -1; //如果已经初始化了就退出
                //初始化
    MYSQL* ret = mysql_init(&m_db); //形式只能是MYSQL m_db  mysql_init(&m_db)
    if (ret == NULL) return -2;
                //链接
    ret = mysql_real_connect(&m_db, args.at("host"), args.at("user"),
        args.at("password"), args.at("db"), atoi(args.at("port")), NULL, 0);
    if ((ret == NULL) && (mysql_errno(&m_db) != 0))
    {
        printf("[%s] %d %s\n", __FUNCTION__, mysql_errno(&m_db), mysql_error(&m_db));
        TRACEE("%d %s", mysql_errno(&m_db), mysql_error(&m_db));
        mysql_close(&m_db);           //出错了记得关闭数据库
        bzero(&m_db, sizeof(m_db));   //将m_db清零，以供下一次链接使用
        return -3;
    }
    
    m_bInit = true;  //初始化完成，设置状态为true
    return 0;
}

int CMysqlClient::Exec(const Buffer& sql)
{ //执行sql语句 直接调用query即可                     //sql语句因为语法问题执行出错的可以在workbench里面验证一下
    if (!m_bInit) return -1;
                //执行sql
    int ret = mysql_real_query(&m_db, sql, sql.size());
    if (ret != 0)
    {
        printf("[%s] %d %s\n", __FUNCTION__, mysql_errno(&m_db), mysql_error(&m_db));
        TRACEE("%d %s", mysql_errno(&m_db), mysql_error(&m_db));
        return -2;
    }
    return 0;
}

int CMysqlClient::Exec(const Buffer& sql, Result& result, const _Table_& table)
{ //带返回值的执行 结果存在result中
    if (!m_bInit) return -1;
                //执行sql
    int ret = mysql_real_query(&m_db, sql, sql.size());
    if (ret != 0)
    {
        printf("[%s] %d %s\n", __FUNCTION__, mysql_errno(&m_db), mysql_error(&m_db));
        TRACEE("%d %s", mysql_errno(&m_db), mysql_error(&m_db));
        return -2;
    }
                        //sql执行的返回结果
    MYSQL_RES* res = mysql_store_result(&m_db);
    MYSQL_ROW row;   //结果的一行，想想一下sql查询的结果
    unsigned num_fields = mysql_num_fields(res);
    while ((row = mysql_fetch_row(res)) != NULL) //一行行取结果
    {
        PTable pt = table.Copy(); //copy 新建一个自己然后返回,是个表
        for (unsigned i = 0; i < num_fields; i++)
        {
            if (row[i] != NULL)
                pt->FieldDefine[i]->LoadFromStr(row[i]); //从字符串加载一行信息，放到表的一列里去
        }
        result.push_back(pt); //表放到结果表里去
    }
    return 0;
}

int CMysqlClient::StartTransaction()
{
    if (!m_bInit) return -1;

    int ret = mysql_real_query(&m_db, "BEGIN", 6);
    if (ret != 0)
    {
        printf("[%s] %d %s\n", __FUNCTION__, mysql_errno(&m_db), mysql_error(&m_db));
        TRACEE("%d %s", mysql_errno(&m_db), mysql_error(&m_db));
        return -2;
    }
    return 0;
}

int CMysqlClient::CommitTransaction()
{
    if (!m_bInit) return -1;
    int ret = mysql_real_query(&m_db, "COMMIT", 7);
    if (ret != 0)
    {
        printf("[%s] %d %s\n", __FUNCTION__, mysql_errno(&m_db), mysql_error(&m_db));
        TRACEE("%d %s", mysql_errno(&m_db), mysql_error(&m_db));
        return -2;
    }
    return 0;
}

int CMysqlClient::RollbackTransaction()
{
    if (!m_bInit) return -1;
    int ret = mysql_real_query(&m_db, "ROLLBACK", 9);
    if (ret != 0)
    {
        printf("[%s] %d %s\n", __FUNCTION__, mysql_errno(&m_db), mysql_error(&m_db));
        TRACEE("%d %s", mysql_errno(&m_db), mysql_error(&m_db));
        return -2;
    }
    return 0;
}

int CMysqlClient::Close()
{
    if (m_bInit) //如果已经初始化了才可以关闭
    {
        m_bInit = false; //初始化状态设为false
        mysql_close(&m_db); //关闭数据库
        bzero(&m_db, sizeof(m_db)); //清零以供下次使用
    }
    return 0;
}

bool CMysqlClient::isConnected()
{
    return m_bInit;
}

_mysql_table_::_mysql_table_(const _mysql_table_& table)  //表的拷贝构造
{
    Database = table.Database;
    Name = table.Name;
    for (size_t i = 0; i < table.FieldDefine.size(); i++)
    {
        PField field = PField(new _mysql_field_(*(_mysql_field_*)table.FieldDefine[i].get()));
        FieldDefine.push_back(field);
        Fields[field->Name] = field;
    }
}

_mysql_table_::~_mysql_table_()
{}

Buffer _mysql_table_::Create()      //创建表的sql语句 注意语法规范
{
    Buffer sql = "CREATE TABLE IF NOT EXISTS " + (Buffer)*this + " (\r\n";
    for (unsigned i = 0; i < FieldDefine.size(); i++)
    {
        if (i > 0) sql += ",\r\n";
        sql += FieldDefine[i]->Create();
        if (FieldDefine[i]->Attr & PRIMARY_KEY)
        {
            sql += ",\r\n PRIMARY KEY (`" + FieldDefine[i]->Name + "`)";
        }
        if (FieldDefine[i]->Attr & UNIQUE)
        {
            sql += ",\r\n UNIQUE INDEX `" + FieldDefine[i]->Name + "_UNIQUE` (";
            sql += (Buffer)*FieldDefine[i] + " ASC) VISIBLE ";
        }
    }
    sql += ");";
    return sql;
}

Buffer _mysql_table_::Drop() //删除表的sql语句
{
    return "DROP TABLE" + (Buffer)*this;
}

Buffer _mysql_table_::Insert(const _Table_& values)  //插入表的sql语句
{
    Buffer sql = "INSERT INTO " + (Buffer)*this + " (";
    bool isfirst = true;
    for (size_t i = 0; i < values.FieldDefine.size(); i++)
    {
        if (values.FieldDefine[i]->Condition & SQL_INSERT)
        {
            if (!isfirst) sql += ',';
            else isfirst = false;
            sql += (Buffer)*values.FieldDefine[i];
        }

    }
    sql += ") VALUES(";
    isfirst = true;
    for (size_t i = 0; i < values.FieldDefine.size(); i++)
    {
        if (values.FieldDefine[i]->Condition & SQL_INSERT)
        {
            if (!isfirst) sql += ',';
            else isfirst = false;
            sql += values.FieldDefine[i]->toSqlStr();
        }
    }
    sql += " );";

    printf("sql = %s\r\n", (char*)sql);
    return sql;
}

Buffer _mysql_table_::Delete(const _Table_& values)  //删除表的sql语句
{
    Buffer sql = "DELETE FROM " + (Buffer)*this + " ";
    Buffer Where = "";
    bool isfirst = true;
    for (size_t i = 0; i < FieldDefine.size(); i++)
    {
        if (FieldDefine[i]->Condition & SQL_CONDITION)
        {
            if (!isfirst) Where += " AND ";
            else isfirst = false;
            Where += (Buffer)*FieldDefine[i] + "=" + FieldDefine[i]->toSqlStr();
        }
    }
    if (Where.size() > 0)
        sql += " WHERE " + Where;
    sql += ";";

    printf("sql = %s\r\n", (char*)sql);
    return sql;
}

Buffer _mysql_table_::Modify(const _Table_& values) //更新表的sql语句
{
    Buffer sql = "UPDATE " + (Buffer)*this + " SET ";
    bool isfirst = true;
    for (size_t i = 0; i < values.FieldDefine.size(); i++)
    {
        if (values.FieldDefine[i]->Condition & SQL_MODIFY)
        {
            if (!isfirst) sql += ',';
            else isfirst = false;
            sql += (Buffer)*values.FieldDefine[i] + "=" + values.FieldDefine[i]->toSqlStr();
        }

    }
    Buffer Where = "";
    for (size_t i = 0; i < values.FieldDefine.size(); i++)
    {
        if (values.FieldDefine[i]->Condition & SQL_CONDITION)
        {
            if (!isfirst) Where += " AND ";
            else isfirst = false;
            Where += (Buffer)*values.FieldDefine[i] + "=" + values.FieldDefine[i]->toSqlStr();
        }
    }
    if (Where.size() > 0)
        sql += " WHERE " + Where;
    sql += " ;";

    printf("sql = %s\r\n", (char*)sql);
    return sql;
}

Buffer _mysql_table_::Query(const Buffer& condition)
{
    Buffer sql = "SELECT ";
    for (size_t i = 0; i < FieldDefine.size(); i++)
    {
        if (i > 0)sql += ',';
        sql += '`' + FieldDefine[i]->Name + "` ";
    }
    sql += " FROM " + (Buffer)*this + " ";
    if (condition.size() > 0)
    {
        sql += " WHERE " + condition;
    }
    sql += ";";

    printf("sql = %s\n", (char*)sql);
    return sql;
}

PTable _mysql_table_::Copy() const
{
    return PTable(new _mysql_table_(*this));
}

_mysql_table_::operator const Buffer() const
{
    Buffer Head;
    if (Database.size())
        Head = '`' + Database + "`.";  //因为是MySQL，所以一定是`` 而不能是""，引号会报错，还不好找原因
    return Head + '`' + Name + '`';
}

void _mysql_table_::ClearFieldUsed()
{
    for (size_t i = 0; i < FieldDefine.size(); i++)
    {
        FieldDefine[i]->Condition = 0;
    }
}

_mysql_field_::_mysql_field_() : _Field_()
{
    nType = TYPE_NULL;
    Value.Double = 0.0;
}

_mysql_field_::_mysql_field_(SqlType ntype, const Buffer& name, unsigned attr, const Buffer& type, const Buffer& size, const Buffer& default_, const Buffer& check)
{   //建表是创建一列的属性值
    nType = ntype;
    switch (ntype)
    {
    case TYPE_VARCHAR:
    case TYPE_TEXT:
    case TYPE_BLOB:
        Value.String = new Buffer();
        break;
    default:
        break;
    }
    Name = name;
    Attr = attr;
    Type = type;
    Size = size;
    Default = default_;
    Check = check;
}

_mysql_field_::_mysql_field_(const _mysql_field_& field)  //一列的拷贝构造
{
    nType = field.nType;
    switch (field.nType)
    {
    case TYPE_VARCHAR:
    case TYPE_TEXT:
    case TYPE_BLOB:
        Value.String = new Buffer();
        *Value.String = *field.Value.String;
        break;
    default:
        break;
    }
    Name = field.Name;
    Attr = field.Attr;
    Type = field.Type;
    Size = field.Size;
    Default = field.Default;
    Check = field.Check;
}

_mysql_field_::~_mysql_field_()
{
    if ((nType == TYPE_VARCHAR) || (nType == TYPE_TEXT) || (nType == TYPE_BLOB))
    {
        if (Value.String)       //在析构函数内，因为字符串类型是new出来的所以需要手动删除
        {
            Buffer* p = Value.String;
            Value.String = NULL;
            delete p;
        }
    }
}

Buffer _mysql_field_::Create()
{
    Buffer sql = "`" + Name + "` " + Type + Size + " ";
    if (Attr & NOT_NULL)
        sql += "NOT NULL";
    else
        sql += "NULL";
    //BLOB TEXT GEOMETRY JSON 不能有默认值
    if ((Attr & DEFAULT) && (Default.size() > 0) &&
        (Type != "BLOB") && (Type != "TEXT") && (Type != "GEOMETRY") && (Type != "JSON"))
    { //尽量减少字符串的判断
        sql += " DEFAULT \"" + Default + "\" ";
    }
    //UNIQUE PRIMARY_KEY 在建表时处理
    //CHECK 在定义时mysql不支持
    if (Attr & AUTOINCREMENT)
        sql += " AUTO_INCREMENT ";

    return sql;
}

void _mysql_field_::LoadFromStr(const Buffer& str)
{
    switch (nType)
    {
    case TYPE_NULL:     //啥也不干
        break;
    case TYPE_BOOL:
    case TYPE_INT:
    case TYPE_DATETIME:
        Value.Integer = atoi(str);
        break;
    case TYPE_REAL:
        Value.Double = atof(str);
        break;
    case TYPE_VARCHAR:
    case TYPE_TEXT:
        *Value.String = str;
        break;
    case TYPE_BLOB:
        *Value.String = Str2Hex(str); //转换成十六进制字符串
        break;
    default:
        printf("type = %d\n", nType);
        break;
    }
}

Buffer _mysql_field_::toEqualExp() const
{
    Buffer sql = (Buffer)*this + " = ";
    std::stringstream ss;
    switch (nType)
    {
    case TYPE_NULL:
        sql += " NULL ";
        break;
    case TYPE_BOOL:
    case TYPE_INT:
    case TYPE_DATETIME:
        ss << Value.Integer;
        sql += ss.str() + " ";
        break;
    case TYPE_REAL:
        ss << Value.Double;
        sql += ss.str() + " ";
        break;
    case TYPE_VARCHAR:
    case TYPE_TEXT:
    case TYPE_BLOB:
        sql += '`' + *Value.String + "` ";
        break;
    default:
        printf("type = %d\n", nType);
        break;
    }

    return sql;
}

Buffer _mysql_field_::toSqlStr() const
{
    Buffer sql = "";
    std::stringstream ss;
    switch (nType)
    {
    case TYPE_NULL:
        sql += " NULL ";
        break;
    case TYPE_BOOL:
    case TYPE_INT:
    case TYPE_DATETIME:
        ss << Value.Integer;
        sql += ss.str() + " ";
        break;
    case TYPE_REAL:
        ss << Value.Double;
        sql += ss.str() + " ";
        break;
    case TYPE_VARCHAR:
    case TYPE_TEXT:
    case TYPE_BLOB:
        sql += '"' + *Value.String + "\" ";
        break;
    default:
        printf("type = %d\n", nType);
        break;
    }

    return sql;
}

_mysql_field_::operator const Buffer() const
{
    return '`' + Name + '`';
}

Buffer _mysql_field_::Str2Hex(const Buffer& data) const
{
    const char* hex = "0123456789ABCDEF";
    std::stringstream ss;
    for (auto ch : data)
        ss << hex[(unsigned char)ch >> 4] << hex[(unsigned char)ch & 0xF];

    return ss.str();
}
