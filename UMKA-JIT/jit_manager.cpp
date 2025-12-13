#include "jit_manager.h"

namespace umka::jit {
void JitManager::worker_loop() {
  while (running) {
    size_t fid;

    // ждать задачу
    {
      std::unique_lock lock(queue_mutex);
      cv.wait(lock,
              [&] {
                return !queue.empty() || !running;
              });

      if (!running) return;

      fid = queue.front();
      queue.pop();
    }

    {
      std::lock_guard lock(state_mutex);
      jit_state[fid] = JitState::RUNNING;
    }

    // выполнить оптимизацию
    JittedFunction optimized = runner->optimize_function(fid);

    {
      std::lock_guard lock_data(data_mutex);
      jit_functions[fid] = std::move(optimized);
    }

    {
      std::lock_guard lock(state_mutex);
      jit_state[fid] = JitState::READY;
    }
  }
}

bool JitManager::has_jitted(size_t fid) {
  std::lock_guard lock(state_mutex);
  return jit_state[fid] == JitState::READY;
}

std::optional<std::reference_wrapper<const JittedFunction>> JitManager::try_get_jitted(size_t fid) {
  std::lock_guard lock(data_mutex);
  const auto it = jit_functions.find(fid);
  if (it == jit_functions.end()) {
    return std::nullopt;
  }
  return std::cref(it->second);
}

void JitManager::request_jit(size_t fid) {
  {
    std::lock_guard lock(state_mutex);

    if (jit_state[fid] != JitState::NONE)
      return;

    jit_state[fid] = JitState::QUEUED;
  }

  {
    std::lock_guard lock(queue_mutex);
    queue.push(fid);
  }

  cv.notify_one();
}
}
