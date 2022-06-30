#pragma once

#include <thread>
#include <mutex>
#include <functional>
#include <vector>
#include <queue>

struct threadPool
{
    void Start();
    void EnqueueJob(const std::function<void()>& Job);
    void Stop();
    bool Busy();

    bool ShouldTerminate=false;
    std::mutex QueueMutex;
    
    //Mutex for access of the jobs queue
    std::condition_variable MutexCondition;

    std::vector<std::thread> Threads;
    std::queue<std::function<void()>> Jobs;  
    std::vector<bool> Finished;
};

