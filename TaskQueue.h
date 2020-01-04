#ifndef TASKQUEUE_H
#define TASKQUEUE_H

#include <atomic>
#include <thread>
#include <mutex>
#include <array>
#include <functional>
#include <condition_variable>
#include <vector>

using namespace std;

class TaskQueue {

	int NumOfThreads;
	vector<thread> threads;
	vector<function<void()>> queue;

	atomic_int jobs;
	atomic_bool thread_close_signal;
	atomic_bool finished;
	condition_variable thread_v;
	condition_variable main_v;
	mutex wait_mutex;
	mutex queue_mutex;

	void Task() {
		while (!thread_close_signal) {
			next_job()();	//zažene delo ki je v queue

			--jobs;
			main_v.notify_one(); //obvestim main thread, da je opravljeno
		}
	}

	function<void()> next_job() {
		function<void(void)> res;
		unique_lock<mutex> job_lock(queue_mutex);

		thread_v.wait(job_lock, [this]() -> bool { return queue.size() || thread_close_signal; });

		if (!thread_close_signal) {
			res = queue.front();
			queue.erase(queue.begin());
		}
		else {
			res = [] {};	//v primeru da bom koncal v lambdo ne posljem nic, tako thread zapusti wait da ga lahko joinam
			++jobs;
		}
		return res;
	}

public:
	TaskQueue()
	{
		jobs = 0;
		thread_close_signal = false;
		finished = false;
		NumOfThreads = thread::hardware_concurrency();

		for (int i = 0; i < NumOfThreads; ++i)
			threads.push_back(thread([this] { Task(); }));
	}

	TaskQueue(int thrNum)
	{
		jobs = 0;
		thread_close_signal = false;
		finished = false;
		NumOfThreads = thrNum;

		for (int i = 0; i < NumOfThreads; ++i)
			threads.push_back(thread([this] { Task(); }));
	}

	~TaskQueue()
	{
		JoinAll();
	}

	void AddJob(function<void()> job) {
		lock_guard<mutex> guard(queue_mutex);
		queue.push_back(job);	//v queue dodam novi job
		++jobs;	//poveèam št del ki so navoljo
		thread_v.notify_one();	//obvestim en thread da priène z delom
	}

	void JoinAll() {
		if (!finished) {
			if (jobs > 0) {	//pocakam da se vsa dela dokoncajo
				//cout << "jobs: " << NumOfThreads << endl;
				unique_lock<mutex> lk(wait_mutex);
				main_v.wait(lk, [this] { return this->jobs == 0; });	//zaklenem mutex, èaka tako dolgo da se zakljucijo vsa dela
				lk.unlock();
			}	//pocakam da se vsa dela dokoncajo

			thread_close_signal = true;	//konec z delom, opomnim vse threade ki so v wait in cakajo na delo
			thread_v.notify_all();

			for (auto& x : threads) {
				if (x.joinable()) {
					x.join();	//vse threade preverim èe so joinable in jih joinam
				}
			}
			finished = true;
		}
	}

	void JoinAllInterrupt() {
		if (!finished) {
			if (jobs > 0) {	//pocakam da se vsa dela dokoncajo
				unique_lock<mutex> lk(wait_mutex);
				main_v.wait(lk, [this] { return this->jobs <= NumOfThreads - 1; });	//zaklenem mutex, ko eden najde blok drugi tudi zakljucijo
				lk.unlock();
			}	//pocakam da se vsa dela dokoncajo

			thread_close_signal = true;	//konec z delom, opomnim vse threade ki so v wait in cakajo na delo
			thread_v.notify_all();

			for (auto& x : threads) {
				if (x.joinable()) {
					x.join();	//vse threade preverim èe so joinable in jih joinam
				}
			}
			finished = true;
		}
	}

	bool checkThreadCloseSignal() {
		if (thread_close_signal) {
			return true;
		}
		else {
			return false;
		}
	}
};
#endif //TASKQUEUE_H