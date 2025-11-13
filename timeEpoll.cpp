#include "timeEpoll.h"
#include <sys/timerfd.h>
#include <iostream>
#include <unistd.h>
TimeEpoll::TimeEpoll() : vote_fd(0), time_out_fd(0), heart_fd(0)
{
    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0)
    {
        std::cerr << " epoll_create1 failed" << std::endl;
        exit(1);
    }
}

TimeEpoll::~TimeEpoll()
{
    if (epoll_fd > 0)
        close(epoll_fd);
}