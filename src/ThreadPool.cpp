#include "ThreadPool.h"



////////////////////////////////////////////////////////////////////////////////////////

void threadPool::Start()
{
    uint32_t NumThreads = std::thread::hardware_concurrency();
    // NumThreads=1;
    
    Threads.resize(NumThreads);
    Finished.resize(NumThreads, true);
    for(uint32_t i=0; i<NumThreads; i++)
    {
        Threads[i] = std::thread([this, i]()
        {
            while(true)
            {
                std::function<void()> Job;

                {
                    //Wait for the job to not be empty, or for the souldTerminate signal
                    std::unique_lock<std::mutex> Lock(QueueMutex);
                    
                    //Locks the thread until MutexCondition is being signalized, or if the jobs queue is not empty, or if we should terminate
                    MutexCondition.wait(Lock, [this]{
                        return !Jobs.empty() || ShouldTerminate;
                    });

                    if(ShouldTerminate) return;

                    Job = Jobs.front();
                    Jobs.pop();
                    Finished[i] = false;
                }

                Job();
                Finished[i]=true;
            }       
        });
    }

}

void threadPool::EnqueueJob(const std::function<void()>& Job)
{
    {
        std::unique_lock<std::mutex> Lock(QueueMutex);
        Jobs.push(Job);
    }
    //Notifies one thread waiting on this condition to be signalized
    MutexCondition.notify_one();
}

void threadPool::Stop()
{
    {
        std::unique_lock<std::mutex> lock(QueueMutex);
        ShouldTerminate=true;
    }

    MutexCondition.notify_all();
    for(int i=0; i<Threads.size(); i++)
    {
        Threads[i].join();
    }
    Threads.clear();
}

bool threadPool::Busy()
{
    bool Busy;
    {
        std::unique_lock<std::mutex> Lock(QueueMutex);
        bool AllFinished=true;
        for(int i=0; i<Finished.size(); i++)
        {
            if(!Finished[i])
            {
                AllFinished=false;
                break;
            }
        }
        Busy = !AllFinished;
    }  

    return Busy;
}

////////////////////////////////////////////////////////////////////////////////////////

