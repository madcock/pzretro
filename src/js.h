#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#include "duktape.h"

namespace js {

class Context {
    // Mutex for all external access to js context
    std::mutex mutex;
    duk_context *ctx;
    std::thread js_thread; 
    std::atomic<bool> js_thread_active;
    std::string thread_code{};
    std::string thread_filename{};
    void thread_loop();
public:
    Context();
    ~Context();

    // Eval js code
    void eval(std::string code, std::string filename);

    // Start thread, keep calling code in a loop until stop_thread() is called
    void start_thread(std::string code, std::string filename);

    // Stop thread from calling it's code
    void stop_thread();

    // Used by fatal error handler
    void backtrace();
};

} // namespace js
