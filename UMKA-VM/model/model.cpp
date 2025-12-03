#include "model.h"

Entity make_array() { return Entity { .value = std::make_shared<Array>() }; }
