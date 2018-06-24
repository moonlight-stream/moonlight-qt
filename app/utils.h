#pragma once

#define THROW_BAD_ALLOC_IF_NULL(x) \
    if ((x) == nullptr) throw new std::bad_alloc()

