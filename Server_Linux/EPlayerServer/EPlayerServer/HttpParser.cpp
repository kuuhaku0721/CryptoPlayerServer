#include "HttpParser.h"

CHttpParser::CHttpParser()
{
	m_complete = false;		//先设置未完成状态
	memset(&m_parser, 0, sizeof(m_parser)); //清零以防万一
	m_parser.data = this; //data设置的可以是任何参数，会传递到回调函数里
		//避免回调函数只能是标准调用或者静态函数，没办法直接和成员函数挂钩，但是传递进去this指针之后就可以和成员函数挂钩
	//通过上面的this，将下面的属性传递给静态方法，然后静态方法再去调用成员方法
	http_parser_init(&m_parser, HTTP_REQUEST);	//调用公共库自己的init初始化函数
	memset(&m_settings, 0, sizeof(m_settings)); //清零setting信息 然后开始输入,下面的一一对应即可
	m_settings.on_message_begin = &CHttpParser::OnMessageBegin;
	m_settings.on_url = &CHttpParser::OnUrl;
	m_settings.on_status = &CHttpParser::OnStatus;
	m_settings.on_header_field = &CHttpParser::OnHeaderField;
	m_settings.on_header_value = &CHttpParser::OnHeaderValue;
	m_settings.on_headers_complete = &CHttpParser::OnHeadersComplete;
	m_settings.on_body = &CHttpParser::OnBody;
	m_settings.on_message_complete = &CHttpParser::OnMessageComplete;
}

CHttpParser::~CHttpParser()	//没有new的部分，没有需要close的部分，析构不需要特地干些什么
{
}

CHttpParser::CHttpParser(const CHttpParser& http)	//http解析器拷贝构造函数，传入的参数即需要解析的http数据
{
	memcpy(&m_parser, &http.m_parser, sizeof(m_parser));
	m_parser.data = this; 
	memcpy(&m_settings, &http.m_settings, sizeof(m_settings));
	m_status = http.m_status;
	m_url = http.m_url;
	m_body = http.m_body;
	m_complete = http.m_complete;
	m_lastField = http.m_lastField;

}

CHttpParser& CHttpParser::operator=(const CHttpParser& http) //赋值，跟拷贝一样，只不过多一部非自己的判断，不能自己赋值给自己
{
	if (this != &http)
	{
		memcpy(&m_parser, &http.m_parser, sizeof(m_parser));
		m_parser.data = this;
		memcpy(&m_settings, &http.m_settings, sizeof(m_settings));
		m_status = http.m_status;
		m_url = http.m_url;
		m_body = http.m_body;
		m_complete = http.m_complete;
		m_lastField = http.m_lastField;
	}
	return *this;
}

size_t CHttpParser::Parser(const Buffer& data)
{
	m_complete = false;	//先设置完成状态为false
	size_t ret = http_parser_execute(&m_parser, &m_settings, data, data.size());  //然后调用公共库的execute执行即可
	if (m_complete == false)
	{
		m_parser.http_errno = 0x7F; //127
		return 0;
	}
	return ret;
}

int CHttpParser::OnMessageBegin(http_parser* parser)
{
	return ((CHttpParser*)parser->data)->OnMessageBegin();
}

int CHttpParser::OnHeadersComplete(http_parser* parser)
{
	return ((CHttpParser*)parser->data)->OnHeadersComplete();
}

int CHttpParser::OnMessageComplete(http_parser* parser)
{
	return ((CHttpParser*)parser->data)->OnMessageComplete();
}

int CHttpParser::OnUrl(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnUrl(at, length);
}

int CHttpParser::OnStatus(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnStatus(at, length);
}

int CHttpParser::OnHeaderField(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnHeaderField(at, length);
}

int CHttpParser::OnHeaderValue(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnHeaderValue(at, length);
}

int CHttpParser::OnBody(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnBody(at, length);
}

int CHttpParser::OnMessageBegin()
{
	return 0;
}

int CHttpParser::OnHeadersComplete()
{
	return 0;
}

int CHttpParser::OnMessageComplete()
{
	m_complete = true;
	return 0;
}

