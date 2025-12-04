#pragma once

#include "../UMKA-VM/model/model.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <stdexcept>
#include <cstdint>

class GarbageCollector {
public:
  // Константы
  static constexpr double GC_PERCENT = 0.25; // 25%

  // Конструктор
  GarbageCollector()
      : bytes_allocated(0),
        gc_threshold(0),
        total_available_ram_bytes(0) {
    // Получаем общий объем доступной RAM
    // Для Windows используем GetPhysicallyInstalledSystemMemory или приблизительную оценку
    // По умолчанию используем 8GB как базовое значение
    total_available_ram_bytes = 8ULL * 1024 * 1024 * 1024; // 8GB в байтах
    gc_threshold = static_cast<size_t>(total_available_ram_bytes * GC_PERCENT);
  }

  // Установка доступной памяти (опционально, для тестирования)
  void set_total_available_ram(size_t bytes) {
    total_available_ram_bytes = bytes;
    gc_threshold = static_cast<size_t>(total_available_ram_bytes * GC_PERCENT);
  }

  // Подсчет размера Entity
  static size_t calculate_entity_size(const Entity& entity) {
    size_t size = sizeof(Entity);

    // Если это массив, добавляем размер элементов
    if (std::holds_alternative<std::map<int, Reference<Entity>>>(entity.value)) {
      const auto& arr = std::get<std::map<int, Reference<Entity>>>(entity.value);
      size += arr.size() * sizeof(std::pair<int, Reference<Entity>>);
    }

    // Если это строка, добавляем размер строки
    if (std::holds_alternative<std::string>(entity.value)) {
      const auto& str = std::get<std::string>(entity.value);
      size += str.capacity(); // Приблизительный размер выделенной памяти
    }

    return size;
  }

  // Увеличить счетчик выделенной памяти
  void add_allocated_bytes(size_t bytes) {
    bytes_allocated += bytes;
  }

  // Уменьшить счетчик выделенной памяти
  void subtract_allocated_bytes(size_t bytes) {
    if (bytes_allocated >= bytes) {
      bytes_allocated -= bytes;
    } else {
      bytes_allocated = 0;
    }
  }

  // Проверка, нужно ли запустить GC
  bool should_collect() const {
    return bytes_allocated > gc_threshold;
  }

  // Основной метод сборки мусора (Stop the world)
  void collect(
      std::vector<Owner<Entity>>& heap,
      const std::vector<Reference<Entity>>& operand_stack,
      const std::vector<StackFrame>& stack_of_functions
  ) {
    // Mark phase
    mark(heap, operand_stack, stack_of_functions);

    // Sweep phase
    sweep(heap);
  }

  // Получить текущее количество выделенных байт
  size_t get_bytes_allocated() const {
    return bytes_allocated;
  }

  // Получить порог GC
  size_t get_gc_threshold() const {
    return gc_threshold;
  }

private:
  size_t bytes_allocated;
  size_t gc_threshold;
  size_t total_available_ram_bytes;

  // Карта для отслеживания пометок объектов
  // Ключ - указатель на Entity, значение - флаг is_marked
  std::unordered_map<const Entity*, bool> marked_objects;

  // Множество всех объектов в куче (для быстрой проверки)
  std::unordered_map<const Entity*, bool> heap_objects;

  // Mark phase: пометить все достижимые объекты
  void mark(
      std::vector<Owner<Entity>>& heap,
      const std::vector<Reference<Entity>>& operand_stack,
      const std::vector<StackFrame>& stack_of_functions
  ) {
    // 1. Сбросить все флаги и построить множество объектов кучи
    marked_objects.clear();
    heap_objects.clear();
    for (const auto& owner : heap) {
      if (owner) {
        const Entity* ptr = owner.get();
        marked_objects[ptr] = false;
        heap_objects[ptr] = true;
      }
    }

    // 2. Обход корней: стек операндов
    for (const auto& ref : operand_stack) {
      if (!ref.expired()) {
        auto owner = ref.lock();
        if (owner) {
          mark_recursive(owner.get());
        }
      }
    }

    // 3. Обход корней: стек вызовов (name_resolver каждой функции)
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

  // Рекурсивная пометка объекта и всех связанных с ним объектов
  void mark_recursive(const Entity* entity) {
    if (!entity) return;

    // Проверяем, что объект находится в куче
    if (heap_objects.find(entity) == heap_objects.end()) {
      return; // Объект не в куче, пропускаем
    }

    // Если уже помечен, пропускаем (избегаем циклов)
    if (marked_objects.find(entity) != marked_objects.end() && marked_objects[entity]) {
      return;
    }

    // Помечаем текущий объект
    marked_objects[entity] = true;

    // Если это массив, рекурсивно обходим все элементы
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

  // Sweep phase: удалить непомеченные объекты
  void sweep(std::vector<Owner<Entity>>& heap) {
    size_t freed_bytes = 0;

    // Удаляем непомеченные объекты
    auto it = heap.begin();
    while (it != heap.end()) {
      if (*it) {
        const Entity* entity_ptr = it->get();

        // Если объект не помечен, удаляем его
        if (marked_objects.find(entity_ptr) == marked_objects.end() ||
            !marked_objects[entity_ptr]) {
          // Подсчитываем освобожденную память
          freed_bytes += calculate_entity_size(**it);
          // Удаляем из кучи
          it = heap.erase(it);
        } else {
          ++it;
        }
      } else {
        // Если shared_ptr пустой, тоже удаляем
        it = heap.erase(it);
      }
    }

    // Уменьшаем счетчик выделенной памяти
    subtract_allocated_bytes(freed_bytes);

    // Сжимаем вектор
    heap.shrink_to_fit();

    // Проверяем, освободили ли достаточно памяти
    // Если после очистки все еще недостаточно памяти, выбрасываем ошибку
    // (это проверяется при следующей попытке выделения)
  }
};

