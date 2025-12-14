#include "model.h"

namespace umka::vm {
Entity make_array() { return Entity { .value = std::make_shared<Array>() }; }
}
