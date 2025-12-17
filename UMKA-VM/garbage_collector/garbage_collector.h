#pragma once

#include <model/model.h>

#include <iostream>
#include <unordered_map>
#include <vector>
#include <memory>
#include <stdexcept>
#include <cstdint>

#if defined(_WIN32)
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#elif defined(__unix__) || defined(__APPLE__)
  #include <unistd.h>
#endif

namespace umka::vm {
template<typename Tag = ReleaseMod>
class GarbageCollector {
public:
  static constexpr double GC_PERCENT = 0.01;

  GarbageCollector()
      : bytes_allocated(0)
      , gc_threshold(0)
      , total_available_ram_bytes(0)
      , after_last_clean(0)
      {
    total_available_ram_bytes = detect_total_ram_bytes();
    gc_threshold = static_cast<size_t>(total_available_ram_bytes * GC_PERCENT);
  }

  void set_total_available_ram(size_t bytes) {
    total_available_ram_bytes = bytes;
    gc_threshold = static_cast<size_t>(total_available_ram_bytes * GC_PERCENT);
  }

  static size_t calculate_entity_size(const Entity& entity) {
    size_t size = sizeof(Entity);

    if (std::holds_alternative<Owner<Array>>(entity.value)) {
      auto arr = std::get<Owner<Array>>(entity.value);
      size += arr->size() * sizeof(std::pair<size_t, Reference<Entity>>);
    }

    if (std::holds_alternative<std::string>(entity.value)) {
      const auto& str = std::get<std::string>(entity.value);
      size += str.capacity();
    }

    return size;
  }

  void add_allocated_bytes(size_t bytes) {
    bytes_allocated += bytes;
  }

  void subtract_allocated_bytes(size_t bytes) {
    if constexpr (std::is_same_v<Tag, DebugMod>) {
      std::cout << "Subtracted: " << bytes << " bytes" << std::endl;
    }
    if (bytes_allocated >= bytes) {
      bytes_allocated -= bytes;
    } else {
      bytes_allocated = 0;
    }
  }

  bool should_collect() const {
    return (bytes_allocated - after_last_clean) > gc_threshold;
  }

  void collect(
      std::vector<Owner<Entity>>& heap,
      const std::vector<Reference<Entity>>& operand_stack,
      const std::vector<StackFrame>& stack_of_functions
  ) {
    if constexpr (std::is_same_v<Tag, DebugMod>) {
      std::cout << "Mark" << std::endl;
    }
    mark(heap, operand_stack, stack_of_functions);

    if constexpr (std::is_same_v<Tag, DebugMod>) {
      std::cout << "Sweep" << std::endl;
    }
    sweep(heap);
  }

  size_t get_bytes_allocated() const {
    return bytes_allocated;
  }

  size_t get_gc_threshold() const {
    return gc_threshold;
  }

private:
  size_t bytes_allocated;
  size_t gc_threshold;
  size_t total_available_ram_bytes;
  size_t after_last_clean;

  std::unordered_map<Owner<Entity>, bool> marked_objects;

  std::unordered_map<Owner<Entity>, bool> heap_objects;

  static size_t detect_total_ram_bytes() {
#if defined(_WIN32)
    // Вариант 1: GlobalMemoryStatusEx (дает общий объем физической памяти)
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
      return static_cast<size_t>(statex.ullTotalPhys);
    }

    // Вариант 2: GetPhysicallyInstalledSystemMemory (KB)
    // Оставлен как резервный, если нужно, но не обязателен.
    // DWORDLONG total_kb = 0;
    // if (GetPhysicallyInstalledSystemMemory(&total_kb)) {
    //   return static_cast<size_t>(total_kb) * 1024;
    // }

    // Фолбэк: 8GB, если системные вызовы не сработали
    return 8ULL * 1024 * 1024 * 1024;
#elif defined(__unix__) || defined(__APPLE__)
    // POSIX: количество страниц * размер страницы
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && page_size > 0) {
      return static_cast<size_t>(pages) * static_cast<size_t>(page_size);
    }
    // Фолбэк: 8GB
    return 8ULL * 1024 * 1024 * 1024;
#else
    return 8ULL * 1024 * 1024 * 1024;
#endif
  }

  void mark(
      std::vector<Owner<Entity>>& heap,
      const std::vector<Reference<Entity>>& operand_stack,
      const std::vector<StackFrame>& stack_of_functions
  ) {
    marked_objects.clear();
    heap_objects.clear();
    for (const auto& owner : heap) {
      if (owner) {
        marked_objects[owner] = false;
        heap_objects[owner] = true;
      }
    }

    for (const auto& ref : operand_stack) {
      if (!ref.expired()) {
        auto owner = ref.lock();
        if (owner) {
          mark_recursive(owner);
        }
      }
    }

    for (const auto& frame : stack_of_functions) {
      for (const auto& [key, ref] : frame.name_resolver) {
        if (!ref.expired()) {
          auto owner = ref.lock();
          if (owner) mark_recursive(owner);
        }
      }
    }
  }

  void mark_recursive(Owner<Entity> entity_owner) {
    if (!entity_owner) return;

    if (!heap_objects.contains(entity_owner)) {
      return;
    }

    if (marked_objects.contains(entity_owner) && marked_objects[entity_owner]) {
      return;
    }

    marked_objects[entity_owner] = true;

    if (!std::holds_alternative<Owner<Array>>(entity_owner->value)) {
      return;
    }

    const auto& arr = std::get<Owner<Array>>(entity_owner->value);
    for (const auto& ref : *arr) {
      if (!ref.expired()) {
        auto owner = ref.lock();
        if (owner) mark_recursive(owner);
      }
    }
  }

  void sweep(std::vector<Owner<Entity>>& heap) {
    size_t freed_bytes = 0;

    auto erase = [&heap](auto it) {
        if (it != std::prev(heap.end())) {
          std::swap(*it, heap.back());
        }
        heap.pop_back();
    };

    if constexpr (std::is_same_v<Tag, DebugMod>) {
      std::cout << "Heap size: " << heap.size() << std::endl;
    }

    auto it = heap.begin();
    while (it != heap.end()) {
      if (!(*it)) {
        erase(it);
        continue;
      }
      const Owner<Entity>& entity_owner = *it;

      if (!marked_objects.contains(entity_owner) || !marked_objects[entity_owner]) {
        freed_bytes += calculate_entity_size(**it);
        erase(it);
      } else {
        ++it;
      }
    }

    subtract_allocated_bytes(freed_bytes);
    after_last_clean = bytes_allocated;

    heap.shrink_to_fit();
    if constexpr (std::is_same_v<Tag, DebugMod>) {
      std::cout << "New heap size: " << heap.size() << std::endl;
    }
  }
};
}
