#ifndef __UTILS_HPP__
#define __UTILS_HPP__

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

#include <sstream>
#include <iostream>
#include <time.h>

//关于这个头文件的引入问题
#include <tr1/unordered_map>
#include <unordered_map>
#include "../../../../usr/include/c++/7/string"

#define LOG(...) do{fprintf(stdout,__VA_ARGS__);fflush(stdout);}while(0) //指定写到某目录

#define MAX_HTTPHDR 4096	//4k
#define MAX_PATH 256
#define WWWROOT "www"
#define MAX_BUFF 4096

std::unordered_map<std::string, std::string> g_err_desc = {
  {"200", "OK"},
  {"400", "Bad Request"},
  {"403", "Forbidden"},
  {"404", "Not Find"},
  {"405", "Method Not Allowed"},
  {"413", "Requst Entity Too Large"},
  {"500", "Internal Server Error"}
};

class Utils
{
public:
	//字符串格式分隔
	static int Split(std::string &src, const std::string &seg, std::vector<std::string> &vec)
	{
		int num = 0; //分隔数量
		size_t idx = 0;//遍历计数
		size_t pos; //目标位置
		//key: val\r\nkey: val\r\nkey: val
		while (idx < src.size())
		{
			pos = src.find(seg, idx);
			if (pos == std::string::npos)
			{
				break;
			}
			vec.push_back(src.substr(idx, pos - idx));
			num++;
			idx = pos + seg.length();
		}
		if (idx < src.length()) {
			vec.push_back(src.substr(idx));
			num++;
		}
		return num;
	}

	//通过错误码获取错误信息
	static const std::string GetErrDesc(const std::string& code)
	{
		auto it = g_err_desc.find(code);
		if (it == g_err_desc.end())
		{
			return "Unknow Error";
		}
		return it->second;
	}

	static void TimeToGMT(time_t t, std::string &gmt)
	{
		//gmtime将当前时间戳转换为格林威治时间，转换出来的是一个结构体，
		//该结构体里面有年月日时分秒，一周的第一天，一年的第几天等信息
		struct tm *mt = gmtime(&t);
		char tmp[128] = { 0 };
		//将结构体的时间按照一定格式，转换为特定时间,返回字符串实际长度
		int len = strftime(tmp, 127, "%a %d %b %Y %H:%M:%S GMT", mt);
		gmt.assign(tmp, len);
	}

	//将数字转为字符串,还是我的Java好呀....
	static void DigitToStr(int64_t num, std::string &str)
	{
		std::stringstream ss;
		ss << num;
		str = ss.str();
	}

