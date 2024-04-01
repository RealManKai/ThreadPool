#include"threadpool.h"
#include<functional>
#include<thread>
#include<iostream>
#include<memory>
#include<mutex>

const int TASK_MAX_THRESHOLD = 1024;//INT32_MAX
const int THREAD_MAX_SIZE = 10;//200
const int THREAD_MAX_IDLE_TIME = 10;//单位：秒//60

//线程池构造函数
ThreadPool::ThreadPool()
	:initThreadSize_(0)
	,threadSizeThreshold_(THREAD_MAX_SIZE)
	,idleThreadSize_(0)
	, curThreadSize_(0)
	,taskSize_(0)
	,taskQueMaxThreshold_(TASK_MAX_THRESHOLD)
	, poolMode_(PoolMode::MODE_FIXED)
	, isPoolRunning_(false)
{}

//线程池析构函数函数
ThreadPool::~ThreadPool()
{
	isPoolRunning_ = false;
	//notEmpty_.notify_all();
	//test
	std::cout << "**********" << curThreadSize_ << " " << threads_.size() << "**********" << std::endl;
	//等待所有线程结束返回(两种状态：1.正在执行，2.阻塞)
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	
	notEmpty_.notify_all();
	
	/*exitCond_.wait(lock, [&]()->bool {return curThreadSize_ == 0; });  //使用curThreadSize_进行判断的前提是要确保其正确性*/
	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
}

//设置线程池模式
void ThreadPool::setMode(PoolMode mode)
{
	if (checkRunningState())
		return;
	poolMode_ = mode;
}


//设置Task任务队列数量
void ThreadPool::set_taskQueMaxThreshold(int threshold)
{
	if (checkRunningState())
		return;
	//记录初始线程个数
	taskQueMaxThreshold_ = threshold;
	
}
//设置线程池Cache模式下最大线程数量
void ThreadPool::setThreadSizeThreshold(int threshold)
{
	if (checkRunningState())
		return;
	if (poolMode_ == PoolMode::MODE_CACHED)
	{
		threadSizeThreshold_ = threshold;
	}
}

//void submitTask(std::shared_ptr<Task> sp);
//给线程池提交任务：用户调用该接口，传入任务对象，生产任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	//获取锁
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	
	//线程的通信，等待任务队列有空余
	//while (taskQue_.size() == taskQueMaxThreshold_) {
	//	notFull_.wait(lock);//当前状态进入等待，此时需要notify(),且需要拿到mutex才可以恢复
	//}与下式同义
	
	//用户提交任务，最长不能阻塞超过1s,否则判断提交任务失败
	if (!notFull_.wait_for(lock, std::chrono::seconds(1)
		, [&]()->bool {return taskQue_.size() < (size_t)taskQueMaxThreshold_; })) 
	{
		//表示等待一秒钟条件依然未完成，
		std::cerr << "Task queue is full, submit task fail!" << std::endl;
		/*
		* 两种返回方式
		* 1.return task->getResult()
		* 2.return Result(task)
		第一种不可行的原因，是任务task完成后，task对象就被析构了，则此时用Result接收，也会被析构，所以不可行
		*/
		return Result(sp, false);
	}
	//如果有空余，把任务放入任务队列中
	taskQue_.emplace(sp);
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
	return Result(sp);
}


//开启线程池；
void ThreadPool::start(int initThreadSize)
{
	//设置线程池的启动状态
	isPoolRunning_ = true;
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;
	//创建线程对象
	for (int i = 0; i < initThreadSize_; i++) {
		//创建thread线程对象的时候把线程函数给到thread线程对象
		//把线程函数绑定到Thread对象，然后拿到ThreadPool的对象指针this
		auto ptr = new Thread(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
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
//定义线程函数
void ThreadPool::threadFunc(int threadid)
{
	auto lastDoTime = std::chrono::high_resolution_clock().now();
	for (;;)//while (isPoolRunning_)
	{
		std::shared_ptr<Task> task;//默认智能指针初始化为空
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
					curThreadSize_--;//此时结束循环，不需要维护正确性
					idleThreadSize_--;
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
			/*
			做两件事
				1.执行任务
				2.把任务的返回值通过setVal()方法给Result对象
			*/
			//task->run();
			task->exec();
		}
		//线程处理完任务，数量++
		idleThreadSize_++;
		lastDoTime = std::chrono::high_resolution_clock().now();//更新线程执行完任务的时间
	}
}

//检查线程池是否开启
bool ThreadPool::checkRunningState()const {
	return isPoolRunning_;
}


////////////////////////////
//启动线程
void Thread::start() {
	//创建一个线程来执行线程函数
	std::thread t(func_, threadId_);
	t.detach();//分离线程
}

int Thread::generateId_ = 0;

int Thread::getThreadId() const
{
	return threadId_;
}

//构造函数
Thread::Thread(ThreadFunc func)
	:func_(func)
	,threadId_(generateId_++)//使用成员变量接受线程函数，成员初始化列表来初始化类的成员变量 func_，将参数 func 的值传递给成员变量 func_。
{

}
//析构函数
Thread::~Thread(){}


Result::Result(std::shared_ptr<Task> task, bool isValid)
	: isValid_(isValid),
	task_(task) 
{
	task_->setResult(this);
}


//setVal()获取任务执行后的返回值
void Result::setVal(Any any)
{
	//存储task的返回值
	this->any_ = std::move(any);
	//已经获取的返回值，信号量加1
	sem_.post();

}

//getVal()用户调用获取task返回值
Any Result::get()
{
	if (!isValid_)
	{
		return " ";
	}
	sem_.wait();//task任务如果没有执行完，就阻塞线程
	return std::move(any_);
}

void Task::exec()
{
	//run();
	if(result_!=nullptr)
	{
		result_->setVal(run());//多态调用

	}
}

void Task::setResult(Result* res)
{
	result_ = res;
}

Task::Task() 
	:result_(nullptr)
{}