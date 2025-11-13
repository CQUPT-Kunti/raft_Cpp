#pragma once;

#include <sys/epoll.h>

class TimeEpoll
{
public:
    TimeEpoll();
    ~TimeEpoll();

private:
    int epoll_fd;
    int vote_fd;
    int heart_fd;
    int time_out_fd;
};