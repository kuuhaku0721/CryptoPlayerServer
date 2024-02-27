#pragma once
#include "Public.h"	//简单理解为一个string
#include "http_parser.h"		//开源的公共库
#include <map>

class CHttpParser	//http解析器，http就是网站按了F12之后出来的控制台后的报文信息，包含了http传送的数据包等等东西
{
private:
	http_parser m_parser; //使用一个公共库的对象
	http_parser_settings m_settings; //设置信息
	std::map<Buffer, Buffer> m_HeaderValues; //头部信息
	Buffer m_status; //状态信息
	Buffer m_url; //url部分信息
	Buffer m_body; //body部分信息
	bool m_complete; //是否解析完成的标志
	Buffer m_lastField; //前面全部解析完剩下的最后一部分

public:
	CHttpParser();
	~CHttpParser();
	CHttpParser(const CHttpParser& http);
	CHttpParser& operator=(const CHttpParser& http);

public:
	size_t Parser(const Buffer& data);		//parser：解析器，传入的即需要解析的报文信息
	//GET POSt ... 等等请求方法
	unsigned Method() const { return m_parser.method; }
	const std::map<Buffer, Buffer>& Header() { return m_HeaderValues; }
	const Buffer& Status() const { return m_status; }
	const Buffer& Url() const { return m_url; }
	const Buffer& Body() const { return m_body; }
	unsigned Errno() const { return m_parser.http_errno; }

protected:
	//三个 http_cb		cb callback  即解析http解析过程的回调函数
	static int OnMessageBegin(http_parser* parser);
	static int OnHeadersComplete(http_parser* parser);
	static int OnMessageComplete(http_parser* parser);
	//剩下几个 http_data_cb		即http解析出的各部分数据的回调函数
	static int OnUrl(http_parser* parser, const char* at, size_t length);
	static int OnStatus(http_parser* parser, const char* at, size_t length);
	static int OnHeaderField(http_parser* parser, const char* at, size_t length);
	static int OnHeaderValue(http_parser* parser, const char* at, size_t length);
	static int OnBody(http_parser* parser, const char* at, size_t length);

	//上面的静态方法对应的成员方法		需要用：调用静态方法-静态方法调用成员方法-成员方法具体实现的方法实现成员方法的调用
	int OnMessageBegin();
	int OnHeadersComplete();
	int OnMessageComplete();

	int OnUrl(const char* at, size_t length);
	int OnStatus(const char* at, size_t length);
	int OnHeaderField(const char* at, size_t length);
	int OnHeaderValue(const char* at, size_t length);
	int OnBody(const char* at, size_t length);
};


class UrlParser			//url解析器，url就是网址上的那一行网址链接
{
public:
	UrlParser(const Buffer& url);
	~UrlParser() {}
	int Parser();
	Buffer operator[](const Buffer& name) const;
	Buffer Protocol() const { return m_protocol; }
	Buffer Host() const { return m_host; }
	int Port() const { return m_port; } //默认返回80
	void SetUrl(const Buffer& url);
	const Buffer Uri() const { return m_uri; }

private:
	Buffer m_url;  //url本体
	Buffer m_protocol; //url协议
	Buffer m_host; //网址的域名
	Buffer m_uri; //url info 即url后面的具体信息
	int m_port; //端口 如8080
	std::map<Buffer, Buffer> m_values; //最后的每一个属性和对应的值 ？= ？的形式
};

