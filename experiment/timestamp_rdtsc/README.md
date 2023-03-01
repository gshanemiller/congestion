# Timestamps from Intel's rdpmc

eRPC uses `rdtsc` Intel processor instruction to measure RTTs and to timestamp
packets for Carousel. `rdtsc` does not measure CPU cycles; the clock runs at a 
fixed frequency different from the CPU. On the other hand, RTT and Carousel 
typically work in units of microseconds. Therefore the practical usage of `rdtsc`
requires a fast way to convert from clock ticks to microseconds.

* `rdtsc` reads a 64-bit counter set 0 when the CPU chip is reset [1] 17.17
* The counter increments even when the processor is halted by the HLT instruction or the external STPCLK# pin. Note that the assertion of the external DPSLP# pin may cause the time-stamp counter to stop. [1] 17.17
* Constant TSC behavior ensures that the duration of each clock tick is uniform and supports the use of the TSC as a wall clock timer even if the processor core changes frequency. This is the architectural behavior moving forward. [1] 17.17
* The `rdtsc` instruction reads the time-stamp counter and is guaranteed to return a monotonically increasing unique value whenever executed, except for a 64-bit counter wraparound. Intel guarantees that the time-stamp counter will not wraparound within 10 years after being reset. The period for counter wrap is longer for Pentium 4, Intel Xeon, P6 family, and Pentium processors. [1] 17.17
* The `rdtsc` instruction is not serializing or ordered with other instructions. It does not necessarily wait until all previous instructions have been executed before reading the counter. [1] 17.17
* As a result of the state transitions due to opportunistic processor performance operation (see Chapter 14, “Power and Thermal Management”), a logical processor or a processor core can operate at frequency different from the Processor Base frequency. The following items are expected to hold true irrespective of when opportunistic processor operation causes state transitions (*) The time stamp counter operates at a fixed-rate frequency of the processor. [1] 19.7.2
* If hyper threading is used, HW threads on the same core will compete for resources. It may be slightly slower [2]
* `rdtsc` is per core. There's no guarantee two different cores or cores on different sockets will agree
* `rdtsc` is an Intel processor instruction. It's unclear at the time of this writing what'd be appropriate for AMD or other processors
* It's possible task switches, unpinned threads, and interrupts will add noise [3]

[1] Intel® 64 and IA-32 Architectures Software Developer’s Manual Volume 3 (3A, 3B, 3C & 3D)
[2] https://stackoverflow.com/questions/56439714/can-different-processes-run-rdtsc-at-the-same-time
[3] https://agner.org/optimize/#testp and https://www.agner.org/optimize/optimizing_assembly.pdf 18.1

# Converting From RDTSC tickets to Time

eRPC runs the following code to find a conversion from `rdtsc` tickes to time.
ChronoTimer is a wrapper over std::chrono::high_resolution_clock. The intuition
is we cannot reliably know the CPU's frequencyn nor the frequency of the clock
`rdtsc` measures. All we can do is compare the time difference in a conventional
clock (high_resolution_clock) to the difference in `rdtsc` ticks. This code must
be run with a pinned thread.

```
/// Convert cycles measured by rdtsc with frequence \p freq_ghz to usec
static double to_usec(size_t cycles, double freq_ghz) {
  return (cycles / (freq_ghz * 1000));
}

/// Convert cycles measured by rdtsc with frequence \p freq_ghz to nsec
static double to_nsec(size_t cycles, double freq_ghz) {
  return (cycles / freq_ghz);
}

static double measure_rdtsc_freq() {
  ChronoTimer chrono_timer;
  const uint64_t rdtsc_start = rdtsc();

  // Do not change this loop! The hardcoded value below depends on this loop
  // and prevents it from being optimized out.
  uint64_t sum = 5;
  for (uint64_t i = 0; i < 1000000; i++) {
    sum += i + (sum + i) * (i % sum);
  }
  rt_assert(sum == 13580802877818827968ull, "Error in RDTSC freq measurement");

  const uint64_t rdtsc_cycles = rdtsc() - rdtsc_start;
  const double freq_ghz = rdtsc_cycles * 1.0 / chrono_timer.get_ns();
  rt_assert(freq_ghz >= 0.5 && freq_ghz <= 5.0, "Invalid RDTSC frequency");

  return freq_ghz;
}
```
