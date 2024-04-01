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
const int THREAD_MAX_IDLE_TIME = 10;//��λ����//60

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

	//���캯��
	Thread(ThreadFunc func)
		:func_(func)
		, threadId_(generateId_++)//ʹ�ó�Ա���������̺߳�������Ա��ʼ���б�����ʼ����ĳ�Ա���� func_�������� func ��ֵ���ݸ���Ա���� func_��
	{}
	//��������
	~Thread() = default;
	//�����߳�
	void start()
	{
		//����һ���߳���ִ���̺߳���
		std::thread t(func_, threadId_);
		t.detach();//�����߳�
	}
	//��ȡ�߳�ID
	int getThreadId() const
	{
		return threadId_;
	}

private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_;//�����߳�ID

};

int Thread::generateId_ = 0;

//�̳߳�����
class ThreadPool {

public:
	//�̳߳ع��캯��
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

	//�̳߳�������������
	~ThreadPool()
	{
		isPoolRunning_ = false;
		std::cout << "**********" << curThreadSize_ << " " << threads_.size() << "**********" << std::endl;
		//�ȴ������߳̽�������(����״̬��1.����ִ�У�2.����)
		std::unique_lock<std::mutex> lock(taskQueMtx_);

		notEmpty_.notify_all();
		exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
	}

	//����Task�����������
	void set_taskQueMaxThreshold(int threshold)
	{
		if (checkRunningState())
			return;
		//��¼��ʼ�̸߳���
		taskQueMaxThreshold_ = threshold;
	}

