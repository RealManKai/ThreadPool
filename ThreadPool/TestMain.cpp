#include<iostream>
#include"threadpool.h"
#include<thread>
#include<chrono>
#include<memory>
#include"threadpool.h"

using ULong = unsigned long long;

/*
有些场景需要获取线程任务的返回值
举例：
1+2+....+100
101+102+....+200
201+202+....+300
main thread:给每一个线程分配计算区间，并等待他们计算返回结果，然后合并最终结果

*/

class MyTask :public Task {
public:
	MyTask(ULong begin, ULong end)
		: begin_(begin)
		, end_(end)
	{}
	
	Any run() 
	{
		std::cout <<"begin!" << " " << "tid:" << std::this_thread::get_id() << std::endl;
		ULong sum = 0;
		for (ULong i = begin_; i <= end_; i++) {
			sum += i;
		}
		std::cout <<"end!" << " " << "tid:" << std::this_thread::get_id() << std::endl;
		return sum;
	}
private:
	ULong begin_;
	ULong end_;
};

class Tak: public Task {
public:
	Any run() {
		std::cout << "begin!" << "tid:" << std::this_thread::get_id() << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(1));
		std::cout << "end!" << "tid:" << std::this_thread::get_id() << std::endl;
		return 0;
	}

};


int main() {
	
/*Test6
	在任务提交之后就出作用域了，开始执行析构函数，那么没执行完的任务是直接结束还是等执行完？
	修改为所有任务执行完才能进行析构
*/
{
	ThreadPool pool;
	//用户自己设置线程池的工作模式
	pool.setMode(PoolMode::MODE_CACHED);
	//开始启动线程池
	pool.start(4);
	Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
	/*ULong sum1 = res1.get().cast_<ULong>();
	std::cout << sum1 << std::endl;*/
}



	

	/*//Test5
	死锁问题
	* 目前考虑到了正在执行和正在等待两种情形下的线程析构，未考虑到已经进入循环等待拿锁抢任务的线程
	* 在进行析构时，析构函数和线程池函数同时抢占锁
	* 1.pool线程先获取到-->阻塞-->进入等待状态，此时释放掉锁-->notEmpty()一直等待,进入死锁
	* 2.线程函数先获取到，此时任务队列为空，进入循环，则notEmpty()一直等待，进入死锁
	
	{
		ThreadPool pool;
		//用户自己设置线程池的工作模式
		pool.setMode(PoolMode::MODE_CACHED);
		//开始启动线程池
		pool.start(4);
		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		ULong sum1 = res1.get().cast_<ULong>();
		std::cout << sum1 << std::endl;
	}
	system("pause");

	*/
	/*
	Test4
	*
	ThreadPool 对象析构之后，怎么把线程池相关的线程资源全部回收？
	
	{
		ThreadPool pool;
		//用户自己设置线程池的工作模式
		pool.setMode(PoolMode::MODE_CACHED);
		//开始启动线程池
		pool.start(4);

		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		ULong sum1 = res1.get().cast_<ULong>();
		ULong sum2 = res2.get().cast_<ULong>();
		ULong sum3 = res3.get().cast_<ULong>();
		std::cout << (sum1 + sum2 + sum3) << std::endl;
	}
	system("pause");
	*/


	/*
	Test3
	ThreadPool pool;
	//用户自己设置线程池的工作模式
	pool.setMode(PoolMode::MODE_CACHED);
	//开始启动线程池
	pool.start(4);

	Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
	Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
	Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
	pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
	pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
	pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
	pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
	ULong sum1 = res1.get().cast_<ULong>();
	ULong sum2 = res2.get().cast_<ULong>();
	ULong sum3 = res3.get().cast_<ULong>();

	*/


	/*
	//Test2
	ThreadPool pool;
	pool.start(4);

	Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
	Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
	Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));

	ULong sum1 = res1.get().cast_<ULong>();
	ULong sum2 = res2.get().cast_<ULong>();
	ULong sum3 = res3.get().cast_<ULong>();

	std::cout << (sum1+ sum2+ sum3) << std::endl;
	*/

	/*
	Test1
	ThreadPool pool;
	pool.start(4);
	pool.submitTask(std::make_shared<Tak>());
	pool.submitTask(std::make_shared<Tak>());
	pool.submitTask(std::make_shared<Tak>());
	pool.submitTask(std::make_shared<Tak>());
	system("pause");
	*/
	return 0;
}