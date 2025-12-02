#include "standart_funcs.h"
#include "../model/model.h"
#include <iostream>
#include <fstream>
#include <ostream>
#include <sstream>
#include <variant>
#include <vector>

void out(std::ostream& os, Reference<Entity> entity) {
    os << entity.lock()->to_string() << "\n";
}

void print(Reference<Entity> entity) {
    out(std::cout, entity);
}

void write(const std::string& filename, Reference<Entity> entity) {
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

int64_t len(Reference<Entity> entity) {
    return std::get<Array>(entity.lock()->value).size();
}

void add_elem(Reference<Entity> array, Reference<Entity> elem) {
    auto owner = array.lock();
    auto size = std::get<Array>(owner->value).size();
    std::get<Array>(owner->value)[size] = elem;
}

void remove(Reference<Entity> array, int64_t index) {
    auto& map = std::get<Array>(array.lock()->value);
    if (index < 0 || index >= static_cast<int64_t>(map.size())) {
        throw std::out_of_range("Array index out of bounds");
    }
    map.erase(index);
    
    Array new_map;
    size_t new_index = 0;
    for (auto& [_, value] : map) {
        new_map[new_index++] = value;
    }
    array.lock()->value = new_map;
}

Reference<Entity> get(Reference<Entity> array, int64_t index) {
    auto& map = std::get<Array>(array.lock()->value);
    if (index < 0 || index >= static_cast<int64_t>(map.size())) {
        throw std::out_of_range("Array index out of bounds");
    }
    return map.at(index);
}

void set(Reference<Entity> array, int64_t index, Reference<Entity> elem) {
    auto& map = std::get<Array>(array.lock()->value);
    if (index < 0 || index >= static_cast<int64_t>(map.size())) {
        throw std::out_of_range("Array index out of bounds");
    }
    map[index] = elem;
}