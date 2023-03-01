#include <chrono>
#include <time.h>
#include <assert.h>
#include <sched.h>
#include <errno.h>
#include <stdio.h>
#include <x86intrin.h>

#include <array>

const int kMAX = 100;

std::array<double, kMAX> frequency1;
std::array<double, kMAX> frequency2;

int pinToCore(int coreId) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(coreId, &mask);

  // Pin caller's thread to specified 'coreId'
  if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1) {
      return errno;
  }

  return 0;
}

// Measure rdtsc frequency with std::chrono::high_resolution_clock::now()
double rdtsc1() {
  alignas(64) const auto clock_start = std::chrono::high_resolution_clock::now();
  alignas(64) const auto rdtsc_start = __rdtsc();

  // Do not change this loop! The hardcoded value below depends on this loop
  // and prevents it from being optimized out.
  alignas(64) uint64_t sum = 5;
  for (alignas(64) uint64_t i = 0; i < 1000000; i++) {
    sum += i + (sum + i) * (i % sum);
  }

  alignas(64) const uint64_t rdtsc_end = __rdtsc();
  alignas(64) const auto clock_end = std::chrono::high_resolution_clock::now();

  if (sum != 13580802877818827968ull) {
    printf("timing loop failed\n");
    exit(1);
  }

  const uint64_t rdtsc_diff = rdtsc_end - rdtsc_start;
  const auto clock_diff = std::chrono::duration_cast<std::chrono::nanoseconds>(clock_end-clock_start).count();

  const double freq_ghz = static_cast<double>(rdtsc_diff)/static_cast<double>(clock_diff);

  // printf("measure rdtsc with std::chrono: clockDiff %lu ns, rdtsc cycles: %lu, rdtscFreq %lf Ghz\n",
  //  clock_diff, rdtsc_diff, freq_ghz);

  return freq_ghz;
}

// Measure rdtsc frequency with clock_gettime
double rdtsc2() {
  alignas(64) struct timespec clock_end;
  alignas(64) struct timespec clock_start;

  clock_gettime(CLOCK_REALTIME, &clock_start);
  alignas(64) const auto rdtsc_start = __rdtsc();

  // Do not change this loop! The hardcoded value below depends on this loop
  // and prevents it from being optimized out.
  alignas(64) uint64_t sum = 5;
  for (alignas(64) uint64_t i = 0; i < 1000000; i++) {
    sum += i + (sum + i) * (i % sum);
  }

  alignas(64) const uint64_t rdtsc_end = __rdtsc();
  clock_gettime(CLOCK_REALTIME, &clock_end);

  if (sum != 13580802877818827968ull) {
    printf("timing loop failed\n");
    exit(1);
  }

  const uint64_t rdtsc_diff = rdtsc_end - rdtsc_start;
  const auto clock_diff = (clock_end.tv_sec*1000000000ULL+clock_end.tv_nsec) -
                          (clock_start.tv_sec*1000000000ULL+clock_start.tv_nsec);

  const double freq_ghz = static_cast<double>(rdtsc_diff)/static_cast<double>(clock_diff);

  // printf("measure rdtsc with clock_gettime: clockDiff %llu ns, rdtsc cycles: %lu, rdtscFreq %lf Ghz\n",
  //  clock_diff, rdtsc_diff, freq_ghz);

  return freq_ghz;;
}

int main() {
  for (unsigned i=0; i<kMAX; ++i) {
    frequency1[i] = rdtsc1();
  }

  for (unsigned i=0; i<kMAX; ++i) {
    frequency2[i] = rdtsc2();
  }

  for (unsigned i=0; i<kMAX; ++i) {
    printf("rdtsc1: sample %u: %.8lf\n", i, frequency1[i]);
  }

  for (unsigned i=0; i<kMAX; ++i) {
    printf("rdtsc2: sample %u: %.8lf\n", i, frequency2[i]);
  }

  return 0;
}
