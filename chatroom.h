#include <sys/types.h>  
#include <netinet/in.h>  
#include <sys/socket.h>  
#include <arpa/inet.h>       
#include <unistd.h>         
#include <fcntl.h>
#include <signal.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>



#define LIMIT_NUM 10
#define EVENT_NUM 100
#define MAX_FD 1024

typedef struct _st_user_data
{
	int index;
	char* pWriteBuf;
	char  readBuf[1024];
}User_data;

class chatServer
{
public:
	chatServer(short nPort);
	chatServer(char* sIP, short nPort);
	int  createNoBlockingfd();
	int  bindfd(int blocklog);
	void runEvent(int milliseconds);
	void release();
private:
	chatServer(const chatServer& rhl);
	const chatServer& operator=(const chatServer& rhl);
private:
	int  setNoBlocking(int fd);
	int  addEpoll(int fd, int op, unsigned int ev);
	void closefd(int fd);
private:
	User_data *mpUserData;
	int  mEpollfd;
	int  mListenfd;
	char *mIP;
	short mPort;
	int   mClientNum;
	int  mArrFd[LIMIT_NUM+1];
};



