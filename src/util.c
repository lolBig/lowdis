#include "util.h"

long long get_time_in_ms() {
  struct timeval t;
  gettimeofday(&t, NULL);
  long long ms = t.tv_sec * 1000LL + t.tv_usec / 1000;
  return ms;
}
