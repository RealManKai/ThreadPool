#include"threadpool.h"
#include<functional>
#include<thread>
#include<iostream>
#include<memory>
#include<mutex>

const int TASK_MAX_THRESHOLD = 1024;//INT32_MAX
const int THREAD_MAX_SIZE = 10;//200
const int THREAD_MAX_IDLE_TIME = 10;//��λ����//60

//�̳߳ع��캯��
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

//�̳߳�������������
ThreadPool::~ThreadPool()
{
	isPoolRunning_ = false;
	//notEmpty_.notify_all();
	//test
	std::cout << "**********" << curThreadSize_ << " " << threads_.size() << "**********" << std::endl;
	//�ȴ������߳̽�������(����״̬��1.����ִ�У�2.����)
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	
	notEmpty_.notify_all();
	
	/*exitCond_.wait(lock, [&]()->bool {return curThreadSize_ == 0; });  //ʹ��curThreadSize_�����жϵ�ǰ����Ҫȷ������ȷ��*/
	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
}

//�����̳߳�ģʽ
void ThreadPool::setMode(PoolMode mode)
{
	if (checkRunningState())
		return;
	poolMode_ = mode;
}


//����Task�����������
void ThreadPool::set_taskQueMaxThreshold(int threshold)
{
	if (checkRunningState())
		return;
	//��¼��ʼ�̸߳���
	taskQueMaxThreshold_ = threshold;
	
}
//�����̳߳�Cacheģʽ������߳�����
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
//���̳߳��ύ�����û����øýӿڣ��������������������
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	//��ȡ��
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	
	//�̵߳�ͨ�ţ��ȴ���������п���
	//while (taskQue_.size() == taskQueMaxThreshold_) {
	//	notFull_.wait(lock);//��ǰ״̬����ȴ�����ʱ��Ҫnotify(),����Ҫ�õ�mutex�ſ��Իָ�
	//}����ʽͬ��
	
	//�û��ύ�����������������1s,�����ж��ύ����ʧ��
	if (!notFull_.wait_for(lock, std::chrono::seconds(1)
		, [&]()->bool {return taskQue_.size() < (size_t)taskQueMaxThreshold_; })) 
	{
		//��ʾ�ȴ�һ����������Ȼδ��ɣ�
		std::cerr << "Task queue is full, submit task fail!" << std::endl;
		/*
		* ���ַ��ط�ʽ
		* 1.return task->getResult()
		* 2.return Result(task)
		��һ�ֲ����е�ԭ��������task��ɺ�task����ͱ������ˣ����ʱ��Result���գ�Ҳ�ᱻ���������Բ�����
		*/
		return Result(sp, false);
	}
	//����п��࣬������������������
	taskQue_.emplace(sp);
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
	return Result(sp);
}


//�����̳߳أ�
void ThreadPool::start(int initThreadSize)
{
	//�����̳߳ص�����״̬
	isPoolRunning_ = true;
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;
	//�����̶߳���
	for (int i = 0; i < initThreadSize_; i++) {
		//����thread�̶߳����ʱ����̺߳�������thread�̶߳���
		//���̺߳����󶨵�Thread����Ȼ���õ�ThreadPool�Ķ���ָ��this
		auto ptr = new Thread(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
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
//�����̺߳���
void ThreadPool::threadFunc(int threadid)
{
	auto lastDoTime = std::chrono::high_resolution_clock().now();
	for (;;)//while (isPoolRunning_)
	{
		std::shared_ptr<Task> task;//Ĭ������ָ���ʼ��Ϊ��
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
					curThreadSize_--;//��ʱ����ѭ��������Ҫά����ȷ��
					idleThreadSize_--;
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
			/*
			��������
				1.ִ������
				2.������ķ���ֵͨ��setVal()������Result����
			*/
			//task->run();
			task->exec();
		}
		//�̴߳�������������++
		idleThreadSize_++;
		lastDoTime = std::chrono::high_resolution_clock().now();//�����߳�ִ���������ʱ��
	}
}

//����̳߳��Ƿ���
bool ThreadPool::checkRunningState()const {
	return isPoolRunning_;
}


////////////////////////////
//�����߳�
void Thread::start() {
	//����һ���߳���ִ���̺߳���
	std::thread t(func_, threadId_);
	t.detach();//�����߳�
}

int Thread::generateId_ = 0;

int Thread::getThreadId() const
{
	return threadId_;
}

//���캯��
Thread::Thread(ThreadFunc func)
	:func_(func)
	,threadId_(generateId_++)//ʹ�ó�Ա���������̺߳�������Ա��ʼ���б�����ʼ����ĳ�Ա���� func_�������� func ��ֵ���ݸ���Ա���� func_��
{

}
//��������
Thread::~Thread(){}


Result::Result(std::shared_ptr<Task> task, bool isValid)
	: isValid_(isValid),
	task_(task) 
{
	task_->setResult(this);
}


//setVal()��ȡ����ִ�к�ķ���ֵ
void Result::setVal(Any any)
{
	//�洢task�ķ���ֵ
	this->any_ = std::move(any);
	//�Ѿ���ȡ�ķ���ֵ���ź�����1
	sem_.post();

}

//getVal()�û����û�ȡtask����ֵ
Any Result::get()
{
	if (!isValid_)
	{
		return " ";
	}
	sem_.wait();//task�������û��ִ���꣬�������߳�
	return std::move(any_);
}

void Task::exec()
{
	//run();
	if(result_!=nullptr)
	{
		result_->setVal(run());//��̬����

	}
}

void Task::setResult(Result* res)
{
	result_ = res;
}

Task::Task() 
	:result_(nullptr)
{}