#include <timely.h>
#include <random>

int main() {
  // The Timely TX rate estimator
  const double nicRate = 10000000000.0; // NIC line rate 10GBps (giga bytes/sec) as bytes/sec
  Experiment::Timely timely(nicRate, 0);
  std::cout << "Initial Timely State: " << timely;

  // Setup random generators
  std::random_device dev;
  std::mt19937 rng(dev());
  std::uniform_int_distribution<double> rttDist(timely.d_minModelRttUs+1e-07, timely.d_maxModelRttUs*2.0);
  std::uniform_int_distribution<double> elapsedDist(1, timely.d_maxModelRttUs*5.0);

  // Now simulate 10s (10,000,000 microseconds) of packet transmissions. At each iteration we randomly sample a RTT
  // in range (d_minModelRttUs, d_maxModelRttUs*2]. This RTT time represents a bonafide RTT; there's no serialization
  // in effect. Call this time 'rttUs'. Now, since the last packet transmission and now, 'rttUs' microseconds must have
  // elapsed. In fact, the elapsed time since the last simulated packet transmission was sent and response received
  // yielding 'rttUs' is at least 'rttUs', but could be higher if the simulated session did nothing for bit. So, we
  // randomly sample in [1, d_maxModelRttUs*5] adding this time to 'rttUs'. In sum,
  //
  // 'rtt' random RTT in units microseconds in range (d_minModelRttUs, d_maxModelRttUs*2]
  // 'elapsedTimeUs'  in units microseconds in range [rttUs+1,         rttUs+d_maxModelRttUs*5)
  //
  printf("Time(us), RTT(us), TimeDelta(us), Rate(byte/sec), RawRate(byte/sec)\n");
  double nowUs = 0;
  while (nowUs < 10000000.0) {
    double rttUs = rttDist(rng);
    double elapsedUs = elapsedDist(rng);
    elapsedUs += rttUs;
    assert(elapsedUs>rttUs);
    nowUs += elapsedUs;
    timely.update(rttUs, nowUs);
    printf("%lf, %lf, %lf, %lf, %lf\n", nowUs, rttUs, elapsedUs, timely.rate(), timely.rawRate());
  }

  std::cout << "Final Timely State: " << timely;

  return 0;
}
