#include "chatroom.h"


chatServer::chatServer(short nPort):
mpUserData(NULL),
mEpollfd(epoll_create(256)),
mListenfd(-1),
mIP(NULL),
mPort(nPort),
mClientNum(0)
{
}

chatServer::chatServer(char* sIP, short nPort):
mpUserData(NULL),
mEpollfd(epoll_create(256)),
mListenfd(-1),
mIP(sIP),
mPort(nPort),
mClientNum(0)
{
	
}

int  chatServer::bindfd(int blocklog)
{	
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(mPort);
	addr.sin_addr.s_addr = (mIP != NULL)?inet_addr(mIP):INADDR_ANY;
	
	int ret = 0;
	do
	{
		unsigned int  ev = EPOLLIN|EPOLLERR;
		ret = addEpoll(mListenfd, EPOLL_CTL_ADD, ev);
		if (ret == -1)
		{
			printf("epoll_ctl error: %d\n", errno);
			break;
		}
		ret = bind(mListenfd, (const sockaddr*)&addr, sizeof(sockaddr));
		if (ret == -1)
		{
			printf("bind error: %d\n", errno);
			break;
		}
		ret = listen(mListenfd, blocklog);
		if (ret == -1)
		{
			printf("listen error: %d\n", errno);
			break;
		}
	
	}while(false);
	return ret;
}

void chatServer::runEvent(int milliseconds)
{
	printf("running chat server....\n");
	mpUserData = new User_data[MAX_FD];
	memset(mpUserData, 0, MAX_FD);
	struct epoll_event events[EVENT_NUM];
	int ready = 0;
	memset(mArrFd, 0, sizeof(int)*(LIMIT_NUM+1));
	while(1)
	{
		ready = epoll_wait(mEpollfd, events, EVENT_NUM, milliseconds);
		if (ready == -1)
		{
			printf("epoll_wait error: %d\n", errno);
			break;
		}
		else if ( ready == 0 )
		{
			printf("timeout...\n");
			continue;
		}
		for (int i = 0; i < ready; ++i)
		{
			if (mListenfd == events[i].data.fd)
			{
				struct sockaddr_in rAddr;
				socklen_t addrLen = sizeof(rAddr);
				int connfd = accept(mListenfd, (struct sockaddr*)&rAddr, &addrLen);
				if (connfd == -1 && errno != EAGAIN)
				{
					printf("accept error: %d\n", errno);
					continue;
				}
				if (mClientNum > LIMIT_NUM)
				{
					printf("client number is overflow\n");
					closefd(connfd);
					continue;
				}
				setNoBlocking(connfd);
				unsigned int   ev = EPOLLIN|EPOLLERR;
				addEpoll(connfd, EPOLL_CTL_ADD, ev);
				mArrFd[mClientNum] = connfd;
				mpUserData[connfd].index = mClientNum;
				++mClientNum;
				printf("remote:[%s:%d]\n", inet_ntoa(rAddr.sin_addr), ntohs(rAddr.sin_port));
			}
			else if(events[i].events&EPOLLERR)   //error
			{
				int fd = events[i].data.fd;
				movefd(fd);
				unsigned int   ev = events[i].events;
				addEpoll(fd, EPOLL_CTL_DEL, ev);
				closefd(fd);
			}
			else if(events[i].events&EPOLLIN)   //read
			{
				int fd = events[i].data.fd;
				memset(mpUserData[fd].readBuf, 0, 1024);
				int ret = recv(fd,  mpUserData[fd].readBuf, 1023, 0);
				if (ret == 0 || (ret < 0 && errno != EAGAIN))
				{
					printf(" recv ret: %d, error: %d\n", ret, errno);
					movefd(fd);
					unsigned int   ev = events[i].events;
					addEpoll(fd, EPOLL_CTL_DEL, ev);
					closefd(fd);
				}
				else if(ret > 0)
				{
					for(int i = 0; i < mClientNum; ++i)
					{
						if (mArrFd[i] == fd)
						{
							continue;
						}
						unsigned int   ev = EPOLLERR|EPOLLOUT;
						mpUserData[mArrFd[i]].pWriteBuf = mpUserData[fd].readBuf;
						addEpoll(mArrFd[i], EPOLL_CTL_MOD, ev);
					}
				}
				printf("recv[%d]: %s\n", fd, mpUserData[fd].readBuf);
			}
			else if(events[i].events&EPOLLOUT) //write
			{
				int ret = 0;
				int fd = events[i].data.fd;
				int size = strlen(mpUserData[fd].pWriteBuf);
				int nAlready = 0;
				while(size)
				{
					int ret = send(fd, mpUserData[fd].pWriteBuf+nAlready, size, 0);
					if (ret < 0 && (errno != EAGAIN || errno != EWOULDBLOCK))
					{
						movefd(fd);
						unsigned int   ev = events[i].events;
						addEpoll(fd, EPOLL_CTL_DEL, ev);
						closefd(fd);
						ret = -1;
						break;
					}
					size -= ret;
					nAlready += ret;
				}
				if ( ret == -1) continue;
				unsigned int   ev = EPOLLERR|EPOLLIN;
				addEpoll(fd, EPOLL_CTL_MOD, ev);
			}
		}
	}
}

void chatServer::release()
{
	closefd(mEpollfd);
	closefd(mListenfd);
	if (mpUserData)
		delete[] mpUserData;
}

int chatServer::setNoBlocking(int fd)
{
	int32_t flags = ::fcntl(fd, F_GETFL, 0);
	flags |= O_NONBLOCK;
	int ret = ::fcntl(fd, F_SETFL, flags);
	return ret;
}

void chatServer::movefd(int fd)
{
	int idx = mpUserData[fd].index;
	mArrFd[idx] = mArrFd[--mClientNum];
	mpUserData[fd] = mpUserData[mArrFd[mClientNum]];
	mpUserData[fd].index = idx;
}

int chatServer::addEpoll(int fd, int op, unsigned int ev)
{
	struct epoll_event event;
	event.data.fd = fd;
	event.events = ev;
	return epoll_ctl(mEpollfd, op, fd, &event);
}

int chatServer::createNoBlockingfd()
{
	//
	mListenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (mListenfd == -1)
	{
		printf("socket error: %d\n", errno);
		return -1;
	}
	return setNoBlocking(mListenfd);
}


void chatServer::closefd(int fd)
{
	close(fd);
}

int main(int argc, char** argv)
{
	chatServer chat(argv[1], atoi(argv[2]));
	if (chat.createNoBlockingfd() == -1)
	{
		printf("createNoBlockingfd failed\n");
		goto ABORT;
	}
	if (chat.bindfd(100) == -1)
	{
		printf("bindfd failed\n");
		goto ABORT;
	}
	chat.runEvent(100000);
ABORT:
	chat.release();
	return 0;
}







