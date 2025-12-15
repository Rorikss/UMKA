#pragma once
#include "../model/model.h"
#include <iostream>
#include <fstream>
#include <ostream>
#include <sstream>
#include <variant>
#include <vector>

namespace umka::vm {
void out(std::ostream& os, Reference<Entity> entity);
void print(Entity entity);
void write(const std::string& filename, Entity entity);
std::vector<std::string> read(const std::string& filename);
int64_t len(Entity entity);
void add_elem(Entity array, Reference<Entity> elem);
void remove(Entity array, int64_t index);
Reference<Entity> get(Entity array, int64_t index);
void set(Entity array, int64_t index, Reference<Entity> elem);
void umka_assert(Entity& condition);
std::string input();
double random();
double pow(double base, double exp);
double sqrt(double number);
void umka_sort(Entity array);
std::vector<std::string> split(const Entity& str_entity, const Entity& delim_entity);
void make_heap(Entity array);
void pop_heap(Entity array);
void push_heap(Entity array, Reference<Entity> elem);
}
