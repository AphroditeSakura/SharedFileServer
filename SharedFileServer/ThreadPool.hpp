#include "Utils.hpp"


typedef bool(*Handler)(int sock);//���庯��ָ������
class HttpTask
{
	//http���������
	//����һ����Ա����socket����
	//����һ��������ӿ�
private:
	int _cli_sock;
	Handler TaskHandler;
public:
	HttpTask() :_cli_sock(-1) {}
	HttpTask(int sock, Handler handler) 
		:_cli_sock(sock),
		TaskHandler(handler){}

	void SetHttpTask(int sock, Handler handle) {
		_cli_sock = sock;
		TaskHandler = handle;
	}
	void Run() {
		TaskHandler(_cli_sock);
	}
};

class ThreadPool
{
	//�̳߳���
	//����ָ���������߳�
	//����һ���̰߳�ȫ���������
	//�ṩ���������кͳ��ӡ��̳߳����ٺͳ�ʼ��

private:
	int _max_thr;//��ǰ�̳߳ص�����߳�����
	int _cur_thr;//��ǰ�̳߳��е��߳��������ж��Ƿ��������
	std::queue<HttpTask> _task_queue;
	pthread_mutex_t _mutex;
	pthread_cond_t _cond;
	bool _is_stop;//�ж��Ƿ���ֹ�߳�

	//����̻߳�ȡ���񣬲�ִ������
	static void* thr_start(void *arg) 
	{
		pthread_detach(pthread_self());
		ThreadPool* tp = (ThreadPool*)arg;
		tp->QueueLock();
		while (tp->QueueEmpty())
		{
			tp->ThreadWait();
		}
		HttpTask ht;
		tp->PopTask(ht);
		tp->QueueUnLock();
		ht.Run();
		return NULL;
	}

	void QueueLock()
	{
		pthread_mutex_lock(&_mutex);
	}

	void QueueUnLock()
	{
		pthread_mutex_unlock(&_mutex);
	}

	bool IsStop()
	{
		return _is_stop;
	}

	void ThreadExit()
	{
		_cur_thr--;
		pthread_exit(NULL);
	}
	bool QueueEmpty()
	{
		return _task_queue.empty();
	}

	void ThreadWait()
	{
		if (IsStop())
		{
			//��Ҫ���٣�����ȴ�
			QueueUnLock();
			ThreadExit();
		}
		pthread_cond_wait(&_cond, &_mutex);
	}

	void ThreadWakeUpOne()
	{
		pthread_cond_signal(&_cond);
	}
	void ThreadWakeUpAll()
	{
		pthread_cond_broadcast(&_cond);
	}
public:
	ThreadPool(int max) :
		_max_thr(max),
		_cur_thr(0),
		_is_stop(false) {}

	//����̴߳�����������������������ʼ��
	bool ThreadPoolInit()
	{
		pthread_t tid;
		for (int i = 0; i < _max_thr; ++i)
		{
			int ret = pthread_create(&tid, NULL, thr_start, (void*)this);
			if (ret != 0)
			{
				LOG("create reror: %s\n", strerror(errno));
				return false;
			}
			_cur_thr++;
		}

		if (pthread_mutex_init(&_mutex, NULL) != 0) {
			LOG("init mutex error!\n");
			return false;
		}
		if (pthread_cond_init(&_cond, NULL) != 0) {
			LOG("init cond error!\n");
			return false;
		}
		return true;
	}
	//�̰߳�ȫ���������
	bool PushTask(HttpTask &tt) 
	{
		QueueLock();
		_task_queue.push(tt);
		QueueUnLock();
		ThreadWakeUpOne();
		return true;
	}

	//�̰߳�ȫ���������
	bool PopTask(HttpTask &tt)
	{
		//��Ϊ����������߳̽ӿ��е��ã������߳̽ӿ��ڳ���ǰ����м�������˴˴�����Ҫ����
		tt = _task_queue.front();
		_task_queue.pop();
		return true;
	}

	//�����̳߳�
	bool ThreadPoolStop() 
	{
		if (!IsStop())
			_is_stop = true;

		while (_cur_thr > 0)
		{
			ThreadWakeUpAll();//����������������ִ�У��������˳�
			usleep(1000);
		}
		return true;
	};

	~ThreadPool()
	{
		pthread_mutex_destroy(&_mutex);
		pthread_cond_destroy(&_cond);
	}
};