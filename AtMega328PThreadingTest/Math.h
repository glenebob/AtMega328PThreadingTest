#pragma once

static inline size_t min_size(size_t x, size_t y)
{
    return (x < y) ? x : y;
}

static inline size_t max_size(size_t x, size_t y)
{
    return (x < y) ? y : x;
}
