0#ifndef THREADPOOL_H
#define THREADPOOL_H
#include<iostream>
#include<vector>
#include<queue>
#include<memory>
#include<atomic>
#include<mutex>
#include<condition_variable>
#include<functional>
#include<unordered_map>

//定义上帝类，接收任意返回值类型
class Any 
{
public:
	Any() = default;
	~Any() = default;
	//左值的拷贝构造和赋值
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	//右值的拷贝构造和赋值
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;
	//构造函数可以让Any类型接收任意其他数据
	template<typename T>
	Any(T data) : base_(std::make_unique<Derive<T>>(data))
	{}
	//把Any对象里边存储的data数据提取出来
	template<typename T>
	T cast_()
	{
		// 我们怎么从base_找到它所指向的Derive对象，从它里面取出data成员变量
		// 基类指针 =》 派生类指针   RTTI
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr)
		{
			std::cout << "error" << std::endl;
			throw "type is unmatch!";
		}
		return pd->data_;
	}
private:
	//基类
	class Base 
	{
	public:
		virtual ~Base() = default;
	};
	template<typename T>
	class Derive :public Base 
	{
	public:
		Derive(T data) :data_(data)
		{}
		T data_;
	};
private:
	//定义基类指针
	std::unique_ptr<Base> base_;
};

//定义信号量
class Semaphore {
public:
	Semaphore(int limit=0)
		:resLimit_(limit)
	{}
	~Semaphore() = default;
	//--
	void wait() 
	{
		std::unique_lock<std::mutex> lock(mtx_);
		//等待信号量资源，否则阻塞
		con_.wait(lock,[&]()->bool {return resLimit_ > 0; });
		resLimit_--;
	}
	//++
	void post()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		resLimit_++;
		con_.notify_all();// 等待状态，释放mutex锁 通知条件变量wait的地方，可以起来干活了
	}

private:
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable con_;

};

//Task声明
class Task;

//Result，将线程返回的结果转换成任务返回的结果
//接收线程返回的结果
class Result {
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	~Result() = default;
	//setVal()获取任务执行后的返回值
	void setVal(Any any);

	//getVal()用户调用获取task返回值
	Any get();

private:
	Any any_;//存储任务的返回值
	Semaphore sem_;//线程通信信号量
	std::shared_ptr<Task> task_;//指向对应获取返回值的任务对象
	std::atomic_bool isValid_;//返回值是否有效



};


//任务抽象基类
//用户可以自定义任意任务类型，从Task继承，从写run()方法，实现自定义任务处理
class Task {
public:
	Task();
	~Task() = default;
	void exec();
	void setResult(Result* res);
	// 用户可以自定义任意任务类型，从Task继承，重写run方法，实现自定义任务处理
	virtual Any run() = 0;
private:
	Result* result_;//Result的周期比task对象长，不需要用强智能指针
};


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
	//启动线程
	void start();
	//构造函数
	 Thread(ThreadFunc func);
	//析构函数
	 ~Thread();
	 //获取线程ID
	 int getThreadId() const;

private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_;//保存线程ID

};

/*
* example：
ThreadPool pool
pool.start()
class MyTask:public Task{
	public:
		void run();
}
pool.submitTask(std::make_shared<MyTask>());

std::make_shared 函数模板接受任意数量的参数，
并将它们传递给 MyTask 类型的构造函数，然后返回
一个 std::shared_ptr 智能指针，该指针指向一个动
态分配的 MyTask 类型对象。
std::make_shared的好处是可以让对象的内存和引用基数的内存进行绑定自动释放，防止内存泄露
*/


//线程池类型
class ThreadPool {

public:
	//线程池构造函数
	ThreadPool();

	//线程池析构函数函数
	~ThreadPool();

	//设置Task任务队列数量
	void set_taskQueMaxThreshold(int threshold);

	//设置线程池Cache模式下最大线程数量
	void setThreadSizeThreshold(int threshold);

	//给线程池提交任务
	Result submitTask(std::shared_ptr<Task> sp);

	//设置线程池模式
	void setMode(PoolMode mode);

	//开启线程池；
	void start(int initThreadSize = std::thread::hardware_concurrency());//hardware_concurrency()系统核心数量

	//线程函数
	void threadFunc(int threadid);

	//禁止线程池进行拷贝构造和赋值
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator= (const ThreadPool&) = delete;
	/*
	ThreadPool&：这是一个引用类型，表示对一个已存在的 ThreadPool 对象的引用。
	&ThreadPool：这是一个指针类型，表示一个指向 ThreadPool 对象的指针。
	*/
	bool checkRunningState()const;

private:
	//std::vector<std::unique_ptr<Thread>> threads_; //线程列表
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;

	size_t initThreadSize_;//初始的线程数量
	int threadSizeThreshold_;//线程上限数量
	std::atomic_int curThreadSize_;//当前线程池线程的数量
	std::atomic_int idleThreadSize_;//记录线程空闲时间的数量
	
	std::queue<std::shared_ptr<Task>> taskQue_;//任务队列
	std::atomic_uint taskSize_; //任务数量
	int taskQueMaxThreshold_;//任务对象数量上限阈值

	std::mutex taskQueMtx_;//保证任务队列的线程安全
	std::condition_variable  notFull_;//保证任务队列不满
	std::condition_variable notEmpty_;//保证任务队列不空
	std::condition_variable exitCond_;//等到线程资源全部回收

	PoolMode poolMode_;//当前线程池的工作模式
	std::atomic_bool isPoolRunning_;//当前线程池的启动状态

};
/*
size_t 是一种数据类型，通常用于表示对象的大小或者数组的索引，通常是无符号整型，能够容纳任何对象的大小
*/




#endif // !THREADPOOL_H


