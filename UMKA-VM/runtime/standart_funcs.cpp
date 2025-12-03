#include "standart_funcs.h"
#include "../model/model.h"
#include <iostream>
#include <fstream>
#include <ostream>
#include <sstream>
#include <variant>
#include <vector>

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
    return std::get<Owner<Array>>(entity.value)->size();
}

void add_elem(Entity array, Reference<Entity> elem) {
    auto owner = std::get<Owner<Array>>(array.value);
    auto size = owner->size();
    (*owner)[size] = elem;
}

void remove(Entity array, int64_t index) {
    auto& map = std::get<Owner<Array>>(array.value);
    if (index < 0 || index >= static_cast<int64_t>(map->size())) {
        throw std::out_of_range("Array index out of bounds");
    }

    for (auto it = map->lower_bound(index); it != map->end(); ++it) {
        (*map)[it->first - 1] = it->second;
    }
    map->erase(std::prev(map->end()));
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