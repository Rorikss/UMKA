#include "jit_manager.h"
#include "const_folding.h"
#include "dce.h"
#include "constant_propagation.h"

namespace umka::jit {
JitManager::JitManager(std::vector<vm::Command> &commands,
                       std::vector<vm::Constant> &const_pool,
                       std::unordered_map<size_t, vm::FunctionTableEntry> &func_table)
  : runner(std::make_unique<JitRunner>(commands, const_pool, func_table)),
    func_table(func_table) {
  for (const auto &id: func_table | std::views::keys) {
    jit_state[id] = JitState::NONE;
  }
  runner->add_optimization(std::make_unique<ConstantPropagation>());
  runner->add_optimization(std::make_unique<ConstFolding>());
  runner->add_optimization(std::make_unique<ConstantPropagation>());
  runner->add_optimization(std::make_unique<DeadCodeElimination>());
  running = true;
  worker = std::thread([this] { worker_loop(); });
}

void JitManager::worker_loop() {
  while (running) {
    size_t fid;

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

std::optional<std::reference_wrapper<const JittedFunction> >
JitManager::try_get_jitted(size_t fid) {
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
