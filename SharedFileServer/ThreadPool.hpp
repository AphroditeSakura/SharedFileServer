#include "Utils.hpp"


typedef bool(*Handler)(int sock);//定义函数指针类型
class HttpTask
{
	//http请求的任务
	//包含一个成员就是socket连接
	//包含一个任务处理接口
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
	//线程池类
	//创建指定数量的线程
	//创建一个线程安全的任务队列
	//提供任务的入队列和出队、线程池销毁和初始化

private:
	int _max_thr;//当前线程池的最大线程数量
	int _cur_thr;//当前线程池中的线程数量，判断是否可以销毁
	std::queue<HttpTask> _task_queue;
	pthread_mutex_t _mutex;
	pthread_cond_t _cond;
	bool _is_stop;//判断是否终止线程

	//完成线程获取任务，并执行任务
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
			//若要销毁，无需等待
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

	//完成线程创建，互斥锁，条件变量初始化
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
	//线程安全的任务入队
	bool PushTask(HttpTask &tt) 
	{
		QueueLock();
		_task_queue.push(tt);
		QueueUnLock();
		ThreadWakeUpOne();
		return true;
	}

	//线程安全的任务出队
	bool PopTask(HttpTask &tt)
	{
		//因为任务出队在线程接口中调用，但是线程接口在出队前会进行加锁，因此此处不需要加锁
		tt = _task_queue.front();
		_task_queue.pop();
		return true;
	}

	//销毁线程池
	bool ThreadPoolStop() 
	{
		if (!IsStop())
			_is_stop = true;

		while (_cur_thr > 0)
		{
			ThreadWakeUpAll();//唤醒所所有有任务执行，无任务退出
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