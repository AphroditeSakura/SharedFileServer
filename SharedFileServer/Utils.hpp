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

//�������ͷ�ļ�����������
#include <tr1/unordered_map>

#define LOG(...) do{fprintf(stdout,__VA_ARGS__);fflush(stdout);}while(0) //ָ��д��ĳĿ¼ 

#define MAX_HTTPHDR 4096	//4k

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
		//�����ݷŵ�������
		char tmp[MAX_HTTPHDR];
		while (true)
		{
			int ret = recv(_cli_sock, tmp, MAX_HTTPHDR, 0);
			if (ret <= 0)
			{
				//EINTR������δ��������Ϊ���źŴ��
				//��������ʽ EAGAIN
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

	}

	//����ӿڣ�2���ļ��ϴ�
	bool CGIHandler(RequestInfo &info)
	{
		InitResponse(info);//��ʼ��cgi��Ӧ��Ϣ
		ProcessCGI(info);//ִ��cgi��Ӧ
	}

	//����ӿڣ�3���ļ���չʾ���ļ�����
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
