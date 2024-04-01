#pragma once
#include<iostream>
#include<vector>
#include<queue>
#include<memory>
#include<atomic>
#include<mutex>
#include<condition_variable>
#include<functional>
#include<thread>
#include<unordered_map>
#include<future>

const int TASK_MAX_THRESHOLD = 1024;//INT32_MAX
const int THREAD_MAX_SIZE = 10;//200
const int THREAD_MAX_IDLE_TIME = 10;//单位：秒//60

//线程池支持的模式
enum class PoolMode {  //enum 是一种用来定义枚举类型的关键字。枚举类型允许您定义一组命名的整数常量，这些常量在枚举类型的范围内具有唯一的标识符。
	MODE_FIXED,
	MODE_CACHED,
};

class Thread {
	//线程类型
public:
	//定义一个线程函数对象类型
	using ThreadFunc = std::function<void(int)>;

	//构造函数
	Thread(ThreadFunc func)
		:func_(func)
		, threadId_(generateId_++)//使用成员变量接受线程函数，成员初始化列表来初始化类的成员变量 func_，将参数 func 的值传递给成员变量 func_。
	{}
	//析构函数
	~Thread() = default;
	//启动线程
	void start()
	{
		//创建一个线程来执行线程函数
		std::thread t(func_, threadId_);
		t.detach();//分离线程
	}
	//获取线程ID
	int getThreadId() const
	{
		return threadId_;
	}

private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_;//保存线程ID

};

int Thread::generateId_ = 0;

//线程池类型
class ThreadPool {

public:
	//线程池构造函数
	ThreadPool()
		:initThreadSize_(0)
		, threadSizeThreshold_(THREAD_MAX_SIZE)
		, idleThreadSize_(0)
		, curThreadSize_(0)
		, taskSize_(0)
		, taskQueMaxThreshold_(TASK_MAX_THRESHOLD)
		, poolMode_(PoolMode::MODE_FIXED)
		, isPoolRunning_(false)
	{}

	//线程池析构函数函数
	~ThreadPool()
	{
		isPoolRunning_ = false;
		std::cout << "**********" << curThreadSize_ << " " << threads_.size() << "**********" << std::endl;
		//等待所有线程结束返回(两种状态：1.正在执行，2.阻塞)
		std::unique_lock<std::mutex> lock(taskQueMtx_);

		notEmpty_.notify_all();
		exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
	}

	//设置Task任务队列数量
	void set_taskQueMaxThreshold(int threshold)
	{
		if (checkRunningState())
			return;
		//记录初始线程个数
		taskQueMaxThreshold_ = threshold;
	}

