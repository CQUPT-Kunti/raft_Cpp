#include "timeEpoll.h"
#include <sys/timerfd.h>
#include <iostream>
#include <unistd.h>
#include <random>
// TimeEpoll::TimeEpoll(func vote_func_, func heart_func_)
//     : vote_fd(0), heart_fd(0),
//       voteCallback(vote_func_), heartCallback(heart_func_)
// {
//     epoll_fd = epoll_create1(0);
//     if (epoll_fd < 0)
//     {
//         std::cerr << " epoll_create1 failed" << std::endl;
//         exit(1);
//     }
// }

TimeEpoll::TimeEpoll() : vote_fd(-1), heart_fd(-1)
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

    if (timeEpoll_thread.joinable())
        timeEpoll_thread.join();
}

int TimeEpoll::randomBetween(int minMs, int maxMs)
{
    static thread_local std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<int> dist(minMs, maxMs);
    return dist(gen);
}

void TimeEpoll::resetOutTime(TimerArgs &time_arg, int min_ms, int max_ms)
{
    std::unique_lock<std::shared_mutex> time_lock(time_mtx);
    time_arg.startTime = std::chrono::steady_clock::now();
    time_arg.middleTime = std::chrono::milliseconds(randomBetween(min_ms, max_ms));
    time_arg.endTime = std::chrono::steady_clock::now() + time_arg.middleTime;
}

void TimeEpoll::addVoteEvent(func voteFunction)
{
    {
        std::unique_lock<std::shared_mutex> lock(time_mtx);
        voteCallback = voteFunction;
        voteState = true;
    }
    resetOutTime(vote_time_arg, 150, 600);
    if (vote_fd == -1)
    {
        vote_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
        struct epoll_event ev{};
        if (vote_fd < 0)
        {
            std::cerr << "timerfd_create() failed!" << std::endl;
            return;
        }
        ev.events = EPOLLIN;
        ev.data.fd = vote_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, vote_fd, &ev) < 0)
        {
            std::cerr << "epoll_ctl ADD vote_fd failed!" << std::endl;
            exit(0);
        }
    }
    auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(
        vote_time_arg.endTime - vote_time_arg.startTime);
    long ms = remain.count();
    if (ms < 1)
        ms = 1;
    struct itimerspec newValue{};
    newValue.it_value.tv_sec = ms / 1000;
    newValue.it_value.tv_nsec = (ms % 1000) * 1000000;

    // 一次性触发
    newValue.it_interval.tv_sec = 0;
    newValue.it_interval.tv_nsec = 0;

    timerfd_settime(vote_fd, 0, &newValue, nullptr);
}

void TimeEpoll::TimeServiceStart()
{
    timeEpoll_thread = std::thread([this]()
                                   { struct epoll_event events[9];
                                   while (true)
                                   {
                                    int nfds = epoll_wait(epoll_fd, events, 9, -1);
                                    if (nfds < 0) {
                                        if (errno == EINTR) continue; // 被信号打断，继续等待
                                        std::cerr << "epoll_wait error" << std::endl;
                                        break;
                                    }
                                    for (int i = 0; i < nfds; ++i){
                                        int fd = events[i].data.fd;
                                        uint64_t expirations;
                                        size_t  s = read(fd,&expirations,sizeof(expirations));
                                        if (s != sizeof(expirations))
                                        {
                                            std::cerr<<"read timerfd error"<<std::endl;
                                            continue;
                                        }

                                        {
                                            std::unique_lock<std::shared_mutex> lock(time_mtx);
                                            if (fd == vote_fd&&voteState == true)
                                            {
                                                voteCallback();
                                            }
                                            if (fd == heart_fd&&heartState == true)
                                            {
                                                // heartCallback();
                                            }

                                        }
                                        
                                    }
                                   } });
    std::cout << "time_ epoll is starting" << std::endl;
}
