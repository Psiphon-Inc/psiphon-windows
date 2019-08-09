#pragma once

// Adapted from https://github.com/embeddedartistry/embedded-resources/blob/e2e7bf23846efa6234f5267a97656d75a8175357/examples/cpp/dispatch.cpp
// and https://embeddedartistry.com/blog/2017/2/1/c11-implementing-a-dispatch-queue-using-stdfunction
// which is licensed CC0

#include <functional>
#include <mutex>
#include <vector>
#include <deque>

class dispatch_queue {
    typedef std::function<void(void)> fp_t;

public:
    dispatch_queue(std::string name, size_t thread_cnt = 1);
    ~dispatch_queue();

    // dispatch and copy
    bool dispatch(int op_type, const vector<int>& skip_if_op_queued, const fp_t& op);
    // dispatch and move
    bool dispatch(int op_type, const vector<int>& skip_if_op_queued, fp_t&& op);

    // Deleted operations
    dispatch_queue(const dispatch_queue& rhs) = delete;
    dispatch_queue& operator=(const dispatch_queue& rhs) = delete;
    dispatch_queue(dispatch_queue&& rhs) = delete;
    dispatch_queue& operator=(dispatch_queue&& rhs) = delete;

private:
    std::string name_;
    std::mutex lock_;
    std::vector<std::thread> threads_;
    std::deque<std::pair<int, fp_t>> q_;
    std::condition_variable cv_;
    bool quit_ = false;

    void dispatch_thread_handler(void);
};
