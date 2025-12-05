#pragma once

#include "../UMKA-VM/model/model.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <stdexcept>
#include <cstdint>

#if defined(_WIN32)
  #define NOMINMAX
  #include <windows.h>
#elif defined(__unix__) || defined(__APPLE__)
  #include <unistd.h>
#endif

class GarbageCollector {
public:
  static constexpr double GC_PERCENT = 0.25;

  GarbageCollector()
      : bytes_allocated(0),
        gc_threshold(0),
        total_available_ram_bytes(0) {
    total_available_ram_bytes = detect_total_ram_bytes();
    gc_threshold = static_cast<size_t>(total_available_ram_bytes * GC_PERCENT);
  }

  void set_total_available_ram(size_t bytes) {
    total_available_ram_bytes = bytes;
    gc_threshold = static_cast<size_t>(total_available_ram_bytes * GC_PERCENT);
  }

  static size_t calculate_entity_size(const Entity& entity) {
    size_t size = sizeof(Entity);

    if (std::holds_alternative<std::map<int, Reference<Entity>>>(entity.value)) {
      const auto& arr = std::get<std::map<int, Reference<Entity>>>(entity.value);
      size += arr.size() * sizeof(std::pair<int, Reference<Entity>>);
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
    if (bytes_allocated >= bytes) {
      bytes_allocated -= bytes;
    } else {
      bytes_allocated = 0;
    }
  }

  bool should_collect() const {
    return bytes_allocated > gc_threshold;
  }

  void collect(
      std::vector<Owner<Entity>>& heap,
      const std::vector<Reference<Entity>>& operand_stack,
      const std::vector<StackFrame>& stack_of_functions
  ) {
    mark(heap, operand_stack, stack_of_functions);

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

  std::unordered_map<const Entity*, bool> marked_objects;

  std::unordered_map<const Entity*, bool> heap_objects;

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
        const Entity* ptr = owner.get();
        marked_objects[ptr] = false;
        heap_objects[ptr] = true;
      }
    }

    for (const auto& ref : operand_stack) {
      if (!ref.expired()) {
        auto owner = ref.lock();
        if (owner) {
          mark_recursive(owner.get());
        }
      }
    }

    for (const auto& frame : stack_of_functions) {
      for (const auto& [key, ref] : frame.name_resolver) {
        if (!ref.expired()) {
          auto owner = ref.lock();
          if (owner) {
            mark_recursive(owner.get());
          }
        }
      }
    }
  }

  void mark_recursive(const Entity* entity) {
    if (!entity) return;

    if (heap_objects.find(entity) == heap_objects.end()) {
      return;
    }

    if (marked_objects.find(entity) != marked_objects.end() && marked_objects[entity]) {
      return;
    }

    marked_objects[entity] = true;

    if (std::holds_alternative<std::map<int, Reference<Entity>>>(entity->value)) {
      const auto& arr = std::get<std::map<int, Reference<Entity>>>(entity->value);
      for (const auto& [key, ref] : arr) {
        if (!ref.expired()) {
          auto owner = ref.lock();
          if (owner) {
            mark_recursive(owner.get());
          }
        }
      }
    }
  }

  void sweep(std::vector<Owner<Entity>>& heap) {
    size_t freed_bytes = 0;

    auto it = heap.begin();
    while (it != heap.end()) {
      if (*it) {
        const Entity* entity_ptr = it->get();

        if (marked_objects.find(entity_ptr) == marked_objects.end() ||
            !marked_objects[entity_ptr]) {
          freed_bytes += calculate_entity_size(**it);
          it = heap.erase(it);
        } else {
          ++it;
        }
      } else {
        it = heap.erase(it);
      }
    }

    subtract_allocated_bytes(freed_bytes);

    heap.shrink_to_fit();
  }
};