	//设置线程池Cache模式下最大线程数量
	void setThreadSizeThreshold(int threshold)
	{
		if (checkRunningState())
			return;
		if (poolMode_ == PoolMode::MODE_CACHED)
		{
			threadSizeThreshold_ = threshold;
		}
	}
	//给线程池提交任务
	template<typename Func,typename... Args>
	auto submitTask(Func&& func, Args&&...args) -> std::future<decltype(func(args...))>
	{
		//打包任务，放到任务队列里
		using RType = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<RType()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		std::future<RType> Result = task->get_future();

		//获取锁
		std::unique_lock<std::mutex> lock(taskQueMtx_);

		//用户提交任务，最长不能阻塞超过1s,否则判断提交任务失败
		if (!notFull_.wait_for(lock, std::chrono::seconds(1)
			, [&]()->bool {return taskQue_.size() < (size_t)taskQueMaxThreshold_; }))
		{
			//表示等待一秒钟条件依然未完成，
			std::cerr << "Task queue is full, submit task fail!" << std::endl;
			auto task = std::make_shared<std::packaged_task<RType()>>(
				[]()->RType {return RType(); });
			return task->get_future();
		}
		//如果有空余，把任务放入任务队列中
		//taskQue_.emplace(sp);
		//using Task = std::function<void()>;
		//此时Task对象为空对象，使用lambda表达式包装中间层
		taskQue_.emplace([task]() {(*task)(); });

		taskSize_++;
		//新放了任务，任务队列不空了，此时再notEmpty_进行通知
		notEmpty_.notify_all();

		//Cache模式（场景：任务处理比较紧急，小而快的任务）需要根据任务数量和空闲线程的数量，判断是否需要创建新的线程
		if (poolMode_ == PoolMode::MODE_CACHED
			&& taskSize_ > idleThreadSize_
			&& curThreadSize_ < threadSizeThreshold_)
		{
			std::cout << "new thread" << std::this_thread::get_id() << "<<<<<<<<<<" << std::endl;
			//创建新线程
			auto ptr = new Thread(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			//threads_.emplace_back(std::move(ptr));//与 push_back() 函数类似，但是 emplace_back() 可以接受任意数量的参数，并将它们传递给元素类型的构造函数，从而在原地构造元素，而不需要额外的拷贝或移动操作。
			int tId = ptr->getThreadId();
			threads_.emplace(tId, std::move(ptr));
			threads_[tId]->start();//启动线程
			//修改线程相关变量
			idleThreadSize_++;
			curThreadSize_++;
		}
		//返回任务的Result对象
		return Result;
	}
	//设置线程池模式
	void setMode(PoolMode mode)
	{
		if (checkRunningState())
			return;
		poolMode_ = mode;
	}

	//开启线程池；
	void start(int initThreadSize = std::thread::hardware_concurrency())//hardware_concurrency()系统核心数量
	{
		//设置线程池的启动状态
		isPoolRunning_ = true;
		initThreadSize_ = initThreadSize;
		curThreadSize_ = initThreadSize;
		//创建线程对象
		for (int i = 0; i < initThreadSize_; i++) {
			//创建thread线程对象的时候把线程函数给到thread线程对象
			//把线程函数绑定到Thread对象，然后拿到ThreadPool的对象指针this
			auto ptr = new Thread(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			//threads_.emplace_back(std::move(ptr));//与 push_back() 函数类似，但是 emplace_back() 可以接受任意数量的参数，并将它们传递给元素类型的构造函数，从而在原地构造元素，而不需要额外的拷贝或移动操作。
			int threadId = ptr->getThreadId();
			threads_.emplace(threadId, std::move(ptr));
		}
		//启动所有线程 std::vector<Thread*> thread_
		for (int i = 0; i < initThreadSize_; i++) {
			threads_[i]->start();//需要去执行一个线程函数
			idleThreadSize_++;//记录初始空闲线程数量
		}
	}

	//禁止线程池进行拷贝构造和赋值
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator= (const ThreadPool&) = delete;

private:

	//线程函数
	void threadFunc(int threadid)
	{
		auto lastDoTime = std::chrono::high_resolution_clock().now();
		for (;;)//while (isPoolRunning_)
		{
			Task task;//默认智能指针初始化为空
			//创建一个局部作用域，在拿走任务后就把锁释放掉（自动析构）
			{
				std::cout << "尝试获取任务！" << "tid：" << std::this_thread::get_id() << std::endl;
				//先获取锁
				std::unique_lock<std::mutex> lock(taskQueMtx_);
				//cache模式下,可能创建了很多线程，但是空闲时间超过60s，应该把多余的线程结束回收？
					//结束前回收（超过initThreadSize_数量的线程要回收掉）
					//当前时间-上一次线程执行时间 > 60S
				//锁+双重判断
				while (taskQue_.size() == 0)
				{
					////线程池要结束，回收线程资源
					if (!isPoolRunning_) {
						//threadid==>thread对象==>删除
						threads_.erase(threadid);
						exitCond_.notify_all();
						std::cout << "thread" << std::this_thread::get_id() << "撤销!" << std::endl;
						return;
					}

					if (poolMode_ == PoolMode::MODE_CACHED)
					{
						//每一秒钟返回一次，怎么区分超时返回还是有任务待返回
							//条件变量，超时返回了
						if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
						{
							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastDoTime);
							if (dur.count() >= THREAD_MAX_IDLE_TIME
								&& curThreadSize_ > initThreadSize_)
							{
								//回收线程
								//记录线程相关数量的值--
								//线程列表中的线程对象删除(难点是没办法确定threadFunc 绑定的是哪个线程对象)
								threads_.erase(threadid);
								curThreadSize_--;
								idleThreadSize_--;
								std::cout << "thread" << std::this_thread::get_id() << "撤销!" << std::endl;
								return;
							}
						}
					}
					else
					{
						//等待notEmpty条件,任务队列不为空
						notEmpty_.wait(lock);
					}
				}
				//线程被分配任务，数量++
				idleThreadSize_--;
				//从任务队列中取一个任务出来
				//std::shared_ptr<Task> task = taskQue_.front();
				task = taskQue_.front();
				//任务--
				taskQue_.pop();
				taskSize_--;
				//通知生产任务
				//如果依然有剩余任务，继续通知其他线程执行任务
				if (taskQue_.size() > 0)
				{
					notEmpty_.notify_all();
				}
				//拿出任务进行通知
				notFull_.notify_all();
			}
			std::cout << "获取任务成功！" << "tid：" << std::this_thread::get_id() << std::endl;
			//当前线程负责执行这个任务
			if (task != nullptr)
			{
				task();//执行function<void()>
			}
			//线程处理完任务，数量++
			idleThreadSize_++;
			lastDoTime = std::chrono::high_resolution_clock().now();//更新线程执行完任务的时间
		}
	}
	//检查pool的运行状态
	bool checkRunningState()const
	{
		return isPoolRunning_;
	}

private:
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;

	int initThreadSize_;//初始的线程数量
	int threadSizeThreshold_;//线程上限数量
	std::atomic_int curThreadSize_;//当前线程池线程的数量
	std::atomic_int idleThreadSize_;//记录线程空闲时间的数量
	
	using Task = std::function<void()>;
	std::queue<Task> taskQue_;//任务队列
	std::atomic_uint taskSize_; //任务数量
	int taskQueMaxThreshold_;//任务对象数量上限阈值

	std::mutex taskQueMtx_;//保证任务队列的线程安全
	std::condition_variable  notFull_;//保证任务队列不满
	std::condition_variable notEmpty_;//保证任务队列不空
	std::condition_variable exitCond_;//等到线程资源全部回收

	PoolMode poolMode_;//当前线程池的工作模式
	std::atomic_bool isPoolRunning_;//当前线程池的启动状态
};
