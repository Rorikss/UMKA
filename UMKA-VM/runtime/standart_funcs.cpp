#include "standart_funcs.h"
#include "../model/model.h"
#include <cmath>
#include <iostream>
#include <fstream>
#include <ostream>
#include <sstream>
#include <variant>
#include <vector>

namespace umka::vm {
void out(std::ostream& os, Entity entity) {
    os << entity.to_string() << "\n";
}

void print(Entity entity) {
    out(std::cout, entity);
}

void write(const std::string& filename, Entity entity) {
    auto file = std::ofstream(filename);
    out(file, entity);
}

std::vector<std::string> read(const std::string& filename) {
    auto file = std::ifstream(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
    
    std::vector<std::string> lines;
    std::string line;
    
    while (std::getline(file, line)) {
        lines.emplace_back(std::move(line));
    }
    
    return lines;
}

int64_t len(Entity entity) {
    if (std::holds_alternative<Owner<Array>>(entity.value)) {
        return std::get<Owner<Array>>(entity.value)->size();
    } else if (std::holds_alternative<std::string>(entity.value)) {
        return std::get<std::string>(entity.value).size();
    }
    throw std::runtime_error("Invalid type for len()");
}

void add_elem(Entity array, Reference<Entity> elem) {
    auto owner = std::get<Owner<Array>>(array.value);
    owner->emplace_back(elem);
}

void remove(Entity array, int64_t index) {
    auto& arr = std::get<Owner<Array>>(array.value);
    if (index < 0 || index >= static_cast<int64_t>(arr->size())) {
        throw std::out_of_range("Array index out of bounds");
    }

    arr->erase(arr->begin() + index);
}

Reference<Entity> get(Entity array, int64_t index) {
    auto& map = std::get<Owner<Array>>(array.value);
    if (index < 0 || index >= static_cast<int64_t>(map->size())) {
        throw std::out_of_range("Array index out of bounds");
    }
    return map->at(index);
}

void set(Entity array, int64_t index, Reference<Entity> elem) {
    auto& map = std::get<Owner<Array>>(array.value);
    if (index < 0 || index >= static_cast<int64_t>(map->size())) {
        throw std::out_of_range("Array index out of bounds");
    }
    (*map)[index] = elem;
}

void umka_assert(Entity& condition) {
    if (!std::get<bool>(condition.value)) {
        throw std::runtime_error("Assertion failed");
    }
}

std::string input() {
    std::string line;
    std::getline(std::cin, line);
    return line;
}

double random() {
    return static_cast<double>(std::rand()) / RAND_MAX;
}

double pow(double base, double exp) {
    return std::pow(base, exp);
}   

double sqrt(double number) {
    return std::sqrt(number);
}
}
