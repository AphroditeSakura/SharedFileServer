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

std::unordered_map<std::string, std::string> g_file_type = {
  {"txt", "apllication/octet-stream"},
  {"html", "text/html"},
  {"htm",  "text/htm"},
  {"jpg",  "image/jpeg"},
  {"zip",  "application/zip"},
  {"mp3",  "audio/mpeg"},
  {"unknow","apllication/octet-stream"}
};

class Utils
{
public:
	//字符串格式分隔
	static int Split(std::string &src, const std::string &seg, std::vector<std::string> &vec)
	{ 	
		//分隔数量
		int num = 0;
		//遍历计数
		size_t idx = 0;
		//目标位置
		size_t pos;
		
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
	//时间转换
	static void TimeToGMT(time_t t, std::string &gmt)
	{
		//gmtime将当前时间戳转换为格林威治时间，转换出来是结构体，
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
	static int64_t StrToDigit(std::string str)
	{
		int64_t num;
		std::stringstream ss;
		ss << str;
		ss >> num;
		return num;
	}
	static void MakeETag(int64_t size,int64_t ino,int64_t mtime ,std::string &etag)
	{
		std::stringstream ss;
		ss<<"\"";
		ss<<std::hex<<ino;
		ss<<"-";
		ss<<std::hex<<size;
		ss<<"-";
		ss<<std::hex<<mtime;
    	ss<<"\"";
		etag = ss.str();
	}
	static void GetMine(const std::string &file,std::string &mine)
	{
  		size_t pos = file.rfind(".");
    	if(pos == std::string::npos)
    	{
    		mine = g_file_type["unknow"];
    		return;
    	}

    std::string last_type = file.substr(pos+1);
    auto it = g_file_type.find(last_type);
    if(it == g_file_type.end())
    {
    	mine = g_file_type["unknow"];
    	return;
    }
    else
    	mine = it->second;
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
	std::tr1::unordered_map<std::string, std::string> _hdr_pair;//头部信息中的键值对
	struct stat _st;//文件类型

public:
	std::string _err_code;//错误码，404、400、500...
	void SetError(const std::string &code)
	{
		_err_code = code;
	}

	//判断请求类型
	bool RequestIsCGI()
	{
		if((_method == "POST") || ((_method == "GET" ) && (!_query_string.empty())))
		{
			return true;
		}
		return false;		
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
				//数据读取失败，认为是服务器端错误
				info.SetError("500");
				return false;
			}

			char* ptr = strstr(tmp, "\r\n\r\n");
			if ( ptr == NULL && (ret == MAX_HTTPHDR))
			{
				//请求的实体过大导致服务器无法处理，拒绝请求
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

	bool PathIsLegal(RequestInfo &info)
	{
    	//path:  /upload
    	//phys: ./www/upload
		std::string path = WWWROOT + info._path_info;
		if(stat(path.c_str(),&info._st) < 0)
		{
			info._err_code = "404";
			return false;
		}
		
    	char tmp[MAX_PATH] = {0};
    	realpath(path.c_str(),tmp);
		info._path_phys = tmp;
		
		if(info._path_phys.find(WWWROOT) == std::string::npos)
		{ 	
			//服务器理解请求客户端的请求，但是拒绝执行此请求
			info._err_code = "403";
			return false;
		}
		return true;
	}
	//解析首行
	bool ParseFirstLine(std::string &first_line, RequestInfo &info)
	{
		std::vector<std::string> line_list;
		if(Utils::Split(first_line, " ", line_list) != 3)
		{
			info._err_code = "400"; //客户端请求错误，服务器无法理解
			return false;
		}
		
		std::string url;
		info._method = line_list[0];
		url = line_list[1];
		info._version = line_list[2];
		
		if(info._method !="GET" && info._method != "POST"
		 && info._method != "HEAD")
		{
			info._err_code = "405"; //客户端请求中的方法被禁止
			return false;
		}
		
		if(info._version !="HTTP/0.9" &&
			 info._version != "HTTP/1.0" && 
				info._version != "HTTP/1.1")
		{	
			//客户端请求错误，服务端无法理解
			info._err_code = "400";
			return false;
		}
		
		//realpath:将一个路径转换为绝对路径，若地址不存在，就会造成段错误，
		size_t pos;
		pos = url.find("?");
		if(pos == std::string::npos)
			info._path_info = url;
		else{
			info._path_info = url.substr(0,pos);
			info._query_string = url.substr(pos+1);
		}
		
		return PathIsLegal(info);
	}
	//解析http请求头
	bool ParseHttpHeader(RequestInfo &info)
	{
		//以\r\n分割出字符串
		std::vector<std::string> hdr_vector;
		Utils::Split(_http_header,"\r\n",hdr_vector);
		
		if(ParseFirstLine(hdr_vector[0],info) == false)
			return false;

		for(size_t i = 1; i<hdr_vector.size() ; ++i)
		{
			size_t pos = hdr_vector[i].find(": ");
			info._hdr_pair[hdr_vector[i].substr(0,pos)] = hdr_vector[i].substr(pos+2);
		}
		return true;
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
	std::string _date; //系统响应时间
	std::string _fsize; //文件大小
	std::string _type_mine; //文件类型
public:
	HttpResponse(int sock) :_cli_sock(sock) {}
	//初始化请求的响应信息
	bool InitResponse(RequestInfo req_info)
	{
		//Last-Modify
		Utils::TimeToGMT(req_info._st.st_mtime,_mtime);
		Utils::MakeETag(req_info._st.st_size, req_info._st.st_ino,
					  req_info._st.st_mtime, _etag);
    
		time_t t = time(NULL);
		Utils::TimeToGMT(t,_date);

    	//文件大小
    	Utils::DigitToStr(req_info._st.st_size, _fsize);
    	
		//文件类型
    	Utils::GetMine(req_info._path_phys,_type_mine);
		return true;				
	}
	//文件下载功能
	bool ProcessFile(RequestInfo &info)
	{
		auto it = info._hdr_pair.find("If-Range");
    	if(it != info._hdr_pair.end()){
	        std::string if_range = it->second;
	        std::string etag_tmp = _etag;

	        if(etag_tmp == if_range){
				//执行断点续传功能
				if(BreakPointResume(info)){
				    return true;
				}
				info._err_code = "404";
				ErrHandler(info);
				return false;
	      	}
    	}

		//头部
		std::string rsp_header;
		rsp_header = info._version + " 200 OK\r\n";
		//决定服务端如何处理响应的数据
		rsp_header += "Content-Type: application/octet-stream;charset=UTF-8\r\n";
		//短链接
		rsp_header += "Content-Length: " + _fsize + "\r\n";
    	rsp_header += "Accept-Ranges: bytes\r\n";
		rsp_header += "ETag: "+ _etag + "\r\n";
		rsp_header += "Last-Modifyed: " + _mtime + "\r\n";
		rsp_header += "Date: "+ _date + "\r\n\r\n";
    
		//正常传输
		SendData(rsp_header);
		LOG("File rsp:[%s]\n", rsp_header.c_str());

		//文件数据 
		int fd = open(info._path_phys.c_str(), O_RDONLY);
		if (fd < 0)
		{
			info._err_code = "400";
			ErrHandler(info);
			return false;
		}

		int rlen = 0;
		char buf[MAX_BUFF];
		while((rlen = read(fd,buf,sizeof(buf))) > 0 )
		{
			send(_cli_sock,buf,rlen,0);
		}
		close(fd);
		return true;
	}
	//文件列表功能
	bool ProcessList(RequestInfo &info)
	{
		//头部
	    std::string rsp_header;
	    rsp_header = info._version + " 200 OK\r\n";

	    rsp_header += "Content-Type: text/html;charset=UTF-8\r\n";
	    rsp_header += "Connection: close\r\n";

	    if(info._version == "HTTP/1.1")
	    	rsp_header += "Transfer-Encoding: chunked\r\n";

	    rsp_header += "ETag: "+ _etag + "\r\n";
	    rsp_header += "Last-Modifyed: " + _mtime + "\r\n";
	    rsp_header += "Data: "+ _date + "\r\n\r\n";
	    
	    //正常传输
	    SendData(rsp_header);
	    LOG("File rsp:[%s]\n", rsp_header.c_str());

	    //采取分块传输
	    std::string rsp_body;
	    rsp_body = "<html><head>";
	    rsp_body += "<title>Welcome to my zone！" + info._path_info + "</title>";
	    rsp_body += "<meta charset='UTF-8'>";
	  
	    rsp_body += "</head><body>";
	    rsp_body += "<h1>Welcome to my zone！" + info._path_info + "</h1>"; 
	    
	    //新添加上传功能的显示信息
	    rsp_body += "<form action='/upload' method='POST' enctype='multipart/form-data'>";
	    //选择文件,name表示属性
	    rsp_body += "<input type='file' name='FileUpLoad' />";
	    //上传窗口，命名上传文件，默认submit
	    rsp_body += "<input type='submit' value='上传文件' />";
	    rsp_body += "</form>";
	    //横线
	    rsp_body += "<hr />";
	    //内部与<li>的作用构成有序列表
	    rsp_body += "<ol>";
	    SendChunkData(rsp_body);
	      
	    struct dirent **p_dirent = NULL;  //为了给二级指针存储内存，出现三级指针
	    //获取目录下的每一个文件，组织出html信息，chunk传输
	    int num = scandir(info._path_phys.c_str(), &p_dirent, 0, alphasort);

	    for(int i = 0; i<num; ++i)
	    {
			std::string file_html;

			std::string file; 
			file = info._path_phys + p_dirent[i]->d_name ; //文件全路径
			struct stat st;
			if(stat(file.c_str(), &st) < 0 )
				continue;

			std::string mtime; 
			Utils::TimeToGMT(st.st_mtime, mtime);
			std::string type_mine;
			Utils::GetMine(p_dirent[i]->d_name, type_mine);
			std::string fsize;
			//默认M改成kbytes；
			Utils::DigitToStr(st.st_size / 1024, fsize );

			file_html += "<li><strong><a href = '"+ info._path_info;
			file_html += p_dirent[i]->d_name ;
			file_html += "'>";
			file_html += p_dirent[i]->d_name ;
			file_html += "</a></strong><br /><small>";
			file_html += "Modified: " + mtime + "<br />";
			file_html += type_mine + "-" + fsize + " kbytes";
			file_html += "<br /><br /></small></li>";
			SendChunkData(file_html);
	    }
  
		rsp_body = "</ol><hr /></body></html>";  
		SendChunkData(rsp_body);
		SendChunkData(""); //分块传输最后为空，则返回\r\n\r\n,结束标识
    
		return true;
	}

	bool BreakPointResume(RequestInfo& info)
   {
    	std::string if_range = info._hdr_pair.find("If-Range")->second;
        auto it = info._hdr_pair.find("Range");
        if(it == info._hdr_pair.end()){
            return false;
        }
        std::string bytes = it->second;
        size_t pos = bytes.find("bytes=");
        size_t post = bytes.find('-');
        if(post == std::string::npos)
            return false;
        std::string start = bytes.substr(pos + 6, post - (pos + 6));
        std::string end = bytes.substr(post + 1);
        int64_t finnal;
        if(end.empty()){
            finnal = info._st.st_size - 1;
        }
        else{
            finnal = Utils::StrToDigit(end);
        }
        int64_t begin = Utils::StrToDigit(start);
        size_t byte = finnal - begin + 1;
        
        //组织html头部 
        std::string rsp_header = info._version + " 206 PARTY_CONTENT\r\n";
        rsp_header += "Content-Type: application/octet-stream\r\n"; 
        //标志文件是否被修改
        rsp_header += "Etag: " + _etag + "\r\n";
        //文件大小
        std::string slen;
        Utils::DigitToStr(byte, slen);
        rsp_header += "Content-Length: " + slen + "\r\n"; 
        int64_t fsize = info._st.st_size;
        Utils::DigitToStr(fsize, slen);

        Utils::DigitToStr(begin, start);
        Utils::DigitToStr(finnal, end);

        rsp_header += "Content-Range: bytes " + start + "-" + end + "/" + slen + "\r\n";
        rsp_header += "Accept-Ranges: bytes\r\n";
        rsp_header += "Last-Modified: " + _date + "\r\n\r\n";
        SendData(rsp_header);
        std::cout << rsp_header << std::endl;
        //发送文件
        int fp = open(info._path_phys.c_str(), O_RDONLY);
        if(fp < 0){
            info._err_code = "400";
            std::cerr << "open error!" << std::endl;
            return false;
        }
        lseek(fp, begin, SEEK_SET);
        //cerr << "seek: " << ftell(fp) << endl; 
        char tmp[MAX_BUFF];
        size_t clen = 0;
        size_t rlen = 0;
        while(clen < byte){
            int len = (byte - clen) > (MAX_BUFF-1) ? (MAX_BUFF-1) : (byte-clen);
            rlen = read(fp, tmp, len);
            clen += rlen;
            send(_cli_sock, tmp, rlen, 0);
        }
        close(fp);
        return true;
    }
	//CGI请求的处理
	bool ProcessCGI(RequestInfo &info)
	{
		int in[2];//父进程向子进程传输正文
		int out[2];//用于从子进程读取处理结果 
    
		if(pipe(in) || pipe(out))
		{
			info._err_code = "500";//服务器内部错误
			ErrHandler(info);
			return false; 
		}
		
		//开启子进程
		int pid = fork();
		if(pid < 0)
		{
			info._err_code = "500";
			ErrHandler(info);
			return false;
		}
		else if(pid == 0)
		{
			//setenv(const char *name,const char * value,int overwrite);
			//setenv()用来改变或增加环境变量的内容。参数name为环境变量名称字符串。
			//参数 value则为变量内容，参数overwrite用来决定是否要改变已存在的环境变量。
			//如果没有此环境变量则无论overwrite为何值均添加此环境变量。若环境变量存在，
			//当overwrite不为0时，原内容会被改为参数value所指的变量内容；
			//当overwrite为0时，则参数value会被忽略。返回值 执行成功则返回0，有错误发生时返回-1。
			//请求首行
			setenv("METHOD", info._method.c_str(),1);
			setenv("VERSION", info._version.c_str(),1);
			setenv("PATH_INFO", info._path_info.c_str(),1);
			setenv("QUERY_STRING", info._query_string.c_str(),1);
			
			//正文
			for(auto it = info._hdr_pair.begin(); it != info._hdr_pair.end(); ++it)
				setenv(it->first.c_str(), it->second.c_str(),1);

			close(in[1]);//关闭写
			close(out[0]);//关闭读
			//开源项目方法：
			//dup(oldfd):对传入的文件描述符进行复制，返回新的文件描述符
			//dup2(oldfd,newfd),newfd指定新的描述符的值，如果已经打开，则现将其关闭。
			dup2(in[0], 0); //将标准输入,重定向in[0],子进程直接在标准输入读取正文数据
			dup2(out[1], 1); //将标准输出，重定向out[1]，父进程读取正文，直接在标准输出读取就好，即子进程直接打印处理结果传递给父进程
			execl(info._path_phys.c_str(), info._path_phys.c_str(), NULL);//失败直接退出
			exit(0);
		}
		else  //父进程通过in管道将正文传给子进程，然后通过out管道读取子进程处理结果直到返回0，将处理结果组织http数据，响应客户端 
		{
			close(in[0]);
			close(out[1]);

			//父进程通过in管道将正文传给子进程，
			//通过out管道读取子进程的处理结果直到返回
			//将处理结果组织http数据，响应给客户端
			auto it  = info._hdr_pair.find("Content-Length");
			//为空表示没有，不需要提交正文数据给子进程
			if(it != info._hdr_pair.end())
			{
				char buf[MAX_BUFF] = {0};
				int64_t content_len = Utils::StrToDigit(it->second);
				int tlen = 0;
				while(tlen < content_len)
				{
					int len =  MAX_BUFF > (content_len - tlen)?(content_len - tlen): MAX_BUFF;
					int rlen = recv(_cli_sock,buf,len,0);
					if(rlen <= 0)
					{
						//响应错误给客户端
						return false;
					}
					if(write(in[1], buf, rlen) < 0)
						return false;
					tlen += rlen;
				}
			}
			//然后等待子进程处理，通过out管道读取子进程处理结果直到返回0，
			//将处理结果组织http数据，响应客户端
     
			//头部
			std::string rsp_header;
			rsp_header = info._version + " 200 OK\r\n";
 			//决定服务端如何处理响应的数据
			rsp_header += "Content-Type: text/html;charset=UTF-8\r\n";
			rsp_header += "Connection: close\r\n"; //短链接
			rsp_header += "ETag: "+ _etag + "\r\n";
			rsp_header += "Last-Modifyed: " + _mtime + "\r\n";
			rsp_header += "Date: "+ _date + "\r\n\r\n";
    
			//正常传输
			SendData(rsp_header);
			LOG("File rsp:[%s]\n",rsp_header.c_str());
   
			while(1)
			{
				char buf[MAX_BUFF] = {0};
				int rlen = read(out[0],buf,MAX_BUFF);
				if(rlen == 0)
					break;
				send(_cli_sock,buf,rlen,0);
				LOG("CGI rsp body:[%s]\n",buf);
			}

			close(in[1]);//关闭写
			close(out[0]);//关闭读
		}
		return true;
	}

	//对外接口：1、处理错误响应
	bool ErrHandler(RequestInfo &info)
	{
		//响应头信息
		std::string rsp_header;
		time_t t = time(NULL);
		std::string rsp_body;
		std::string gmt;
		std::string body_length;

		//组织正文
		rsp_body = "<html><body><h1>" + info._err_code;
		rsp_body += "<h1></body></html>";

		//组织头部
		rsp_header = info._version + " " + info._err_code + " ";
		rsp_header += Utils::GetErrDesc(info._err_code) + "\r\n";
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
    
	//普通传输文件
    bool SendData(const std::string &buf)
    {
      return send(_cli_sock, buf.c_str(), buf.length(), 0) < 0;
    }
	
	//分块传输
    bool SendChunkData(const std::string &buf)
    {
        if(buf.empty())
          return SendData("0\r\n\r\n");

        std::stringstream ss;
        ss << std::hex << buf.length();
        ss << "\r\n";

        SendData(ss.str());
        SendData(buf);
        SendData("\r\n");

        return true;
    }
	//对外接口：3、文件夹展示和文件下载
	bool FileIsDir(RequestInfo &info)
	{
		if(info._st.st_mode & S_IFDIR)
		{
			std::string path = info._path_info;
			if(path[path.length()-1] != '/')
			{
				info._path_info.push_back('/');
			}
			std::string phys = info._path_phys; 
			if(phys[phys.length()-1] != '/')
			{
				info._path_phys.push_back('/');
			}
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

//	bool ErrHandler(RequestInfo& info)//错误响应
//	{
//    	//响应头信息
//		std::string rsp_header;
//	
//		std::string rsp_body;
//		rsp_body = "<html><body><h1>"+info._err_code;
//		rsp_body += "<h1></body></html>";
//    
//    	//首行：版本 状态码 描述信息（code对应的描述信息）
//		rsp_header = info._version + " " + info._err_code + " ";
//		rsp_header += Utils::GetErrDesc(info._err_code)+"\r\n";	
//		time_t t = time(NULL);
//		std::string gmt;
//		Utils::TimeToGMT(t,gmt); 
//		rsp_header += "Date: "+gmt + "\r\n";	
//	
//		std::string content_length;
//		Utils::DigitToStr(rsp_body.size(),content_length);
//		rsp_header += "Content-Length: " + content_length + "\r\n\r\n";
//		
//    	//发送头部
//		send(_cli_sock,rsp_header.c_str(),rsp_header.size(),0);
//    	//发送正文
//		send(_cli_sock,rsp_body.c_str(),rsp_body.size(),0);
//		return true;
//	}
};

#endif //!__UTILS_HPP__
