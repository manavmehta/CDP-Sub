// Server.c
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <bits/stdc++.h>

#define MAX 2000

using namespace std;
vector<string> ques_thread = {
	"1. Which one of the following is not shared by threads?\nA. program counter\nB. stack\nC. both (a) and (b)\nD. none of the mentioned\n ",
	"2. A process can be:\n A. single threaded\nB. multithreaded\nC. both (a) and (b)\nD. none of the mentioned\n",
	"3. If one thread opens a file with read privileges then:\nA. other threads in the another process can also read from that file\nB. other threads in the same process can also read from that file\nC. any other thread can not read from that file\nD. all of the mentioned\n",
	"4. The time required to create a new thread in an existing process is:\nA. greater than the time required to create a new process\nB. less than the time required to create a new process\nC. equal to the time required to create a new process\nD. none of the mentioned\n",
	"5. When the event for which a thread is blocked occurs,\nA. thread moves to the ready queue\nB. thread remains blocked\nC. thread completes\nD. a new thread is provided\n"};

vector<char> ans_thread = {'C', 'C', 'B', 'B', 'A'};

vector<string> ques_scheduling = {
	"1. The processes that are residing in main memory and are ready and waiting to execute are kept on a list called _____________\nA. job queue\nB. ready queue\nC. execution queue\nD. process queue\n",
	"2. The interval from the time of submission of a process to the time of completion is termed as ____________\nA. waiting time\nB. turnaround time\nC. response time\nD. throughput\n",
	"3. Which scheduling algorithm allocates the CPU first to the process that requests the CPU first?\nA. first-come, first-served scheduling\nB.shortest job scheduling\nC. priority scheduling\nD. none of the mentioned\n",
	"4. Which algorithm is defined in Time quantum?\nA. shortest job scheduling algorithm\nB. round robin scheduling algorithm\nC. priority scheduling algorithm\nD. multilevel queue scheduling algorithm\n",
	"5.In multilevel feedback scheduling algorithm ____________\nA. a process can move to a different classified ready queue\nB. classification of ready queue is permanent\nC. processes are not classified into groups\nD. none of the mentioned\n"};

vector<char> ans_scheduling = {'B', 'B', 'A', 'B', 'A'};

vector<string> ques_memory = {
	"1. CPU fetches the instruction from memory according to the value of ____________\nA. program counter\nB. status register\nC. instruction register\nD. program status word\n",
	"2. Memory management technique in which system stores and retrieves data from secondary storage for use in main memory is called?\nA. fragmentation\nB. paging\nC. mapping\nD. none of the mentioned\n",
	"3. A memory buffer used to accommodate a speed differential is called ____________\nA. stack pointer\nB. cache\nC. accumulator\nD. disk buffer\n",
	"4. The address of a page table in memory is pointed by ____________\nA. stack pointer\nB. page table base register\nC. page register\nD. program counter\n",
	"5. What is compaction?\nA. a technique for overcoming internal fragmentation\nB. a paging technique\nC. a technique for overcoming external fragmentation\nD. a technique for overcoming fatal error"};

vector<char> ans_memory = {'A', 'B', 'B', 'B', 'C'};

map<string, vector<string>> Question = {{"1", ques_thread}, {"2", ques_scheduling}, {"3", ques_memory}};
map<string, vector<char>> Answer = {{"1", ans_thread}, {"2", ans_scheduling}, {"3", ans_memory}};

map<string, int> user_fd;
map<int, string> fd_user;

class LockManager
{
private:
	mutex m_lock; // mutex to ensure mutual exclusion of monitor
	unordered_map<string, condition_variable> condition_var_write;
	unordered_map<string, int> AW;

public:
	bool okToWrite(const string user_name)
	{
		return !(AW[user_name]);
	}
	void acquireWriteLock(string user_name)
	{
		unique_lock<mutex> lock(m_lock);
		condition_var_write[user_name].wait(lock, [&, this] { return this->okToWrite(user_name); });
		AW[user_name]++;
	}
	void add_user_fd(string user, int fd)
	{
		unique_lock<mutex> lock(m_lock);
		user_fd[user] = fd;
		fd_user[fd] = user;
	}
	void releaseWriteLock(string user_name)
	{
		unique_lock<mutex> lock(m_lock);
		AW[user_name]--;
		lock.unlock();
		condition_var_write[user_name].notify_one();
		lock.lock();
	}
};

