#include <queue>
#include <pthread.h>
#include <mutex>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <iostream>
#include <time.h>

//关于这个头文件的引入问题
#include <tr1/unordered_map>

#define LOG(...) do{fprintf(stdout,__VA_ARGS__);fflush(stdout);}while(0) //指定写到某目录 

#define MAX_HTTPHDR 4096	//4k

class RequestInfo
{
	//包含HttpRequest解析出的请求信息
public:
	std::string _method;//请求方法
	std::string _version;//协议版本
	std::string _path_info;//资源路径
	std::string _path_phys;//请求资源的实际路径
	std::string _query_string;//查询的字符串
	std::tr1::unordered_map<std::string, std::string> _hdr_list;//头部信息中的键值对
	struct stat _st;//文件类型

	//判断是否是CGI请求
	bool RequestIsCGI()
	{
		return true;
	}

public:
	std::string _error_code;//错误码，404、400、500...
	void SetError(const std::string &code)
	{
		_error_code = code;
	}
};


class HttpRequest
{
	//http数据的接收接口
	//http数据的解析接口
	//对外提供能够获取处理结果的接口
private:
	int _cli_sock;
	std::string _http_header;
	RequestInfo _req_info;

public:
	HttpRequest(int sock) :_cli_sock(sock) {}
	//接收Http请求头
	bool RecvHttpHeader(RequestInfo &info)
	{	
		//将数据放到缓冲区
		char tmp[MAX_HTTPHDR];
		while (true)
		{
			int ret = recv(_cli_sock, tmp, MAX_HTTPHDR, 0);
			if (ret <= 0)
			{
				//EINTR代表这次错错误是因为被信号打断
				//非阻塞形式 EAGAIN
				if (errno == EINTR || errno == EAGAIN)
					continue;
				//info._error_code = "500";
				info.SetError("500");
				return false;
			}
			if (strstr(tmp, "\r\n\r\n") == NULL) 
			{
				//TODO 1:24:13
			}
		}
		
		return true;
	}
	//解析http请求头
	bool ParseHttpHeader(RequestInfo &info)
	{

	}
	//向外提供解析结果
	RequestInfo& GetRequestInfo() 
	{
		
	}
};

class HttpResponse
{
	//提供文件请求（完成文件下载、列表功能）接口
	//CGI请求（文件上传）接口
private:
	int _cli_sock;
	std::string _etag;//表明文件是否修改过
	std::string _mtime;//文件的最后一次修改时间
	std::string _cont_len;//文件大小
public:
	HttpResponse(int sock) :_cli_sock(sock) {}
	bool InitResponse(RequestInfo req_info);//初始化请求的响应信息
	bool ProcessFile(RequestInfo &info);//文件下载功能
	bool ProcessList(RequestInfo &info);//文件列表功能
	bool ProcessCGI(RequestInfo &info);//CGI请求的处理

	//对外接口：1、处理错误响应
	bool ErrHandler(RequestInfo &info)
	{

	}

	//对外接口：2、文件上传
	bool CGIHandler(RequestInfo &info)
	{
		InitResponse(info);//初始化cgi响应信息
		ProcessCGI(info);//执行cgi响应
	}

	//对外接口：3、文件夹展示和文件下载
	bool FileHandler(RequestInfo &info)
	{
		//初始化文件响应信息
		InitResponse(info);

		//判断是否是目录
		if (DIR) {
			//文件夹展示
			ProcessList(info);
		}
		else
		{
			//文件下载
			ProcessFile(info);
		}
	}

};
