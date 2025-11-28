#include "model.h"
#include <iostream>
#include <fstream>
#include <ostream>
#include <sstream>
#include <variant>
#include <vector>

void out(std::ostream& os, Entity entity) {
    os << entity.to_string();
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

int64_t len(Entity& entity) {
    return std::get<std::map<size_t, Reference<Entity>>>(entity.value).size();
}

void add_elem(Entity& array, Reference<Entity> elem) {
    auto size = std::get<std::map<size_t, Reference<Entity>>>(array.value).size();
    std::get<std::map<size_t, Reference<Entity>>>(array.value)[size] = elem;
}

void remove(Entity& array, int64_t index) {
    auto& map = std::get<std::map<size_t, Reference<Entity>>>(array.value);
    if (index < 0 || index >= static_cast<int64_t>(map.size())) {
        throw std::out_of_range("Array index out of bounds");
    }
    map.erase(index);
    
    std::map<size_t, Reference<Entity>> new_map;
    size_t new_index = 0;
    for (auto& [_, value] : map) {
        new_map[new_index++] = value;
    }
    array.value = new_map;
}

Reference<Entity> get(Entity& array, int64_t index) {
    auto& map = std::get<std::map<size_t, Reference<Entity>>>(array.value);
    if (index < 0 || index >= static_cast<int64_t>(map.size())) {
        throw std::out_of_range("Array index out of bounds");
    }
    return map.at(index);
}

void set(Entity& array, int64_t index, Reference<Entity> elem) {
    auto& map = std::get<std::map<size_t, Reference<Entity>>>(array.value);
    if (index < 0 || index >= static_cast<int64_t>(map.size())) {
        throw std::out_of_range("Array index out of bounds");
    }
    map[index] = elem;
}
