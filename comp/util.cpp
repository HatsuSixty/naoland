#include "util.hpp"

#include <ctime>
#include <cstdint>

int64_t timespec_to_msec(const struct timespec *a)
{
    return (int64_t)a->tv_sec * 1000 + a->tv_nsec / 1000000;
}

int64_t get_time_milli()
{
    timespec now;
    timespec_get(&now, TIME_UTC);
    return timespec_to_msec(&now);
}
