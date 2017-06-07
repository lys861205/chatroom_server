#include "server.h"

void ChatRoom::join(ChatSession* session)
{
	session_.insert(session);
	deque<string>::iterator it = recent_msgs.begin();
	for ( ; it != recent_msgs.end(); ++it )
	{
		cout << "send " << *it << endl;
		session->deliver(*it);
	}
}
void ChatRoom::leave(ChatSession* session)
{
	session_.erase(session);
}
void ChatRoom::deliver(const string& msg, ChatSession* outSession)
{
	recent_msgs.push_back(msg);
	while ( recent_msgs.size() > 100 )
		recent_msgs.pop_front();

	set<ChatSession*>::iterator it = session_.begin();
	for ( ; it != session_.end(); ++it )
	{
		if ( *it != outSession )
		{
			(*it)->deliver(msg);
		}
	}
}

ChatServer::ChatServer(short& nPort):
	mListenFd(-1),
	mepollId(epoll_create(256)),
	mPort(nPort)
{	
}
int ChatServer::openServer(int blockflag)
{
	if ( -1==createNonBlockFd() )
	{
		printf("create non block socket failed\n");
		return -1;
	}
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(mPort);
	addr.sin_addr.s_addr = INADDR_ANY;
	int ret = 0;
	unsigned int ev = EPOLLIN|EPOLLERR;
	ret = addEpoll(mListenFd, EPOLL_CTL_ADD, ev, NULL);
	if ( -1 == ret )
	{
		printf("add socket to epoll failed, error: %d\n", errno);
		return ret;
	}
	int reuse = 1;
	setsockopt(mListenFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	ret = bind(mListenFd, (const sockaddr*)&addr, sizeof(addr));
	if ( -1 == ret )
	{
		printf("bind socket failed, error: %d\n", errno);
		return ret;
	}
	ret = listen(mListenFd, blockflag);
	if ( -1 == ret )
	{
		printf("listen socket failed, error: %d\n", errno);
		return ret;
	}
	return ret;
}

int ChatServer::closeServer()
{
	closeFd(mListenFd);
	closeFd(mepollId);
}
int ChatServer::runServer(int milliseconds)
{
	printf("running server[:%d]\n", mPort);	

	struct epoll_event events[EVENT_LIMIT];
	int ready = 0;
	while (1)
	{
		ready = ::epoll_wait(mepollId, events, EVENT_LIMIT, milliseconds);
		if ( ready == -1 )
		{
			if ( errno == EINTR )
			{
				printf("..");
				continue;
			}
			printf("epoll_wait failed, error: %d, %s\n", errno, strerror(errno));
			break;
		}
		else if ( ready == 0 )
		{
			printf("epoll_wait timeout\n");
			continue;
		}
		for ( int i=0; i<ready; ++i )
		{
			if ( mListenFd == events[i].data.fd )
			{
				createSession(mListenFd);	
			}
			else if  (events[i].events & EPOLLHUP || events[i].events & EPOLLERR )
			{
				ChatSession* pSession = (ChatSession*)events[i].data.ptr;
				if ( pSession == NULL ) continue;
				int fd = pSession->getfd();
				addEpoll(fd, EPOLL_CTL_DEL, events[i].events, NULL);
				closeFd(fd);
				mSessionMap.erase(fd);
				delete pSession;
			}
			else if ( events[i].events & EPOLLIN )
			{
				ChatSession* pSession = (ChatSession*)events[i].data.ptr;
				if ( pSession == NULL ) continue;
				if ( pSession->reciver() == -1 )
				{
					int fd = pSession->getfd();
					addEpoll(fd, EPOLL_CTL_DEL, events[i].events, NULL);
					closeFd(fd);
					mSessionMap.erase(fd);
					delete pSession;
				}
			}
			else if ( events[i].events & EPOLLOUT )
			{
				ChatSession* pSession = (ChatSession*)events[i].data.ptr;
				if ( pSession == NULL ) continue;
				if ( pSession->deliver() == -1 )
				{
					int fd = pSession->getfd();
					addEpoll(fd, EPOLL_CTL_DEL, events[i].events, NULL);
					closeFd(fd);
					mSessionMap.erase(fd);
					delete pSession;
				}
			}
		}

	}
}


int ChatServer::addEpoll(int fd, int op, unsigned int ev, void* p)
{
	struct epoll_event event;
	event.data.fd = fd;
	event.events  = ev;
	if ( NULL != p )
		event.data.ptr= p;
	return ::epoll_ctl(mepollId, op, fd, &event);
}
void ChatServer:: closeFd(int fd)
{
	close(fd);
}
int ChatServer:: createSession(int fd)
{
	struct sockaddr_in raddr;
	socklen_t len = sizeof(raddr);
	int connfd = ::accept(fd, (struct sockaddr*)&raddr, &len);
	if ( connfd == -1 && ( errno != EAGAIN) )
	{
		printf("accept failed, error: %d\n", errno);
		return -1;
	}
	setFdNonBlock(connfd);
	ChatSession* chat = new ChatSession(connfd, mRoom,this); 
	mSessionMap.insert(std::make_pair(connfd, chat));
	unsigned int ev = EPOLLIN|EPOLLERR|EPOLLHUP;
	addEpoll(connfd, EPOLL_CTL_ADD, ev, (void*)chat);
	return 0;
}
int ChatServer:: createNonBlockFd()
{
	mListenFd = socket(AF_INET, SOCK_STREAM, 0);
	if ( mListenFd == -1 )
	{
		printf("sokcet failed, error: %d\n", errno);
		return -1;
	}
	return setFdNonBlock(mListenFd);
}
int ChatServer:: setFdNonBlock(int fd)
{
	int32_t flags = ::fcntl(fd, F_GETFL, 0);
	flags |= O_NONBLOCK;
	return ::fcntl(fd, F_SETFL, flags);
}

ChatSession:: ChatSession(int fd, ChatRoom& room, ChatServer* server):
	mfd(fd),
	room_(room),
	server_ptr_(server),
	is_send_enable_(false)
{
	room_.join(this);
}
int ChatSession:: deliver(const string& msg)
{
	m_send_msgs.push_back(msg);
	int ret = deliver();
	if ( ret == -1 )
	{
		room_.leave(this);
		close(mfd);
	}
	return ret;
}
int ChatSession:: deliver()
{
	int ret = 0;
	while ( !m_send_msgs.empty() )
	{
		string& outmsg = m_send_msgs.front();
		ret = send(mfd, outmsg.c_str(), outmsg.size(), 0);
		if ( ret < 0 && errno == EAGAIN )
		{
			if ( !is_send_enable_)
			{
				unsigned int ev = EPOLLOUT|EPOLLERR|EPOLLIN;
				server_ptr_->addEpoll(mfd, EPOLL_CTL_MOD, ev, (void*)this); 	
				is_send_enable_ = true;
			}
			return 0;
		}
		else if ( ret < 0 )
		{
			return -1;
		}
		else 
		{
			m_send_msgs.pop_front();
		}
	}
	if (is_send_enable_)
	{
		unsigned int ev = EPOLLIN|EPOLLERR;
		server_ptr_->addEpoll(mfd, EPOLL_CTL_MOD, ev, (void*)this); 	
		is_send_enable_ = false;
		return 0;
	}
}

int ChatSession:: reciver()
{
	char buf[msg_len] = {0};
	int ret = recv(mfd, buf, msg_len-1, 0);
	if ( ret== 0 || (ret < 0 && errno != EAGAIN ) )
	{
		return -1;
	}
	else if ( ret > 0 )
	{
		m_recv_msg.assign(buf);
		room_.deliver(m_recv_msg, this);
	}
	return 0;
}


int main()
{
	short port = 2888;
	ChatServer server(port);
	if ( -1 == server.openServer(100) ) 
	{
		printf("open server failed.\n");
		return 0;
	}
	server.runServer(10000);
	return 0;
}
