#include<iostream>
#include<functional>
#include<thread>
#include<future>
#include"Threadppool.h"

using namespace std;

int sum1(int a, int b) {
	return a + b;
}


int main() {
	

	ThreadPool pool;
	pool.start(4);
	auto res = pool.submitTask(sum1, 30, 20);
	cout << res.get() << endl;

	//packaged_task<int(int, int)> task(sum1);//���һ����������
	//future<int> res = task.get_future();
	//task(10, 20); 
	//cout << res.get() << endl;//future��get()����᷵��������

	//thread t(move(task), 10, 30);
	//t.detach();
	return 0;
}