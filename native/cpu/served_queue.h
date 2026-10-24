#pragma once

#include <cstddef>

namespace raven {

// One producer and one consumer per guest thread; indices count records and publish their contents with release ordering.
struct ServedQueue {
	static constexpr size_t head = 16, tail = 17, enabled = 18, gl_elements = 19;
	static constexpr size_t records = 32, words = 16, capacity = 16;
};

}
