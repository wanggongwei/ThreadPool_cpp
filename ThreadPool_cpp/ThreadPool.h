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

#include "MyLog.h"
using namespace myLog;

const size_t MAX_THREAD_NUM = 1024;
const size_t MAX_TASK_NUM = 2;
const size_t MAX_THREAD_IDLE_TIME = 5;
/*
线程池项目，支持两种模式fixed、cached模式
*/
enum POOLMODE {
	FIXED,	//固定数量线程池
	CACHED	//可动态增长线程池
};
class myThread
{
public:
	myThread(std::function<void(size_t)> func);
	//启动线程
	void startThread();
	size_t getId() const;
	~myThread() = default;
private:

	std::function<void(size_t)> _func;	//线程函数
	size_t _threadId;	//记录所创建线程id
	static size_t _generateId;	//ID起始数
};
size_t myThread::_generateId = 0;
class ThreadPool
{
public:
	//构造函数
	ThreadPool();

	//删除拷贝构造和赋值操作
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

	//开启线程池
	void startPool(size_t);
	//设置线程池模式
	void setPoolMode(POOLMODE mode = FIXED);

	//提交任务到任务队列
	//使用模板，用户可以传入任意多参数
	//使用decltype推测返回值类型
	//使用future得到返回值
	template<typename Func, typename... Args>
	auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
	{
		using RTtype = decltype(func(args...));	//任务返回对象的类型
		//将任务封装
		auto task = std::make_shared<std::packaged_task<RTtype()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		std::future<RTtype> result = task->get_future();

		//对任务队列进行操作，先获取锁
		std::unique_lock<std::mutex> lockTaskQueue(taskMtx);
		//提交任务，如果队列满，等待1s，否则提交失败
		if (!_notFull.wait_for(lockTaskQueue, std::chrono::seconds(1), [&]()->bool
			{ return _taskQueue.size() < _maxTaskNum;
			}))
		{
			LOG(WARNING, "提交任务超时，提交失败。。。请重试");
			auto task = std::make_shared<std::packaged_task<RTtype()>>([]()->RTtype {return RTtype(); });

			(*task)();	//执行一下，返回空结果
			return task->get_future();
		}
		if (_mode == POOLMODE::CACHED) 
		{
			//创建新线程
			if (_curIdleThreadNum <_taskQueue.size() && _curThreadNum < _maxThreadNum)
			{
				auto ptr = std::make_unique<myThread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
				size_t id = ptr->getId(); 
				_threads.emplace(id, std::move(ptr));
				try 
				{
					_threads[id]->startThread();
					LOG(INFO, "线程启动： %d", id);
				}
				catch (const std::exception &e) 
				{
					LOG(WARNING, "线程启动失败，错误原因：%s",e.what());
				}
				_curThreadNum++;
				_curIdleThreadNum++;
			}
		}
		//任务入队列,封装成void返回值，无参数的方法，以适配_taskQueue的元素类型
		_taskQueue.emplace([task]() {(*task)(); });
		_notEempty.notify_all(); //通知线程可以取任务
		return result;	//返回任务结果
	}

	//析构函数
	~ThreadPool();
private:
	//线程函数，负责线程的执行
	void threadFunc(size_t threadId);

	POOLMODE _mode;	//记录线程池模式

	size_t _initThreadNum; //初始线程数量
	size_t _maxThreadNum;	//最大线程数量
	std::atomic_size_t _curThreadNum;	//当前总线程数量
	std::atomic_size_t _curIdleThreadNum; //当前空闲线程数量，非线程安全
	std::unordered_map<size_t, std::unique_ptr<myThread>> _threads;	//线程队列


	size_t _maxTaskNum; //任务队列最大数量
	std::queue<std::function<void()>> _taskQueue;	//任务队列，非线程安全，消费者和生产者可能同时访问


	std::mutex taskMtx;	//任务队列锁
	std::condition_variable _notEempty; //条件变量，进行线程通信
	std::condition_variable _notFull; //条件变量，进行线程通信
	std::condition_variable _exitCon;	//等待线程资源全部回收

	std::atomic_bool _isPoolRunning;	//线程池运行状态
};