void send_string(string s, int fd)
{
	int t = 0;
	while (t < s.size())
	{
		int n = write(fd, s.substr(t).c_str(), s.size() - t);
		if (n < 0)
		{
			printf("Oh dear, something went wrong with write()! %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		t += n;
	}
}

pair<string, string> rec_message(int fd)
{
	char BUFF[MAX];

	string header = "";
	int t = 0;
	while (t < 12)
	{
		memset(BUFF, 0, MAX);
		int n = read(fd, BUFF, 12 - t);
		if (n < 0)
		{
			printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		t += n;
		header += BUFF;
	}
	header.erase(header.find('x'));

	string len = "";
	t = 0;
	while (t < 4)
	{
		memset(BUFF, 0, MAX);
		int n = read(fd, BUFF, 4 - t);
		if (n < 0)
		{
			printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		t += n;
		len += BUFF;
	}

	int len_of_payload = atoi(len.c_str());
	string payload = "";
	t = 0;
	while (t < len_of_payload)
	{
		memset(BUFF, 0, MAX);
		int n = read(fd, BUFF, len_of_payload - t);
		if (n < 0)
		{
			printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		t += n;
		payload += BUFF;
	}
	return {header, payload};
}

void send_message(string header, string payload, int fd)
{
	while (header.size() < 12)
	{
		header += 'x';
	}

	string len = to_string(payload.size());
	while (len.size() < 4)
	{
		len = '0' + len;
	}

	send_string(header + len + payload, fd);
}

pair<string, string> send_rec(string header, string payload, int fd)
{
	send_message(header, payload, fd);
	pair<string, string> p = rec_message(fd);
	return p;
}

void initial_connect(LockManager *lkMgr, int fd)
{
	string user = rec_message(fd).second;
	send_message("welcome", "Welcome " + user + " to online quiz on OS!", fd);
	lkMgr->add_user_fd(user, fd);
}

void individual_mode(string topic, LockManager *lkMgr, int fd)
{
	bool exit = false;
	while (!exit)
	{
		int ind = rand() % Question[topic].size();
		lkMgr->acquireWriteLock(fd_user[fd]);
		pair<string, string> ans = send_rec("response", Question[topic][ind], fd);
		lkMgr->releaseWriteLock(fd_user[fd]);
		sleep(0);

		string retryexit = "To attempt another question press 'N', press 'Q' to quit, press 'R' to return to main menu \n";
		if (ans.second[0] == Answer[topic][ind])
		{
			retryexit = "Correct\n" + retryexit;
		}
		else
		{
			retryexit = "Wrong\n" + retryexit;
		}
		lkMgr->acquireWriteLock(fd_user[fd]);
		pair<string, string> retry_exit_res = send_rec("response", retryexit, fd);
		lkMgr->releaseWriteLock(fd_user[fd]);
		sleep(0);
		if (retry_exit_res.second == "Q" || retry_exit_res.second == "R")
		{
			exit = true;
		}
	}
}

string chat(int fd1, int fd2)
{
	struct epoll_event event1, event2, events[5];
	int epoll_fd = epoll_create1(0);

	if (epoll_fd == -1)
	{
		fprintf(stderr, "Failed to create epoll file descriptor\n");
		return "N";
	}

	event1.events = EPOLLIN;
	event1.data.fd = fd1;
	event2.events = EPOLLIN;
	event2.data.fd = fd2;

	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd1, &event1))
	{
		fprintf(stderr, "Failed to add file descriptor to epoll\n");
		close(epoll_fd);
	}
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd2, &event2))
	{
		fprintf(stderr, "Failed to add file descriptor to epoll\n");
		close(epoll_fd);
	}

	while (true)
	{
		int event_count = epoll_wait(epoll_fd, events, 5, 3000);
		for (int i = 0; i < event_count; i++)
		{
			int fd = events[i].data.fd;
			pair<string, string> p = rec_message(fd);
			string header = p.first;
			string msg = p.second;
			if (msg[0] == '@')
			{
				if (fd == fd1)
				{
					send_message("chat", msg.substr(1), fd2);
				}
				else
				{
					send_message("chat", msg.substr(1), fd1);
				}
			}
			else
			{
				if (close(epoll_fd))
				{
					fprintf(stderr, "Failed to close epoll file descriptor\n");
				}
				return msg;
			}
		}
	}
}

void group_quiz(string topic, string user1, string user2, int fd1, int fd2)
{
	bool exit = false;
	while (!exit)
	{
		int ind = rand() % Question[topic].size();

		send_message("chat", Question[topic][ind], fd1);
		send_message("chat", Question[topic][ind], fd2);
		string ans = chat(fd1, fd2);
		// pair<string, string> ans1 = rec_message(fd1);
		// pair<string, string> ans2 = rec_message(fd2);
		string retryexit = "To attempt another question press 'N', press 'Q' to quit, press 'R' to return to main menu \n";
		if (ans[0] == Answer[topic][ind])
		{
			retryexit = "Correct\n" + retryexit;
		}
		else
		{
			retryexit = "Wrong\n" + retryexit;
		}
		send_message("response", retryexit, fd1);
		send_message("response", retryexit, fd2);
		pair<string, string> retry_exit_res1 = rec_message(fd1);
		pair<string, string> retry_exit_res2 = rec_message(fd2);
		if (retry_exit_res1.second == "Q" || retry_exit_res1.second == "R" || retry_exit_res2.second == "Q" || retry_exit_res2.second == "R")
		{
			exit = true;
		}
	}
}

void group_mode(string user, LockManager *lkMgr, int fd)
{
	string user1 = fd_user[fd];
	string user2 = user;
	int fd1 = fd, fd2 = user_fd[user2];
	assert(fd1 != fd2);
	string connection_request = user1 + " wants to connect.\n Y\n N\n";
	lkMgr->acquireWriteLock(fd_user[fd1]);
	lkMgr->acquireWriteLock(fd_user[fd2]);
	pair<string, string> conn_status = send_rec("connect", connection_request, fd2);
	if (conn_status.second == "Y")
	{
		group_quiz("1", user1, user2, fd1, fd2);
	}
	lkMgr->releaseWriteLock(fd_user[fd1]);
	lkMgr->releaseWriteLock(fd_user[fd2]);
}

void server(LockManager *lkMgr, int fd)
{
	while (true)
	{
		string mode_msg = "Welcome to CDP quiz!\nSelect Mode:\nPress I for individual Mode\nPress G for group mode\nPress A for admin mode\nPress Q to Quit.\n";
		lkMgr->acquireWriteLock(fd_user[fd]);
		pair<string, string> mode_res = send_rec("response", mode_msg, fd);
		lkMgr->releaseWriteLock(fd_user[fd]);
		sleep(0);

		string mode = mode_res.second;
		if (mode == "I")
		{
			string topic_msg = "Ok. Pick a topic from the following: \n 1.Threads \n 2.Scheduling \n 3.Memory Management \n";
			lkMgr->acquireWriteLock(fd_user[fd]);
			pair<string, string> topic_res = send_rec("response", topic_msg, fd);
			lkMgr->releaseWriteLock(fd_user[fd]);
			sleep(0);
			individual_mode(topic_res.second, lkMgr, fd);
		}
		else if (mode == "G")
		{
			string group_msg = "The following users are online:- ";
			for (auto elm : user_fd)
			{
				group_msg += ", " + elm.first;
			}
			group_msg += "\n specify the user_name of the user with whom you want to collaborate\n";
			lkMgr->acquireWriteLock(fd_user[fd]);
			pair<string, string> group_res = send_rec("response", group_msg, fd);
			lkMgr->releaseWriteLock(fd_user[fd]);
			sleep(0);
			group_mode(group_res.second, lkMgr, fd);
		}
		else if (mode == "A")
		{
			string admin_msg = "Now You are in admin mode.\n";
			lkMgr->acquireWriteLock(fd_user[fd]);
			send_rec("response", admin_msg, fd);
			lkMgr->releaseWriteLock(fd_user[fd]);
			sleep(0);
		}
		else
		{
			string quit_msg = "Thanks for playing!";
			lkMgr->acquireWriteLock(fd_user[fd]);
			send_message("quit", quit_msg, fd);
			lkMgr->releaseWriteLock(fd_user[fd]);
			return;
		}
	}
}

vector<thread *> v_th;

void execute(LockManager *lkMgr, int fd)
{
	initial_connect(lkMgr, fd);
	server(lkMgr, fd);
	string user = fd_user[fd];
	user_fd.erase(user_fd.find(user));
	fd_user.erase(fd_user.find(fd));
	return;
}

int main()
{

	int lstnsockfd, consockfd, clilen, portno = 5033;
	struct sockaddr_in serv_addr, cli_addr;

	memset((char *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	// Server protocol
	/* Create Socket to receive requests*/
	lstnsockfd = socket(AF_INET, SOCK_STREAM, 0);
	/* Bind socket to port */
	bind(lstnsockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	printf("Bounded to port\n");
	LockManager *lkMgr = new LockManager();
	while (1)
	{
		printf("Listening for incoming connections\n");

		/* Listen for incoming connections */
		listen(lstnsockfd, 10);

		// clilen = sizeof(cli_addr);

		/* Accept incoming connection, obtaining a new socket for it */
		consockfd = accept(lstnsockfd, (struct sockaddr *)&cli_addr,
						   (socklen_t *)&clilen);

		if (consockfd < 0)
		{
			printf("Oh dear, something went wrong with connect! %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		cout << "Accepted connection" << endl;
		v_th.push_back(new thread(execute, lkMgr, consockfd));
	}
	for (auto elm : v_th)
	{
		elm->join();
	}
	for (auto elm : fd_user)
	{
		close(elm.first);
	}
	close(lstnsockfd);

	return 0;
}
