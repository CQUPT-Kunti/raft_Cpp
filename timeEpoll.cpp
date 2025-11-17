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

TimeEpoll::TimeEpoll() : vote_fd(0), heart_fd(0)
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
                                            if (fd == vote_fd)
                                            {
                                                // voteCallback();
                                            }
                                            if (fd == heart_fd)
                                            {
                                                // heartCallback();
                                            }

                                        }
                                        
                                    }
                                   } });
    std::cout << "time_ epoll is starting" << std::endl;
}