int CHttpParser::OnUrl(const char* at, size_t length)
{
	m_url = Buffer(at, length);  //封装一个字符串，开始位置是at，长度是length，就是分割url为一个个模块用的
	return 0;
}

int CHttpParser::OnStatus(const char* at, size_t length)
{
	m_status = Buffer(at, length);
	return 0;
}

int CHttpParser::OnHeaderField(const char* at, size_t length)
{
	m_lastField = Buffer(at, length);
	return 0;
}

int CHttpParser::OnHeaderValue(const char* at, size_t length)
{
	m_HeaderValues[m_lastField] = Buffer(at, length);
	return 0;
}

int CHttpParser::OnBody(const char* at, size_t length)
{
	m_body = Buffer(at, length);
	return 0;
}

UrlParser::UrlParser(const Buffer& url)
{
	m_url = url;
}

int UrlParser::Parser()
{ //一共解析三步：协议、域名和端口、uri(url info)、键值对
		//解析协议
	const char* pos = m_url;
	const char* target = strstr(pos, "://"); //strstr 取到后面参数指明的位置之前
	if (target == NULL) return -1;
	m_protocol = Buffer(pos, target);  //从开始位置到参数":\\"的位置之间的位置就是协议部分 如https:\\
		//解析域名和端口
	pos = target + 3; //往后挪三个位置 这三个就是:\\的位置，再往后就是域名和端口
	target = strchr(pos, '/');  //域名和端口和后面的内容以/作为分割，比如LocalHost/8080
	if (target == NULL)  //没有端口部分，只有域名部分，那端口就是默认的80
	{
		if (m_protocol.size() + 3 >= m_url.size())
			return -2;
		m_host = pos;
		return 0;
	}
	Buffer value = Buffer(pos, target); //截至/之前的部分为域名和端口
	if (value.size() == 0) return -3;
	target = strchr(value, ':');		//在域名和端口的结合体之中找端口，端口和后面的内容以:分割
	if (target != NULL)
	{ //如果存在：后面的内容
		m_host = Buffer(value, target); //域名就是从value头开始到：之前 
		m_port = atoi(Buffer(target + 1, (char*)value + value.size())); //加一个跳过去：，剩下的就是端口
	}
	else
	{ //后面没东西，那整个都是域名
		m_host = value;
	}
		//解析uri
	pos = strchr(pos, '/');  //url info的部分是/之后？之前。/之前是域名和端口，？之后是属性值的键值对
	target = strchr(pos, '?');
	if (target == NULL)
	{
		m_uri = pos + 1; //后面键值对没有，那整个都是uri
		return 0;
	}
	else
	{
		m_uri = Buffer(pos + 1, target); //先拿到uri，剩下的就是键值对部分
			//解析key-value
		pos = target + 1;	//前闭后开区间，所以位置加1
		const char* t = NULL;
		do
		{
			target = strchr(pos, '&');  //键值对之间以&分隔
			if (target == NULL)
			{
				t = strchr(pos, '='); //=前后分别为键值 key=value  如name=Tom&age=18
				if (t == NULL) return -4;
				m_values[Buffer(pos, t)] = Buffer(t + 1); //键值对数组里加一项
			}
			else
			{
				Buffer kv(pos, target); //先拿到整个键值对，三部分 key = value 三部分
				t = strchr(kv, '='); //在这三部分里面找=的位置
				if (t == NULL) return -5;
				m_values[Buffer(kv, t)] = Buffer(t + 1, (char*)kv + kv.size());
				pos = target + 1; //跳过& 往后挪一个
			}
		} while (target != NULL);
	}

	return 0;
}

Buffer UrlParser::operator[](const Buffer& name) const
{
	auto it = m_values.find(name);		//为了能够以字符串形式使用[]，所以重载下标运算符
	if (it == m_values.end()) return Buffer();
	return it->second; //根据key找value，返回value值
}

void UrlParser::SetUrl(const Buffer& url)
{
	m_url = url;	//设置默认url
	m_protocol = "";
	m_host = "";
	m_port = 80;
	m_values.clear();
}
