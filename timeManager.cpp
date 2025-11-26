#include "timeManager.h"

TimeManager::TimeManager(ThreadPool &pool) : threadPool(pool)
{
    consume_thread = std::thread(&TimeManager::run, this);
}

TimeManager::~TimeManager()
{
    stop();
}

void TimeManager::run()
{
    while (running)
    {
        std::unique_lock<std::mutex> lock(queue_mtx);

        if (queue.empty())
        {
            cv.wait(lock, [this]
                    { return !running || !queue.empty(); });
            if (!running)
            {
                return; // 正常退出
            }
        }

        auto now = std::chrono::steady_clock::now();
        auto timer = queue.top();

        if (timer->getTime() <= now)
        {
            queue.pop();
            // 提交任务到线程池
            threadPool.enqueue(timer->getCallback());
        }
        else
        {
            auto wait_time = std::chrono::duration_cast<std::chrono::milliseconds>(timer->getTime() - now);
            cv.wait_for(lock, wait_time, [this]
                        { return !running; });
        }
    }
}

void TimeManager::stop()
{
    {
        std::lock_guard<std::mutex> lock(queue_mtx);
        running = false;
    }
    cv.notify_all();
    if (consume_thread.joinable())
    {
        consume_thread.join();
    }
}

bool TimeManager::add_timer(std::shared_ptr<Timer> timer)
{
    {
        std::lock_guard<std::mutex> lock(queue_mtx);
        queue.push(timer);
    }
    cv.notify_one();
    return true;
}
