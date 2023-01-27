#include <timely.h>
#include <random>

const double nicRate = 10000000000.0; // NIC line rate 10GBps (giga bytes/sec) as bytes/sec

void test1() {
  // The Timely TX rate estimator
  Experiment::Timely timely(nicRate, 0);

  // Setup random generators
  std::random_device dev;
  std::mt19937 rng(dev());

  // Sample from Guassian distribution
  const double mean = (timely.d_maxModelRttUs-timely.d_minModelRttUs)/2.0;
  const double stddev = 4.0;
  std::normal_distribution<double> rttDist(mean, stddev);

  FILE *fid = fopen("./test1.dat", "wt");
  assert(fid!=0);
  fprintf(fid, "# NIC Rate (bytes/sec): %lf, RTTs sampled from Guassian distribution mean=%lf, stddev=%lf\n",
    nicRate, mean, stddev);

  // Now simulate 10s (10,000,000 microseconds) of packet transmissions. At each iteration we randomly sample a RTT
  // from 'rttDist'. This RTT time represents a bonafide RTT without serialization. Call this time 'rttUs'. Assuming
  // there's no overhead to transmit the packet, the elapsed time form the last simulated transmission to now is just
  // 'rttUs'. 

  fprintf(fid, "Time,RTT,Rate,RawRate\n");
  double nowUs = 0;
  while (nowUs < 10000000.0) {
    double rttUs = rttDist(rng);
    nowUs += rttUs;
    timely.update(rttUs, nowUs);
    fprintf(fid, "%lf,%lf,%lf,%lf\n", nowUs, rttUs, timely.rate(), timely.rawRate());
  }

  fclose(fid);
  std::cerr << "test1: Timely Final State: " << timely << std::endl << std::endl;
}

void test2() {
  // The Timely TX rate estimator
  Experiment::Timely timely(nicRate, 0);

  const double inc  = 5;
  const double smallInc  = 0.5;
  const double stopRatio = 0.8;
  const double mean = (timely.d_maxModelRttUs-timely.d_minModelRttUs)/2.0;

  FILE *fid = fopen("./test2.dat", "wt");
  assert(fid!=0);
  fprintf(fid, "# NIC Rate (bytes/sec): %lf. RTTs start at mean %lf rising to max %lf\n", nicRate, mean, timely.d_maxModelRttUs);
  fprintf(fid, "# in %lf us increments until the min TX rate %lf is reached. Then RTTs decrease\n", inc, timely.d_minRateBps);
  fprintf(fid, "# in %lf increments until %lf of NIC bandwidth reached\n", smallInc, stopRatio); 

  fprintf(fid, "Time,RTT,Rate,RawRate\n");
  double nowUs = 0;
  double rttUs = mean;
  do {
    nowUs += rttUs;
    timely.update(rttUs, nowUs);
    fprintf(fid, "%lf,%lf,%lf,%lf\n", nowUs, rttUs, timely.rate(), timely.rawRate());
    rttUs += inc;
  } while (rttUs<=timely.d_maxModelRttUs);

  do {
    rttUs -= smallInc;
    nowUs += rttUs;
    timely.update(rttUs, nowUs);
    fprintf(fid, "%lf,%lf,%lf,%lf\n", nowUs, rttUs, timely.rate(), timely.rawRate());
  } while (rttUs>timely.d_minRttUs && timely.rate()<=(nicRate*stopRatio));

  fclose(fid);
  std::cerr << "test2: Timely Final State: " << timely << std::endl << std::endl;
}

void test3() {
  // The Timely TX rate estimator
  Experiment::Timely timely(nicRate, 0);

  // Setup random generators
  std::random_device dev;
  std::mt19937 rng(dev());

  // Sample from Guassian distribution
  const double mean = (timely.d_minModelRttUs+10);
  const double stddev = 4.0;
  std::normal_distribution<double> rttDist(mean, stddev);

  FILE *fid = fopen("./test3.dat", "wt");
  assert(fid!=0);
  fprintf(fid, "# NIC Rate (bytes/sec): %lf, RTTs sampled from Guassian distribution mean=%lf, stddev=%lf\n",
    nicRate, mean, stddev);

  // Now simulate 10s (10,000,000 microseconds) of packet transmissions. At each iteration we randomly sample a RTT
  // from 'rttDist'. This RTT time represents a bonafide RTT without serialization. Call this time 'rttUs'. Assuming
  // there's no overhead to transmit the packet, the elapsed time form the last simulated transmission to now is just
  // 'rttUs'. 

  fprintf(fid, "Time,RTT,Rate,RawRate\n");
  double nowUs = 0;
  while (nowUs < 10000000.0) {
    double rttUs = rttDist(rng);
    nowUs += rttUs;
    timely.update(rttUs, nowUs);
    fprintf(fid, "%lf,%lf,%lf,%lf\n", nowUs, rttUs, timely.rate(), timely.rawRate());
  }

  fclose(fid);
  std::cerr << "test1: Timely Final State: " << timely << std::endl << std::endl;
}

int main() {
  test1();
  test2();
  test3();
  return 0;
}