	//�����̳߳�Cacheģʽ������߳�����
	void setThreadSizeThreshold(int threshold)
	{
		if (checkRunningState())
			return;
		if (poolMode_ == PoolMode::MODE_CACHED)
		{
			threadSizeThreshold_ = threshold;
		}
	}
	//���̳߳��ύ����
	template<typename Func,typename... Args>
	auto submitTask(Func&& func, Args&&...args) -> std::future<decltype(func(args...))>
	{
		//������񣬷ŵ����������
		using RType = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<RType()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		std::future<RType> Result = task->get_future();

		//��ȡ��
		std::unique_lock<std::mutex> lock(taskQueMtx_);

		//�û��ύ�����������������1s,�����ж��ύ����ʧ��
		if (!notFull_.wait_for(lock, std::chrono::seconds(1)
			, [&]()->bool {return taskQue_.size() < (size_t)taskQueMaxThreshold_; }))
		{
			//��ʾ�ȴ�һ����������Ȼδ��ɣ�
			std::cerr << "Task queue is full, submit task fail!" << std::endl;
			auto task = std::make_shared<std::packaged_task<RType()>>(
				[]()->RType {return RType(); });
			return task->get_future();
		}
		//����п��࣬������������������
		//taskQue_.emplace(sp);
		//using Task = std::function<void()>;
		//��ʱTask����Ϊ�ն���ʹ��lambda���ʽ��װ�м��
		taskQue_.emplace([task]() {(*task)(); });

		taskSize_++;
		//�·�������������в����ˣ���ʱ��notEmpty_����֪ͨ
		notEmpty_.notify_all();

		//Cacheģʽ��������������ȽϽ�����С�����������Ҫ�������������Ϳ����̵߳��������ж��Ƿ���Ҫ�����µ��߳�
		if (poolMode_ == PoolMode::MODE_CACHED
			&& taskSize_ > idleThreadSize_
			&& curThreadSize_ < threadSizeThreshold_)
		{
			std::cout << "new thread" << std::this_thread::get_id() << "<<<<<<<<<<" << std::endl;
			//�������߳�
			auto ptr = new Thread(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			//threads_.emplace_back(std::move(ptr));//�� push_back() �������ƣ����� emplace_back() ���Խ������������Ĳ������������Ǵ��ݸ�Ԫ�����͵Ĺ��캯�����Ӷ���ԭ�ع���Ԫ�أ�������Ҫ����Ŀ������ƶ�������
			int tId = ptr->getThreadId();
			threads_.emplace(tId, std::move(ptr));
			threads_[tId]->start();//�����߳�
			//�޸��߳���ر���
			idleThreadSize_++;
			curThreadSize_++;
		}
		//���������Result����
		return Result;
	}
	//�����̳߳�ģʽ
	void setMode(PoolMode mode)
	{
		if (checkRunningState())
			return;
		poolMode_ = mode;
	}

	//�����̳߳أ�
	void start(int initThreadSize = std::thread::hardware_concurrency())//hardware_concurrency()ϵͳ��������
	{
		//�����̳߳ص�����״̬
		isPoolRunning_ = true;
		initThreadSize_ = initThreadSize;
		curThreadSize_ = initThreadSize;
		//�����̶߳���
		for (int i = 0; i < initThreadSize_; i++) {
			//����thread�̶߳����ʱ����̺߳�������thread�̶߳���
			//���̺߳����󶨵�Thread����Ȼ���õ�ThreadPool�Ķ���ָ��this
			auto ptr = new Thread(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			//threads_.emplace_back(std::move(ptr));//�� push_back() �������ƣ����� emplace_back() ���Խ������������Ĳ������������Ǵ��ݸ�Ԫ�����͵Ĺ��캯�����Ӷ���ԭ�ع���Ԫ�أ�������Ҫ����Ŀ������ƶ�������
			int threadId = ptr->getThreadId();
			threads_.emplace(threadId, std::move(ptr));
		}
		//���������߳� std::vector<Thread*> thread_
		for (int i = 0; i < initThreadSize_; i++) {
			threads_[i]->start();//��Ҫȥִ��һ���̺߳���
			idleThreadSize_++;//��¼��ʼ�����߳�����
		}
	}

	//��ֹ�̳߳ؽ��п�������͸�ֵ
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator= (const ThreadPool&) = delete;

private:

	//�̺߳���
	void threadFunc(int threadid)
	{
		auto lastDoTime = std::chrono::high_resolution_clock().now();
		for (;;)//while (isPoolRunning_)
		{
			Task task;//Ĭ������ָ���ʼ��Ϊ��
			//����һ���ֲ������������������Ͱ����ͷŵ����Զ�������
			{
				std::cout << "���Ի�ȡ����" << "tid��" << std::this_thread::get_id() << std::endl;
				//�Ȼ�ȡ��
				std::unique_lock<std::mutex> lock(taskQueMtx_);
				//cacheģʽ��,���ܴ����˺ܶ��̣߳����ǿ���ʱ�䳬��60s��Ӧ�ðѶ�����߳̽������գ�
					//����ǰ���գ�����initThreadSize_�������߳�Ҫ���յ���
					//��ǰʱ��-��һ���߳�ִ��ʱ�� > 60S
				//��+˫���ж�
				while (taskQue_.size() == 0)
				{
					////�̳߳�Ҫ�����������߳���Դ
					if (!isPoolRunning_) {
						//threadid==>thread����==>ɾ��
						threads_.erase(threadid);
						exitCond_.notify_all();
						std::cout << "thread" << std::this_thread::get_id() << "����!" << std::endl;
						return;
					}

					if (poolMode_ == PoolMode::MODE_CACHED)
					{
						//ÿһ���ӷ���һ�Σ���ô���ֳ�ʱ���ػ��������������
							//������������ʱ������
						if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
						{
							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastDoTime);
							if (dur.count() >= THREAD_MAX_IDLE_TIME
								&& curThreadSize_ > initThreadSize_)
							{
								//�����߳�
								//��¼�߳����������ֵ--
								//�߳��б��е��̶߳���ɾ��(�ѵ���û�취ȷ��threadFunc �󶨵����ĸ��̶߳���)
								threads_.erase(threadid);
								curThreadSize_--;
								idleThreadSize_--;
								std::cout << "thread" << std::this_thread::get_id() << "����!" << std::endl;
								return;
							}
						}
					}
					else
					{
						//�ȴ�notEmpty����,������в�Ϊ��
						notEmpty_.wait(lock);
					}
				}
				//�̱߳�������������++
				idleThreadSize_--;
				//�����������ȡһ���������
				//std::shared_ptr<Task> task = taskQue_.front();
				task = taskQue_.front();
				//����--
				taskQue_.pop();
				taskSize_--;
				//֪ͨ��������
				//�����Ȼ��ʣ�����񣬼���֪ͨ�����߳�ִ������
				if (taskQue_.size() > 0)
				{
					notEmpty_.notify_all();
				}
				//�ó��������֪ͨ
				notFull_.notify_all();
			}
			std::cout << "��ȡ����ɹ���" << "tid��" << std::this_thread::get_id() << std::endl;
			//��ǰ�̸߳���ִ���������
			if (task != nullptr)
			{
				task();//ִ��function<void()>
			}
			//�̴߳�������������++
			idleThreadSize_++;
			lastDoTime = std::chrono::high_resolution_clock().now();//�����߳�ִ���������ʱ��
		}
	}
	//���pool������״̬
	bool checkRunningState()const
	{
		return isPoolRunning_;
	}

private:
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;

	int initThreadSize_;//��ʼ���߳�����
	int threadSizeThreshold_;//�߳���������
	std::atomic_int curThreadSize_;//��ǰ�̳߳��̵߳�����
	std::atomic_int idleThreadSize_;//��¼�߳̿���ʱ�������
	
	using Task = std::function<void()>;
	std::queue<Task> taskQue_;//�������
	std::atomic_uint taskSize_; //��������
	int taskQueMaxThreshold_;//�����������������ֵ

	std::mutex taskQueMtx_;//��֤������е��̰߳�ȫ
	std::condition_variable  notFull_;//��֤������в���
	std::condition_variable notEmpty_;//��֤������в���
	std::condition_variable exitCond_;//�ȵ��߳���Դȫ������

	PoolMode poolMode_;//��ǰ�̳߳صĹ���ģʽ
	std::atomic_bool isPoolRunning_;//��ǰ�̳߳ص�����״̬
};
