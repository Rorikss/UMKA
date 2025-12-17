#pragma once

#include <unordered_map>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <optional>
#include <functional>

#include "jitted_function.h"
#include "jit_runner.h"

namespace umka::jit {
enum class JitState {
  NONE,
  QUEUED,
  RUNNING,
  READY
};

class JitManager {
  public:
    JitManager(std::vector<vm::Command> &commands,
               std::vector<vm::Constant> &const_pool,
               std::unordered_map<size_t, vm::FunctionTableEntry> &func_table);

    ~JitManager() {
      running = false;
      cv.notify_all();
      if (worker.joinable())
        worker.join();
    }

    // начало обработки функции
    void request_jit(size_t fid);

    // проверка есть ли готовая версия функции
    bool has_jitted(size_t fid);

    //попробовать взять jitted функцию
    std::optional<std::reference_wrapper<const JittedFunction>> try_get_jitted(size_t fid);

  private:
    void worker_loop();

    std::unique_ptr<JitRunner> runner;

    std::unordered_map<size_t, vm::FunctionTableEntry> &func_table;

    std::unordered_map<size_t, JitState> jit_state;
    std::unordered_map<size_t, JittedFunction> jit_functions;

    std::queue<size_t> queue;
    std::mutex queue_mutex;

    std::mutex state_mutex;
    std::mutex data_mutex;

    std::condition_variable cv;
    std::thread worker;
    std::atomic<bool> running{false};
};
} // namespace umka::jit
