#pragma once
#include "../model/model.h"
#include <iostream>
#include <fstream>
#include <ostream>
#include <sstream>
#include <variant>
#include <vector>

void out(std::ostream& os, Reference<Entity> entity);
void print(Reference<Entity> entity);
void write(const std::string& filename, Reference<Entity> entity);
std::vector<std::string> read(const std::string& filename);
int64_t len(Reference<Entity> entity);
void add_elem(Reference<Entity> array, Reference<Entity> elem);
void remove(Reference<Entity> array, int64_t index);
Reference<Entity> get(Reference<Entity> array, int64_t index);
void set(Reference<Entity> array, int64_t index, Reference<Entity> elem);
