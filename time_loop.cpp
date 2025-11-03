#include "time_loop.h"

TimeEvent::TimeEvent()
{
    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0)
    {
        std::cerr << " epoll_create1 failed" << std::endl;
        exit(1);
    }
}

TimeEvent::~TimeEvent()
{
    if (epoll_fd > 0)
        close(epoll_fd);
}

itimerspec TimeEvent::make_random_time(int min_ms, int max_ms)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(min_ms, max_ms);

    int interval = dist(gen);
    itimerspec spec{};
    spec.it_value.tv_sec = interval / 1000;
    spec.it_value.tv_nsec = (interval % 1000) * 1000000;
    spec.it_interval = {0, 0};
    return spec;
}

int TimeEvent::addRandomTimer(int min_ms, int max_ms, const Callback &cb)
{
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (tfd < 0)
    {
        std::cerr << " timerfd_create failed" << std::endl;
        exit(1);
    }

    itimerspec spec = make_random_time(min_ms, max_ms);
    timerfd_settime(tfd, 0, &spec, nullptr);

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = tfd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tfd, &ev);

    callbacks_[tfd] = cb;
    return tfd;
}

void TimeEvent::addFd(int fd, const Callback &cb)
{
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
    callbacks_[fd] = cb;
}

void TimeEvent::handleEvent(int fd)
{
    uint64_t expirations;
    read(fd, &expirations, sizeof(expirations)); // 清 timerfd
    if (callbacks_.count(fd))
    {
        callbacks_[fd]();
        callbacks_.erase(fd); // 一次性触发
        close(fd);
    }
}

void TimeEvent::pollOnce()
{
    epoll_event events[10];
    int nfds = epoll_wait(epoll_fd, events, 10, 0); // 非阻塞检查
    for (int i = 0; i < nfds; ++i)
    {
        handleEvent(events[i].data.fd);
    }
}
