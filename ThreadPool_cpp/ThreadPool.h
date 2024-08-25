#pragma once
#include <iostream>
#include <mutex>
#include <memory>
#include <thread>
#include <functional>
#include <unordered_map>
#include <atomic>
#include <queue>
#include <future>
#include <chrono>

const size_t MAX_THREAD_NUM = 1024;
const size_t MAX_TASK_NUM = 1024;
/*
�̳߳���Ŀ��֧������ģʽfixed��cachedģʽ
*/
enum POOLMODE {
	FIXED,	//�̶������̳߳�
	CACHED	//�ɶ�̬�����̳߳�
};
class myThread
{
public:
	myThread(std::function<void(size_t)> func);
	//�����߳�
	void startThread();
	size_t getId() const;
	~myThread() = default;
private:

	std::function<void(size_t)> _func;	//�̺߳���
	size_t _threadId;	//��¼�������߳�id
	static size_t _generateId;	//ID��ʼ��
};
size_t myThread::_generateId = 0;
class ThreadPool
{
public:
	//���캯��
	ThreadPool();

	//ɾ����������͸�ֵ����
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

	//�����̳߳�
	void startPool(size_t);
	//�����̳߳�ģʽ
	void setPoolMode(POOLMODE mode = FIXED);

	//�ύ�����������
	//ʹ��ģ�壬�û����Դ�����������
	//ʹ��decltype�Ʋⷵ��ֵ����
	//ʹ��future�õ�����ֵ
	template<typename Func, typename... Args>
	auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
	{
		using RTtype = decltype(func(args...));	//���񷵻ض��������
		//�������װ
		auto task = std::make_shared<std::packaged_task<RTtype()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		std::future<RTtype> result = task->get_future();

		//��������н��в������Ȼ�ȡ��
		std::unique_lock<std::mutex> lockTaskQueue(taskMtx);
		//�ύ����������������ȴ�1s�������ύʧ��
		if (!_notFull.wait_for(lockTaskQueue, std::chrono::seconds(1), [&]()->bool
			{ return _taskQueue.size() < _maxTaskNum;
			}))
		{
			std::cerr << "�ύ����ʱ���ύʧ�ܡ�����������" << std::endl;
			auto task = std::make_shared<std::packaged_task<RTtype()>>([]()->RTtype {return RTtype(); });

			(*task)();	//ִ��һ�£����ؿս��
			return task->get_future();
		}
		if (_mode == POOLMODE::CACHED) 
		{
			//�������߳�
			if (_curIdleThreadNum <_taskQueue.size() && _curThreadNum < _maxThreadNum)
			{
				auto ptr = std::make_unique<myThread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
				int id = ptr->getId();
				_threads.emplace(id, std::move(ptr));
				try 
				{
					
					_threads[id]->startThread();
					std::cout << "�߳������� " << id << std::endl;

				
				}
				catch (const std::exception &e) 
				{
					std::cerr << "�߳�����ʧ�ܣ�����ԭ�� " << e.what() << std::endl;
				}
				_curThreadNum++;
				_curIdleThreadNum++;
			}
		}
		//���������,��װ��void����ֵ���޲����ķ�����������_taskQueue��Ԫ������
		_taskQueue.emplace([task]() {(*task)(); });

		_notEempty.notify_all(); //֪ͨ�߳̿���ȡ����

		return result;	//����������
	}

	//��������
	~ThreadPool() = default;
private:
	//�̺߳����������̵߳�ִ��
	void threadFunc(size_t threadId);

	POOLMODE _mode;	//��¼�̳߳�ģʽ

	size_t _initThreadNum; //��ʼ�߳�����
	size_t _maxThreadNum;	//����߳�����
	std::atomic_size_t _curThreadNum;	//��ǰ���߳�����
	std::atomic_size_t _curIdleThreadNum; //��ǰ�����߳����������̰߳�ȫ
	std::unordered_map<size_t, std::unique_ptr<myThread>> _threads;	//�̶߳���


