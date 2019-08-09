#include "stdafx.h"
#include "dispatch_queue.h"

using namespace std;


dispatch_queue::dispatch_queue(std::string name, size_t thread_cnt) :
    name_(name), threads_(thread_cnt)
{
    printf("Creating dispatch queue: %s\n", name.c_str());
    printf("Dispatch threads: %zu\n", thread_cnt);

    for (size_t i = 0; i < threads_.size(); i++)
    {
        threads_[i] = std::thread(&dispatch_queue::dispatch_thread_handler, this);
    }
}

dispatch_queue::~dispatch_queue()
{
    printf("Destructor: Destroying dispatch threads...\n");

    // Signal to dispatch threads that it's time to wrap up
    std::unique_lock<std::mutex> lock(lock_);
    quit_ = true;
    lock.unlock();
    cv_.notify_all();

    // Wait for threads to finish before we exit
    for (size_t i = 0; i < threads_.size(); i++)
    {
        if (threads_[i].joinable())
        {
            printf("Destructor: Joining thread %zu until completion\n", i);
            threads_[i].join();
        }
    }
}

bool dispatch_queue::dispatch(int op_type, const vector<int>& skip_if_op_type_queued, const fp_t& op)
{
    std::unique_lock<std::mutex> lock(lock_);

    for (const auto& qop : q_) {
        for (const auto& skip : skip_if_op_type_queued) {
            if (skip == qop.first) {
                return false;
            }
        }
    }
    q_.emplace_back(op_type, op);

    // Manual unlocking is done before notifying, to avoid waking up
    // the waiting thread only to block again (see notify_one for details)
    lock.unlock();
    cv_.notify_all();

    return true;
}

bool dispatch_queue::dispatch(int op_type, const vector<int>& skip_if_op_type_queued, fp_t&& op)
{
    std::unique_lock<std::mutex> lock(lock_);

    for (const auto& qop : q_) {
        for (const auto& skip : skip_if_op_type_queued) {
            if (skip == qop.first) {
                return false;
            }
        }
    }
    q_.emplace_back(op_type, std::move(op));

    // Manual unlocking is done before notifying, to avoid waking up
    // the waiting thread only to block again (see notify_one for details)
    lock.unlock();
    cv_.notify_all();

    return true;
}

void dispatch_queue::dispatch_thread_handler(void)
{
    std::unique_lock<std::mutex> lock(lock_);

    do {
        //Wait until we have data or a quit signal
        cv_.wait(lock, [this] {
            return (q_.size() || quit_);
        });

        //after wait, we own the lock
        if (!quit_ && q_.size())
        {
            auto op = std::move(q_.front().second);
            q_.pop_front();

            //unlock now that we're done messing with the queue
            lock.unlock();

            op();

            lock.lock();
        }
    } while (!quit_);
}
