#pragma once

// Purpose: Estimate TX rate in bytes/sec for next transmission based on last RTT using the Timely algorithm
// 
// Classes:
//   Experiment::Timely: Implements Timely 
//
// Thread Safety: not-thread-safe.
//
// Exception Policy: No exceptions

#include <stdio.h>
#include <iostream>
#include <assert.h>

namespace Experiment {

class Timely {
public:
  // CONSTANTS
  const double d_alpha = 0.875;                     // EWMA smoothing factor (needs research)
  const double d_beta = 0.8;                        // multiplicative decrease factor
  const double d_delta = 10000000.0;                // additive rate increase 10 million bytes/second

  const double d_minRttUs       = 20;               // RTT <= (20us) not considered; state unchanged
  const double d_minModelRttUs  = 50;               // minimum model RTT limit (50us); see comments above
  const double d_maxModelRttUs = 500;               // maximum model RTT limit (500 us); see comments above

  const double d_maxNicBps;                         // Maximum NIC bandwidth (bytes-per-sec)

  const double d_minRateBps = 500000.0;             // minimum transmit rate bytes/sec; computed rate must >= this limit
  const double d_maxRateBps = d_maxNicBps;          // maximum transmit rate bytes/sec; computed rate must <= this limit

  const double d_byteToGbits=8.0/(1000*1000*1000);  // factor to convert from bytes to Gbits (Giga bits)

private:
  double d_lineRateBps;                             // calculated TX rate (bytes-per-second)
  double d_rawLineRateBps;                          // calculated TX rate (bytes-per-second) before bounding
  double d_prevTimeUs;                              // absolute time in microseconds 'update' was last called
  double d_prevRttUs;                               // last rtt provided in 'update'
  double d_weightedRttDiffUs;                       // weighted RTT difference

public:
  // CREATORS
  Timely(double maxNicBps, unsigned sessionCount);
    // Create a Timely object to estimate TX rate in bytes/sec where 'maxNicBps' is the NIC's maximum bandwidth in
    // bytes/sec, and 'sessionCount' is the number of existing (not including the new session co-managed by this
    // object) already running, Behavior is defined 'maxNicBps>=1e6'. Upon creation, 'd_lineRateBps' is initialized
    // to be 'maxNicBps/(sessionCount+1)'.

  Timely() = delete;
    // Default constructor not provided

  Timely(const Timely& other) = delete;
    // Copy constructor not provided

  ~Timely() = default;
    // Destroy this object

  // ACCESSORS
  double rate() const;
    // Return the last estimated TX rate in bytes/sec. Note that, if called immediately following construction,
    // this returns the initial, estimated rate

  double rateAsGbps() const;
    // Exactly like 'rate' but expressed as Gbps (Giga bits per second)

  double rawRate() const;
    // Return the last estimated TX rate in bytes/sec before it was bounded by '[d_minRateBps, d_maxRateBps]'. This
    // is the raw Timely computed value without any repair

  double rawRateAsGbps() const;
    // Exactly like 'rawRate' but expressed as Gbps (Giga bits per second)

  // MANIPULATORS
  double update(double rttUs, double nowUs);
    // Return the new, estimated transmission rate in bytes/sec based on the specified 'rttUs' (units microseconds)
    // and the absolute wall-clock time 'nowUs' (units microseconds). Behavior is defined provided 'rttUs>0' and
    // 'nowUs>d_prevTimeUs'. 'rttUs' represents the most recent RTT (round trip time) completed and should not include
    // any serialization time. See comments top of file. Note that 'rttUs' less than or equal 'd_minRttUs' are ignored,
    // and state is not changed. Also note the calculated rate 'r' will always satisfy 'd_minRateBps<=r<=d_maxRateBps'.
    // Finally, this implementation uses the patched algoritm in [1]s section 4.3

  Timely& operator=(const Timely& rhs) = delete;
    // Assignment operator not provided