	size_t _maxTaskNum; //��������������
	std::queue<std::function<void()>> _taskQueue;	//������У����̰߳�ȫ�������ߺ������߿���ͬʱ����


	std::mutex taskMtx;	//���������
	std::condition_variable _notEempty; //���������������߳�ͨ��
	std::condition_variable _notFull; //���������������߳�ͨ��

	std::atomic_bool _isPoolRunning;	//�̳߳�����״̬
};


//-----------------------------------Thread��Ա����ʵ��---------------------------
//���캯��
myThread::myThread(std::function<void(size_t)> func)
	:_func(func)
	, _threadId(_generateId++)

{}
void myThread::startThread()
{
	std::thread t(_func, _threadId);	// std::this_thread::get_id()
	//std::cout << "�߳������� " << std::this_thread::get_id() << std::endl;

	t.detach();
}
//�����Զ�����߳�ID
size_t myThread::getId() const {
	return _threadId;
}
//---------------------------------ThreadPool��Ա����ʵ��--------------------------
//���캯��
ThreadPool::ThreadPool()
	:_mode(FIXED)
	, _initThreadNum(0)
	, _maxThreadNum(MAX_THREAD_NUM)
	, _curThreadNum(0)
	, _curIdleThreadNum(0)
	, _maxTaskNum(MAX_TASK_NUM)
	, _isPoolRunning(false)
{
	//�̳߳�ʼ��
}
//�����̳߳�ģʽ
void ThreadPool::setPoolMode(POOLMODE mode)
{
	_mode = mode;
}
//�����̳߳�
void ThreadPool::startPool(size_t  initThread = 4)	//Ĭ������4�߳�
{
	_isPoolRunning = true;
	_initThreadNum = initThread;
	_curIdleThreadNum =0;
	//����߳�
	for (size_t i = 0; i < _initThreadNum; i++)
	{
		auto ptr = std::make_unique<myThread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));	//�����̶߳���ָ�룬���̺߳���
		size_t id = ptr->getId();
		_threads.emplace(id, std::move(ptr));	//�߳�����
	}
	for (auto& thread : _threads)
	{
		try
		{
			thread.second->startThread();
			std::cout << "�߳������� " << thread.second->getId() << std::endl;
			_curIdleThreadNum++;	//�����߳�++
		}
		catch (const std::exception& e)
		{
			std::cerr << "�����߳�ʧ��" << e.what() << std::endl;
		}
	}
}
//�̺߳����������̵߳�ִ��
void ThreadPool::threadFunc(size_t threadId)
{
	//һֱѭ��
	while (1)
	{
		std::function<void()> task;
		{
			std::unique_lock<std::mutex> lockTaskQueue(taskMtx);
			std::cout << "thread id: " << std::this_thread::get_id() << "���Ի�ȡ����" << std::endl;
			while (_taskQueue.size() == 0)
			{
				_notEempty.wait(lockTaskQueue);	//һֱ�ȴ���ֱ��������������
			}
			
			
			//��ȡ����
			_curIdleThreadNum--;//�����߳�--
			task = _taskQueue.front();
			_taskQueue.pop();
			std::cout << "thread id: " << std::this_thread::get_id() << "��ȡ����ɹ�" << std::endl;
			//�����������֪ͨ�����߳�ȡ����
			if (_taskQueue.size() > 0)
			{
				_notEempty.notify_all();
			}
			_notFull.notify_all();	//֪ͨ�����߿��Լ�����������
		}
		//��ǰ�̸߳���ִ��ȡ��������
		//��ʱ��Ҫ�����ͷţ������������Զ��ͷ���
		if (task != nullptr)
		{
			std::cout << "��ʼִ��" << std::endl;
			task();	//ִ��function<void()>
		}
		//ִ����ɣ������߳�����++
		_curIdleThreadNum++;
	}
}
