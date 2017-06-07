#ifndef server_h_
#define server_h_
#include <iostream>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <string>
#include <deque>
#include <map>
#include <set>
#include <memory>
#include <unistd.h>

using namespace std;

#define EVENT_LIMIT 1000

class ChatSession;
class ChatServer;
class ChatRoom;

class ChatRoom
{
public:
	void join(ChatSession* session);
	void leave(ChatSession* session);
	void deliver(const string& msg, ChatSession* outSession);
private:
	set<ChatSession*> session_;
	deque<string> recent_msgs;
};

class ChatServer 
{
public:
	ChatServer(short& nPort);
	int openServer(int blockflag);
	int closeServer();
	int runServer(int milliseconds);

	int addEpoll(int fd, int op, unsigned int ev, void* p);
private:
	void closeFd(int fd);
	int createSession(int fd);
	int createNonBlockFd();
	int setFdNonBlock(int fd);
private:
	int   mListenFd;
	int   mepollId;
	short mPort;
	ChatRoom mRoom;
	std::map<int, ChatSession*> mSessionMap;
};

class ChatSession 
{
	enum { msg_len = 512, };
public:
	ChatSession(int fd, ChatRoom& room, ChatServer* server);
	int deliver(const string& msg);
	int deliver();
	int reciver();
	int getfd()
	{
		return mfd;
	}
private:
	int mfd;
	ChatRoom& room_;
	ChatServer* server_ptr_;
	std::deque<std::string> m_send_msgs;
	std::string m_recv_msg;
	bool is_send_enable_;
};

#endif 
