#include<iostream>
#include"threadpool.h"
#include<thread>
#include<chrono>
#include<memory>
#include"threadpool.h"

using ULong = unsigned long long;

/*
��Щ������Ҫ��ȡ�߳�����ķ���ֵ
������
1+2+....+100
101+102+....+200
201+202+....+300
main thread:��ÿһ���̷߳���������䣬���ȴ����Ǽ��㷵�ؽ����Ȼ��ϲ����ս��

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
	�������ύ֮��ͳ��������ˣ���ʼִ��������������ôûִ�����������ֱ�ӽ������ǵ�ִ���ꣿ
	�޸�Ϊ��������ִ������ܽ�������
*/
{
	ThreadPool pool;
	//�û��Լ������̳߳صĹ���ģʽ
	pool.setMode(PoolMode::MODE_CACHED);
	//��ʼ�����̳߳�
	pool.start(4);
	Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
	/*ULong sum1 = res1.get().cast_<ULong>();
	std::cout << sum1 << std::endl;*/
}



	

	/*//Test5
	��������
	* Ŀǰ���ǵ�������ִ�к����ڵȴ����������µ��߳�������δ���ǵ��Ѿ�����ѭ���ȴ�������������߳�
	* �ڽ�������ʱ�������������̳߳غ���ͬʱ��ռ��
	* 1.pool�߳��Ȼ�ȡ��-->����-->����ȴ�״̬����ʱ�ͷŵ���-->notEmpty()һֱ�ȴ�,��������
	* 2.�̺߳����Ȼ�ȡ������ʱ�������Ϊ�գ�����ѭ������notEmpty()һֱ�ȴ�����������
	
	{
		ThreadPool pool;
		//�û��Լ������̳߳صĹ���ģʽ
		pool.setMode(PoolMode::MODE_CACHED);
		//��ʼ�����̳߳�
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
	ThreadPool ��������֮����ô���̳߳���ص��߳���Դȫ�����գ�
	
	{
		ThreadPool pool;
		//�û��Լ������̳߳صĹ���ģʽ
		pool.setMode(PoolMode::MODE_CACHED);
		//��ʼ�����̳߳�
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
	//�û��Լ������̳߳صĹ���ģʽ
	pool.setMode(PoolMode::MODE_CACHED);
	//��ʼ�����̳߳�
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