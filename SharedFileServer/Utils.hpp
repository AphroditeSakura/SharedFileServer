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

//�������ͷ�ļ�����������
#include <tr1/unordered_map>
#include <unordered_map>

#define LOG(...) do{fprintf(stdout,__VA_ARGS__);fflush(stdout);}while(0) //ָ��д��ĳĿ¼ 

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
	//�ַ�����ʽ�ָ�
	static int Split(std::string &src, const std::string &seg, std::vector<std::string> &vec)
	{
		int num = 0; //�ָ�����
		size_t idx = 0;//��������
		size_t pos; //Ŀ��λ��
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

	//ͨ���������ȡ������Ϣ
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
		//gmtime����ǰʱ���ת��Ϊ��������ʱ�䣬ת����������һ���ṹ�壬
		//�ýṹ��������������ʱ���룬һ�ܵĵ�һ�죬һ��ĵڼ������Ϣ
		struct tm *mt = gmtime(&t);
		char tmp[128] = { 0 };
		//���ṹ���ʱ�䰴��һ����ʽ��ת��Ϊ�ض�ʱ��,�����ַ���ʵ�ʳ���
		int len = strftime(tmp, 127, "%a %d %b %Y %H:%M:%S GMT", mt);
		gmt.assign(tmp, len);
	}

	//������תΪ�ַ���,�����ҵ�Java��ѽ....
	static void DigitToStr(int64_t num, std::string &str)
	{
		std::stringstream ss;
		ss << num;
		str = ss.str();
	}

	//���ַ���תΪ����,�����ҵ�Java��ѽ....
	static int64_t StrToDigit(std::string &str)
	{
		int64_t num;
		std::stringstream ss;
		ss << str;
		ss >> num;
		return num;
	}
};

class RequestInfo
{
	//����HttpRequest��������������Ϣ
public:
	std::string _method;//���󷽷�
	std::string _version;//Э��汾
	std::string _path_info;//��Դ·��
	std::string _path_phys;//������Դ��ʵ��·��
	std::string _query_string;//��ѯ���ַ���
	std::tr1::unordered_map<std::string, std::string> _hdr_list;//ͷ����Ϣ�еļ�ֵ��
	struct stat _st;//�ļ�����

	//�ж��Ƿ���CGI����
	bool RequestIsCGI()
	{
		return true;
	}

public:
	std::string _error_code;//�����룬404��400��500...
	void SetError(const std::string &code)
	{
		_error_code = code;
	}
};


class HttpRequest
{
	//http���ݵĽ��սӿ�
	//http���ݵĽ����ӿ�
	//�����ṩ�ܹ���ȡ�������Ľӿ�
private:
	int _cli_sock;
	std::string _http_header;
	RequestInfo _req_info;

public:
	HttpRequest(int sock) :_cli_sock(sock) {}
	//����Http����ͷ
	bool RecvHttpHeader(RequestInfo &info)
	{	
		LOG("into RecvHttpHeader!!\n");
		//�����ݷŵ�������
		char tmp[MAX_HTTPHDR] = { 0 };
		while (true)
		{
			int ret = recv(_cli_sock, tmp, MAX_HTTPHDR, 0);
			if (ret <= 0)
			{
				//EINTR������δ��������Ϊ���źŴ��
				//��������ʽ EAGAIN
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
			//��ȡheader_len��ô�����ַ���
			_http_header.assign(tmp, header_len);
			//��ͷ���Ƴ�����ʵ���Ƕ���
			recv(_cli_sock, tmp, header_len + 4, 0);
			LOG("header:[%s]\n", _http_header.c_str());
			break;
		}
		
		return true;
	}
	//����http����ͷ
	bool ParseHttpHeader(RequestInfo &info)
	{

	}
	//�����ṩ�������
	RequestInfo& GetRequestInfo() 
	{
		
	}
};

class HttpResponse
{
	//�ṩ�ļ���������ļ����ء��б��ܣ��ӿ�
	//CGI�����ļ��ϴ����ӿ�
private:
	int _cli_sock;
	std::string _etag;//�����ļ��Ƿ��޸Ĺ�
	std::string _mtime;//�ļ������һ���޸�ʱ��
	std::string _cont_len;//�ļ���С
public:
	HttpResponse(int sock) :_cli_sock(sock) {}
	bool InitResponse(RequestInfo req_info);//��ʼ���������Ӧ��Ϣ
	bool ProcessFile(RequestInfo &info);//�ļ����ع���
	bool ProcessList(RequestInfo &info);//�ļ��б���
	bool ProcessCGI(RequestInfo &info);//CGI����Ĵ���

	//����ӿڣ�1�����������Ӧ
	bool ErrHandler(RequestInfo &info)
	{
		//��Ӧͷ��Ϣ
		std::string rsp_header;

		//���� �汾  ״̬�� ״̬����\r\n
		//ͷ�� ���ȣ� Content-Length    ��ǰϵͳʱ��:Date
		//����             
		//���� rsp_body = "<html><body><h1> 404; <h1></body></html>"  
		//һ�����û������ �еĻ���html��ǩ  ���ı�ǩ ����

		//��Ӧ���� <html><body><h1> 404;<h1></body></html>
		//���У��汾 ״̬�� ������Ϣ��code��Ӧ��������Ϣ��
		time_t t = time(NULL);
		std::string rsp_body;
		std::string gmt;
		std::string body_length;

		//��֯����
		rsp_body = "<html><body><h1>" + info._error_code;
		rsp_body += "<h1></body></html>";

		//��֯ͷ��
		rsp_header = info._version + " " + info._error_code + " ";
		rsp_header += Utils::GetErrDesc(info._error_code) + "\r\n";
		Utils::TimeToGMT(t, gmt);
		rsp_header += "Date: " + gmt + "\r\n";
		Utils::DigitToStr(rsp_body.length(), body_length);
		rsp_header += "Content-Length: " + body_length+"\r\n\r\n";

		//����ͷ��������
		send(_cli_sock, rsp_header.c_str(), rsp_header.length(), 0);
		send(_cli_sock, rsp_body.c_str(), rsp_body.length(), 0);
		return true;
	}

	//����ӿڣ�2���ļ��ϴ�
	bool CGIHandler(RequestInfo &info)
	{
		InitResponse(info);//��ʼ��cgi��Ӧ��Ϣ
		ProcessCGI(info);//ִ��cgi��Ӧ
	}

	//����ӿڣ�3���ļ���չʾ���ļ�����
	//TODO ¼��0��42��19
	bool FileHandler(RequestInfo &info)
	{
		//��ʼ���ļ���Ӧ��Ϣ
		InitResponse(info);

		//�ж��Ƿ���Ŀ¼
		if (DIR) {
			//�ļ���չʾ
			ProcessList(info);
		}
		else
		{
			//�ļ�����
			ProcessFile(info);
			
		}
	}

};

#endif //!__UTILS_HPP__