//-----------------------------------Thread成员函数实现---------------------------
//构造函数
myThread::myThread(std::function<void(size_t)> func)
	:_func(func)
	, _threadId(_generateId++)
{}
void myThread::startThread()
{
	std::thread t(_func, _threadId);	// std::this_thread::get_id()
	//std::cout << "线程启动： " << std::this_thread::get_id() << std::endl;
	t.detach();
}
//返回自定义的线程ID
size_t myThread::getId() const {
	return _threadId;
}
//---------------------------------ThreadPool成员函数实现--------------------------
//构造函数
ThreadPool::ThreadPool()
	:_mode(FIXED)
	, _initThreadNum(0)
	, _maxThreadNum(MAX_THREAD_NUM)
	, _curThreadNum(0)
	, _curIdleThreadNum(0)
	, _maxTaskNum(MAX_TASK_NUM)
	, _isPoolRunning(false)
{}
ThreadPool::~ThreadPool() 
{
	_isPoolRunning = false;
	std::unique_lock<std::mutex> lock(taskMtx);
	_notEempty.notify_all();//唤醒在在等待任务中的线程
	_exitCon.wait(lock, [&]() {return _threads.size() == 0; });	//等待所有线程资源释放完成
}
//设置线程池模式
void ThreadPool::setPoolMode(POOLMODE mode)
{
	if (!_isPoolRunning)
	{
		_mode = mode;
	}
	else return;
}
//开启线程池
void ThreadPool::startPool(size_t  initThread = 4)	//默认启动4线程
{
	_isPoolRunning = true;
	_initThreadNum = initThread;
	_curIdleThreadNum =0;
	//添加线程
	for (size_t i = 0; i < _initThreadNum; i++)
	{
		auto ptr = std::make_unique<myThread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));	//构造线程对象指针，绑定线程函数
		size_t id = ptr->getId();
		_threads.emplace(id, std::move(ptr));	//线程入列
	}
	for (auto& thread : _threads)
	{
		try
		{
			thread.second->startThread();
			LOG(INFO, "线程%d启动", thread.second->getId());
			_curIdleThreadNum++;	//空闲线程++
			auto startTime = std::chrono::high_resolution_clock().now();	//更新空闲开始时间
		}
		catch (const std::exception& e)
		{
			LOG(WARNING, "线程%d启动失败，错误原因：%s", thread.second->getId(), e.what());
		}
	}
}
//线程函数，负责线程的执行
void ThreadPool::threadFunc(size_t threadId)
{
	//一直循环
	auto startTime = std::chrono::high_resolution_clock().now();	//更新空闲开始时间
	for(;;)
	{
		
		
		std::function<void()> task;
		{
			
			std::unique_lock<std::mutex> lockTaskQueue(taskMtx);
			LOG(INFO, "线程%d尝试获取任务",threadId);
			while (_taskQueue.size() == 0)
			{
				if (!_isPoolRunning && _taskQueue.empty())
				{
					_threads.erase(threadId);
					_curThreadNum--;
					_curIdleThreadNum--;
					LOG(INFO, "线程%d即将结束", threadId);
					_exitCon.notify_all();
					return;
				}
				//如果是cached模式，需要检查创建出的线程的空闲时间，如果超时，则析构
				if (_mode == POOLMODE::CACHED)
				{
					if (std::cv_status::timeout == _notEempty.wait_for(lockTaskQueue, std::chrono::seconds(1)))
					{
						auto nowTime = std::chrono::high_resolution_clock().now();
						auto idleTime = std::chrono::duration_cast<std::chrono::seconds>(nowTime - startTime);
						if (idleTime.count() >= (long long)MAX_THREAD_IDLE_TIME && _curThreadNum > _initThreadNum)
						{
							_threads.erase(threadId);
							_curThreadNum--;
							_curIdleThreadNum--;
							LOG(INFO, "动态创建的线程%d空闲超时，回收！", threadId);
							return;
						}
					}
				}
				else 
				{
					_notEempty.wait(lockTaskQueue);	//一直等待，直到队列中有任务
				}
			}
			//接取任务
			_curIdleThreadNum--;//空闲线程--
			task = _taskQueue.front();
			_taskQueue.pop();
			LOG(INFO, "线程%d获取任务成功", threadId);
			//如果还有任务，通知其他线程取任务
			if (_taskQueue.size() > 0)
			{
				_notEempty.notify_all();
			}
			_notFull.notify_all();	//通知生产者可以继续生产任务
		}
		//当前线程负责执行取到的任务
		//此时需要将锁释放，利用作用域自动释放锁
		if (task != nullptr)
		{
			//std::cout << "开始执行" << std::endl;
			task();	//执行function<void()>
		}
		//执行完成，空闲线程数量++
		_curIdleThreadNum++;
		startTime = std::chrono::high_resolution_clock().now();	//更新空闲开始时间
	}
}
