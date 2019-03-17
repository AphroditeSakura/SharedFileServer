#include "ThreadPool.hpp"

#define MAX_LISTEN 10
#define MAX_THREAD 8

class HttpServer
{
	//����һ��tcp����˳���,����������
	//Ϊ��������֯һ���̳߳�������������̳߳���


private:
	int _serv_sock;
	ThreadPool *_tp;
	//http����Ĵ�����
	static bool HttpHandler(int sock) 
	{
		LOG("into HttpHandler!!\n");
		RequestInfo _info;
		HttpRequest _req(sock);
		HttpResponse _rsp(sock);
		//����httpͷ��
		if (_req.RecvHttpHeader(_info) == false)
		{
			goto out;
		}

		//����httpͷ��
		if (_req.ParseHttpHeader(_info) == false)
		{
			goto out;
		}

		//�ж��Ƿ���CGI����
		if (_info.RequestIsCGI()) 
		{
			//�����CGI������ִ��CGI��Ӧ
			_rsp.CGIHandler(_info);
		}
		else {
			//�������CGI������ִ��Ŀ¼�б�/�ļ�����
			_rsp.FileHandler(_info);
		}
		_info._error_code = "404";
		_rsp.ErrHandler(_info);
		close(sock);
		return true;
	out:
		_rsp.ErrHandler(_info);
		close(sock);
		return false;
	}
public:
	HttpServer() :_serv_sock(-1), _tp(NULL) {}
	//���TCP����˳���ĳ�ʼ�����̳߳صĳ�ʼ��
	bool HttpServerInit(std::string ip, int port) {
		_serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

		if(_serv_sock < 0) {
			LOG("sock error : %s\n", strerror(errno));
			return false;
		}

		sockaddr_in lst_addr;
		lst_addr.sin_family = AF_INET;
		lst_addr.sin_port = htons(port);
		lst_addr.sin_addr.s_addr = inet_addr(ip.c_str());
		socklen_t len = sizeof(sockaddr_in);

		if (bind(_serv_sock, (sockaddr*)&lst_addr, len) < 0)
		{
			LOG("bind socket error:%s\n", strerror(errno));
			close(_serv_sock);
			return false;
		}

		if (listen(_serv_sock, MAX_LISTEN) < 0)
		{
			LOG("listen socket error:%s\n", strerror(errno));
			close(_serv_sock);
			return false;
		}

		_tp = new ThreadPool(MAX_THREAD);
		if (_tp == NULL) 
		{
			LOG("thread pool malloc error!\n");
			return false;
		}

		if (_tp->ThreadPoolInit() == false) 
		{
			LOG("thread pool init error!\n");
			return false;
		}
		return true;
	}

	//��ʼ��ȡ�ͻ��������ӡ������������������
	bool Start()
	{
		while (true) 
		{
			//��ȡ�ͻ���������
			sockaddr_in cli_addr;
			socklen_t len = sizeof(sockaddr_in);
			int cli_sock = accept(_serv_sock, (sockaddr*)&cli_addr, &len);

			if (cli_sock < 0) 
			{
				LOG("accept error: %s", strerror(errno));
				continue;
			}
			LOG("new client!!\n");
			//��������
			HttpTask ht;
			ht.SetHttpTask(cli_sock, HttpHandler);

			//���������
			_tp->PushTask(ht);
		}
		return true;
	}
};


class UpLoad
{
	//�ļ��ϴ����ܴ���ӿ�
};


int main(int argc, char *argv[])
{
	std::string ip = argv[1];
	int port = atoi(argv[2]);
	HttpServer hs;
	hs.HttpServerInit(ip, port);
	hs.Start();
	return 0;
}