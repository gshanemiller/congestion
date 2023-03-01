#include <chrono>
#include <time.h>
#include <assert.h>
#include <sched.h>
#include <errno.h>

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
void rdtsc1() {
  alignas(64) const auto clock_start = std::chrono::high_resolution_clock::now();
  alignas(64) const auto rdtsc_start = __builtin_ia32_rdtsc();

  // Do not change this loop! The hardcoded value below depends on this loop
  // and prevents it from being optimized out.
  alignas(64) uint64_t sum = 5;
  for (uint64_t i = 0; i < 1000000; i++) {
    sum += i + (sum + i) * (i % sum);
  }
  assert(sum == 13580802877818827968ull);

  alignas(64) const uint64_t rdtsc_end = __builtin_ia32_rdtsc();
  alignas(64) const auto clock_end = std::chrono::high_resolution_clock::now();

  const uint64_t rdtsc_diff = rdtsc_end - rdtsc_start;
  const auto clock_diff = std::chrono::duration_cast<std::chrono::nanoseconds>(clock_end-clock_start).count();

  const double freq_ghz = static_cast<rdtsc_cycles> / static_cast<double>(clock_diff);

  printf("measure rdtsc with std::chrono: clockDiff %llu ns, rdtsc cycles: %llu, rdtscFreq %lf Ghz\n",
    clock_diff, rdtsc_diff, freq_ghz);
}

int main() {
  rdtsc1();
  return 0;
}