	//将字符串转为数字,还是我的Java好呀....
	static int64_t StrToDigit(std::string &str)
	{
		int64_t num;
		std::stringstream ss;
		ss << str;
		ss >> num;
		return num;
	}
	static void MakeETag(int64_t size, int64_t ino, int64_t mtime, std::string &etag){

	    std::stringstream ss;
	    ss << "\"" << std::hex << ino <<"-"<< std::hex
	    << size << "-" << std::hex << mtime << "\"";
	    etag = ss.str();
	}
};

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
    struct stat _st;//stat结构体，通过文件名获取文件详细信息

	//判断是否是CGI请求
	bool RequestIsCGI()
	{
		return (_method == "GET" && !_query_string.empty()) ||
			   (_method == "POST");
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
		LOG("into RecvHttpHeader!!\n");
		//将数据放到缓冲区
		char tmp[MAX_HTTPHDR] = { 0 };
		while (true)
		{
			int ret = recv(_cli_sock, tmp, MAX_HTTPHDR, 0);
			if (ret <= 0)
			{
				//EINTR代表这次错错误是因为被信号打断
				//非阻塞形式 EAGAIN
				if (errno == EINTR || errno == EAGAIN)
					continue;
				info.SetError("500");
				return false;
			}

			char* ptr = strstr(tmp, "\r\n\r\n");
			if ( ptr == NULL && (ret == MAX_HTTPHDR))
			{
				info.SetError("413");
				return false;
			}
			else if ((ptr == NULL) && (ret < MAX_HTTPHDR)) {
				usleep(1000);
				continue;
			}

			int header_len = ptr - tmp;
			//截取header_len这么长的字符串
			_http_header.assign(tmp, header_len);
			//把头部移除，其实就是读走
			recv(_cli_sock, tmp, header_len + 4, 0);
			LOG("header:[%s]\n", _http_header.c_str());
			break;
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
	//ETage: "inode-fsize-mtime"\r\n
	std::string _etag;//表明文件是否
	std::string _mtime;//文件的最后一次修改时间
	std::string _date;//
public:
	HttpResponse(int sock) :_cli_sock(sock)
	{

	}
	//初始化请求的响应信息
	bool InitResponse(RequestInfo req_info)
	{
        Utils::DigitToStr(req_info._st.st_mtime, _mtime);
        Utils::MakeETag(req_info._st.st_size, req_info._st.st_ino,
                         req_info._st.st_mtime, _etag);

        time_t t = time(NULL);
        Utils::TimeToGMT(t, _date);
		return true;
	}

	bool SendData(std::string &buf)
    {
        return send(_cli_sock, buf.c_str(), buf.length(), 0) >= 0;
    }
    bool SendCData(std::string &buf)
    {
	    if(buf.empty()){
	        return SendData("0\r\n\r\n");
	    }
	    std::stringstream ss;
	    ss << std::hex << buf.length() << "\r\n";
	    ss << "\r\n";
	    SendData(ss.str());
	    SendData(buf);
	    SendData("\r\n");
	    return true;
	}

	//文件下载功能
	bool ProcessFile(RequestInfo &info)
	{
		return true;
	}
	//文件列表功能
	bool ProcessList(RequestInfo &info)
	{
        //组织头部信息
        //首行
        //Content-Type: text/html\r\n
        //Etag: \r\n
        //Data: \r\n
        //Connection: close\r\n\r\n   //就是接受完请求就关闭链接，因为是短链接
        //正文：每一个目录下的文件都要组织一个html信息
        //头部
        std::string rsp_header;
        rsp_header = info._version + " 200 OK\r\n";

        rsp_header += "Content-Type: text/html;charset=UTF-8\r\n";
        rsp_header += "Connection: close\r\n";
        if(info._version == "HTTP/1.1")
        {
            rsp_header += "Transfer-Encoding: chunked\r\n";
        }
        rsp_header += "ETag: "+ _etag + "\r\n";
        rsp_header += "Last-Modifyed: " + _mtime + "\r\n";
        rsp_header += "Data: "+ _date + "\r\n\r\n";

        SendData(rsp_header);

        std::string rsp_body;
        rsp_body = "<html><head>";
        rsp_body += "<title>" + info._path_info + "</title>";
        rsp_body += "<meta charset='UTF-8'>";

        rsp_body += "</head><body>";
        rsp_body += "<h1>" + info._path_info + "</h1><hr />";
        while(true){
            std::string file_html;
            SendCData(file_html);

        }
        rsp_body = "</body></html>";
	    SendCData(rsp_body);
	    SendCData("");
		return true;
	}
	//CGI请求的处理
	bool ProcessCGI(RequestInfo &info)
	{
		return true;
	}

	//对外接口：1、处理错误响应
	bool ErrHandler(RequestInfo &info)
	{
		//响应头信息
		std::string rsp_header;

		//首行 版本  状态码 状态描述\r\n
		//头部 长度： Content-Length    当前系统时间:Date
		//空行
		//正文 rsp_body = "<html><body><h1> 404; <h1></body></html>"
		//一般错误没有正文 有的话：html标签  正文标签 标题

		//响应正文 <html><body><h1> 404;<h1></body></html>
		//首行：版本 状态码 描述信息（code对应的描述信息）
		time_t t = time(NULL);
		std::string rsp_body;
		std::string gmt;
		std::string body_length;

		//组织正文
		rsp_body = "<html><body><h1>" + info._error_code;
		rsp_body += "<h1></body></html>";

		//组织头部
		rsp_header = info._version + " " + info._error_code + " ";
		rsp_header += Utils::GetErrDesc(info._error_code) + "\r\n";
		Utils::TimeToGMT(t, gmt);
		rsp_header += "Date: " + gmt + "\r\n";
		Utils::DigitToStr(rsp_body.length(), body_length);
		rsp_header += "Content-Length: " + body_length+"\r\n\r\n";

		//发送头部和正文
		send(_cli_sock, rsp_header.c_str(), rsp_header.length(), 0);
		send(_cli_sock, rsp_body.c_str(), rsp_body.length(), 0);
		return true;
	}

	//对外接口：2、文件上传
	bool CGIHandler(RequestInfo &info)
	{
		InitResponse(info);//初始化cgi响应信息
		ProcessCGI(info);//执行cgi响应
	}

	//对外接口：3、文件夹展示和文件下载
	bool FileIsDir(RequestInfo &info){
	    if(info._st.st_mode & S_IFDIR){
	        return true;
	    }
	    return false;
	}
	bool FileHandler(RequestInfo &info)
	{
		//初始化文件响应信息
		InitResponse(info);

		//判断是否是目录
		if (FileIsDir(info)) {
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

#endif //!__UTILS_HPP__
