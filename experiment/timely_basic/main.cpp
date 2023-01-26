#include <timely.h>

int main() {
  const double nicRate = 10000000000.0; // NIC line rate 10GBps (giga bytes/sec) as bytes/sec

  // The Timely TX rate estimator
  Experiment::Timely timely(nicRate, 0);
  std::cout << timely;

  return 0;
}
