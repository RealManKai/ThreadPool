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

//�����ϵ��࣬�������ⷵ��ֵ����
class Any 
{
public:
	Any() = default;
	~Any() = default;
	//��ֵ�Ŀ�������͸�ֵ
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	//��ֵ�Ŀ�������͸�ֵ
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;
	//���캯��������Any���ͽ���������������
	template<typename T>
	Any(T data) : base_(std::make_unique<Derive<T>>(data))
	{}
	//��Any������ߴ洢��data������ȡ����
	template<typename T>
	T cast_()
	{
		// ������ô��base_�ҵ�����ָ���Derive���󣬴�������ȡ��data��Ա����
		// ����ָ�� =�� ������ָ��   RTTI
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr)
		{
			std::cout << "error" << std::endl;
			throw "type is unmatch!";
		}
		return pd->data_;
	}
private:
	//����
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
	//�������ָ��
	std::unique_ptr<Base> base_;
};

//�����ź���
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
		//�ȴ��ź�����Դ����������
		con_.wait(lock,[&]()->bool {return resLimit_ > 0; });
		resLimit_--;
	}
	//++
	void post()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		resLimit_++;
		con_.notify_all();// �ȴ�״̬���ͷ�mutex�� ֪ͨ��������wait�ĵط������������ɻ���
	}

private:
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable con_;

};

//Task����
class Task;

//Result�����̷߳��صĽ��ת�������񷵻صĽ��
//�����̷߳��صĽ��
class Result {
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	~Result() = default;
	//setVal()��ȡ����ִ�к�ķ���ֵ
	void setVal(Any any);

	//getVal()�û����û�ȡtask����ֵ
	Any get();

private:
	Any any_;//�洢����ķ���ֵ
	Semaphore sem_;//�߳�ͨ���ź���
	std::shared_ptr<Task> task_;//ָ���Ӧ��ȡ����ֵ���������
	std::atomic_bool isValid_;//����ֵ�Ƿ���Ч



};


//����������
//�û������Զ��������������ͣ���Task�̳У���дrun()������ʵ���Զ���������
class Task {
public:
	Task();
	~Task() = default;
	void exec();
	void setResult(Result* res);
	// �û������Զ��������������ͣ���Task�̳У���дrun������ʵ���Զ���������
	virtual Any run() = 0;
private:
	Result* result_;//Result�����ڱ�task���󳤣�����Ҫ��ǿ����ָ��
};


//�̳߳�֧�ֵ�ģʽ
enum class PoolMode {  //enum ��һ����������ö�����͵Ĺؼ��֡�ö����������������һ��������������������Щ������ö�����͵ķ�Χ�ھ���Ψһ�ı�ʶ����
	MODE_FIXED,
	MODE_CACHED,
};

class Thread {
	//�߳�����
public:
	//����һ���̺߳�����������
	using ThreadFunc = std::function<void(int)>;
	//�����߳�
	void start();
	//���캯��
	 Thread(ThreadFunc func);
	//��������
	 ~Thread();
	 //��ȡ�߳�ID
	 int getThreadId() const;

private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_;//�����߳�ID

};

/*
* example��
ThreadPool pool
pool.start()
class MyTask:public Task{
	public:
		void run();
}
pool.submitTask(std::make_shared<MyTask>());

std::make_shared ����ģ��������������Ĳ�����
�������Ǵ��ݸ� MyTask ���͵Ĺ��캯����Ȼ�󷵻�
һ�� std::shared_ptr ����ָ�룬��ָ��ָ��һ����
̬����� MyTask ���Ͷ���
std::make_shared�ĺô��ǿ����ö�����ڴ�����û������ڴ���а��Զ��ͷţ���ֹ�ڴ�й¶
*/


//�̳߳�����
class ThreadPool {

public:
	//�̳߳ع��캯��
	ThreadPool();

	//�̳߳�������������
	~ThreadPool();

	//����Task�����������
	void set_taskQueMaxThreshold(int threshold);

	//�����̳߳�Cacheģʽ������߳�����
	void setThreadSizeThreshold(int threshold);

	//���̳߳��ύ����
	Result submitTask(std::shared_ptr<Task> sp);

	//�����̳߳�ģʽ
	void setMode(PoolMode mode);

	//�����̳߳أ�
	void start(int initThreadSize = std::thread::hardware_concurrency());//hardware_concurrency()ϵͳ��������

	//�̺߳���
	void threadFunc(int threadid);

	//��ֹ�̳߳ؽ��п�������͸�ֵ
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator= (const ThreadPool&) = delete;
	/*
	ThreadPool&������һ���������ͣ���ʾ��һ���Ѵ��ڵ� ThreadPool ��������á�
	&ThreadPool������һ��ָ�����ͣ���ʾһ��ָ�� ThreadPool �����ָ�롣
	*/
	bool checkRunningState()const;

private:
	//std::vector<std::unique_ptr<Thread>> threads_; //�߳��б�
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;

	size_t initThreadSize_;//��ʼ���߳�����
	int threadSizeThreshold_;//�߳���������
	std::atomic_int curThreadSize_;//��ǰ�̳߳��̵߳�����
	std::atomic_int idleThreadSize_;//��¼�߳̿���ʱ�������
	
	std::queue<std::shared_ptr<Task>> taskQue_;//�������
	std::atomic_uint taskSize_; //��������
	int taskQueMaxThreshold_;//�����������������ֵ

	std::mutex taskQueMtx_;//��֤������е��̰߳�ȫ
	std::condition_variable  notFull_;//��֤������в���
	std::condition_variable notEmpty_;//��֤������в���
	std::condition_variable exitCond_;//�ȵ��߳���Դȫ������

	PoolMode poolMode_;//��ǰ�̳߳صĹ���ģʽ
	std::atomic_bool isPoolRunning_;//��ǰ�̳߳ص�����״̬

};
/*
size_t ��һ���������ͣ�ͨ�����ڱ�ʾ����Ĵ�С���������������ͨ�����޷������ͣ��ܹ������κζ���Ĵ�С
*/




#endif // !THREADPOOL_H