  // ASPECTS
  std::ostream& print(std::ostream& stream) const;
    // Print to specified 'stream' a human readable dump of this object's state returning 'stream'
};

// FREE OPERATORS
std::ostream& operator<<(std::ostream& stream, const Timely& object);
  // Print into specified 'stream' human readable dump of 'object' returning 'stream'

// INLINE DEFINITIONS
// CREATORS
inline
Timely::Timely(double maxNicBps, unsigned sessionCount)
: d_maxNicBps(maxNicBps)
, d_lineRateBps(d_maxNicBps/(sessionCount+1.0))
, d_rawLineRateBps(d_lineRateBps)
, d_prevTimeUs(0)
, d_prevRttUs(d_minRttUs)
, d_weightedRttDiffUs(0)
{
  assert(d_alpha>0.0 && d_alpha<=1.0);
  assert(d_beta>0.0  && d_beta<=1.0);
  assert(d_delta>=1000000.0);
  assert(d_minRateBps>0);
  assert(d_minRateBps<d_maxRateBps);
  assert(d_maxRateBps<=d_maxNicBps);
  assert(d_maxNicBps>=1000000.0);
  assert(d_minRttUs>=0);
  assert(d_minRttUs<d_minModelRttUs);
  assert(d_minModelRttUs<d_maxModelRttUs);
}

// ACCESSORS
inline
double Timely::rate() const {
  return d_lineRateBps;
}

inline
double Timely::rateAsGbps() const {
  return d_lineRateBps * d_byteToGbits;
}

inline
double Timely::rawRate() const {
  return d_rawLineRateBps;
}

inline
double Timely::rawRateAsGbps() const {
  return d_rawLineRateBps * d_byteToGbits;
}

// MANIPULATORS
inline
double Timely::update(double rttUs, double nowUs) {
  assert(rttUs>0);
  assert(nowUs>d_prevTimeUs);

  // When 'rttUs' is too small, skip Timely update                                                                      
  if (rttUs<=d_minRttUs) {
    return d_lineRateBps;
  }

  // Calculate difference in current and previous RTT
  const double newRttDiff = rttUs - d_prevRttUs;
  d_prevRttUs = rttUs;

  d_prevTimeUs = nowUs;

  // Update weighted diff
  d_weightedRttDiffUs = ((1-d_alpha)*d_weightedRttDiffUs) + (d_alpha*newRttDiff);

  if (rttUs < d_minModelRttUs) {
    d_lineRateBps += d_delta;
  } else if (rttUs > d_maxModelRttUs) {
    d_lineRateBps = d_lineRateBps * (1 - d_beta*(1.0-(d_maxModelRttUs/rttUs)));
  } else {
    const double rttGradient = d_weightedRttDiffUs / d_minRttUs;
    double weight(-1.0);
    if (rttGradient <= -0.25) {
      weight = 0.0;
    } else if (rttGradient >= 0.25) {
      weight = 1.0;
    } else {
      weight = 2*rttGradient + 0.5;
    }
    const double error = (rttUs-d_minRttUs) / d_minRttUs;
    d_lineRateBps = d_lineRateBps*(1.0 - (d_beta*weight*error)) + d_delta*(1-weight);
  }

  // Store Timely value as calculated
  d_rawLineRateBps = d_lineRateBps;

  // Bound estimated rate above and below
  d_lineRateBps = std::min(d_lineRateBps, d_maxRateBps);
  return d_lineRateBps = std::max(d_lineRateBps, d_minRateBps);
}

// ASPECTS
inline
std::ostream& Timely::print(std::ostream& stream) const {
  stream << "[" << std::endl;
  stream << "    rateGbps (last estimated rate)       : " << rateAsGbps()            << std::endl;
  stream << "    rateBps (last estimated rate)        : " << d_lineRateBps           << std::endl;
  stream << "    rawRateBps (last estimated raw rate) : " << d_rawLineRateBps        << std::endl;
  stream << "    prevTimeUs (last reported abs time)  : " << d_prevTimeUs            << std::endl;
  stream << "    prevRttUs (last reported RTT)        : " << d_prevRttUs             << std::endl;
  stream << "    alpha (EWMA smoothing factor)        : " << d_alpha                 << std::endl;
  stream << "    beta (multiplicative decrease factor): " << d_beta                  << std::endl;
  stream << "    delta (additive increase factor)     : " << d_delta                 << std::endl;
  stream << "    minRttUs (RTTs <= ignored)           : " << d_minRttUs              << std::endl;
  stream << "    minModelRttUs (min model RTT model)  : " << d_minModelRttUs         << std::endl;
  stream << "    maxModelRttUs (max model RTT model)  : " << d_maxModelRttUs         << std::endl;
  stream << "    NIC bandwidth (bytes/sec)            : " << d_maxNicBps             << std::endl;
  stream << "    minimum computed rate (bytes/sec)    : " << d_minRateBps            << std::endl;
  stream << "    maximum computed rate (bytes/sec)    : " << d_maxRateBps            << std::endl;
  stream << "]" << std::endl;
  return stream;
}
inline
std::ostream& operator<<(std::ostream& stream, const Timely& object) {
  return object.print(stream);
}

} // namespace Experiment
