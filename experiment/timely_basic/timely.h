#pragma once

// Purpose: Estimate TX rate in bytes/sec for next transmission based on last RTT using the Timely algorithm
// 
// Classes:
//   Experiment::Timely: Implements Timely 
//
// Thread Safety: not-thread-safe.
//
// Exception Policy: No exceptions
//
// This work is based on:
// 
// * [1] [ECN or Delay: Lessons Learnt from Analysis of DCQCN and TIMELY](http://yibozhu.com/doc/ecndelay-conext16.pdf)
// * [2] [ECN or Delay: Lessons Learnt from Analysis of DCQCN and TIMELY](https://github.com/jitupadhye-zz/rdma)
// * [3] [TIMELY: RTT-based Congestion Control for the Datacenter](https://conferences.sigcomm.org/sigcomm/2015/pdf/papers/p537.pdf)
//
// The intuition is that each transmitted packet gets an explicit response packet from which RTT (Round Trip Time) is
// is computed. RTT is used as an input to a model which computes the transmission rate the next packet(s) should have.
// RTT is seen as a proxy or data point for how much congestion there is between the sender and receiveer. Longer RTTs
// suggests a decrease in TX rate and vice-versa. RTT is defined in [3] section 3.1:
//
//      RTT = t_completion - t_send - Seg/NLR
//
//      where
//            t_completion = timestamp when transmitter got response packet
//            t_send       = timestamp when transmitter sent request packet
//            Seg          = #of bytes in transmitted packet
//            NLR          = NIC line rate (bytes/sec) at which bytes are written onto the wire
//
// In otherwords, RTT is just end-start minus the time it takes serialize bytes onto the wire. For example, it takes
// 52 µs to serialize 64KB on a 10 Gbps (10 Giga bits/second) NIC connection:
//
//      1/(10*1000*1000*1000) sec/bits * (64*1024*8) bits = .000052428sec ~= 52us
//
// If RTTs are of the same order of 10 as the serialization time, RTT should clearly not be penalized for serialization
// delays. This time does not reflect congestion outside the transmitting box.
//
// The Timely model ignores RTTs under d_minRttUs i.e. model is unchanged. Timely uses model min/maxes to determine
// how the new, estimated computation should proceed:
//
//      0             d_minModelRttUs          d_maxModelRttUs
//      +--------------------+----------------------+---------------->
//      | additive increase  | gradient based +/-   | multiplicative
//      | to new rate        | change to new rate   | decrease in new rate
//
// RTTs are computed with HW timestamp differences either with Intel's 'rdtsc()' in [6] or NIC HW provided timestamps
// taken at the appropiate TX/RX points [3].
//
// Zooming out to applications, consider several client boxes talking to 1 server box. For simplicity, assume each
// client has only one connected session with the server. There is a putative congestion point in the switch or 
// server that, depending on how bad it is, drives RTTs up. RTTs go down when congestion is cleared. All clients
// compete for bandwidth in the switch and server. Since Timely adjusts the TX rate for each session, each of the N
// clients will have their own Timely object to co-manage how fast they send:
//
//      +---------+
//      | client1 |------------------------------+
//      +---------+                              |
//                                               |
//      +---------+                          +--------------+            +--------+
//      | client2 |--------------------------| switch/router|------------| server |
//      +---------+                          +--------------+            +--------+
//          .                                    |
//          .                          +---------+
//          .                         /
//      +---------+                  /
//      | clientN |-----------------+
//      +---------+
//  
// The computed transmissions rates are used with what [6,7] calls a 'Timing Wheel'. Timing Wheels are not in scope
// here, however, Timely impact is umotivated unless it's clear what should be done with the rates. Let's paint a more
// realistic picture in commercial production data loads where each box has multi-connected-sessions. Each session
// on each box:
//
// * talks to sessions in the same box and rack through its rack switch
// * talks to sessions on different boxes in the same rack through its rack switch
// * talks to sessions in different racks via its switch + router
//
// The per box NIC is not shown. Assume all sessions go through 1 shared NIC or each have their own NIC. For the
// the purposes of Timing Wheels, this detail is not important. What is important? Each box shares a Timing Wheel:
//
//                        +----------+                        
//                        | Top of   |  to/from other routers for other racks
//                        | Rack     <---------------------------------------->
//                        | Router   |
//           +------------+----+-----+-----------+
//           |                 |                 |
//      +----------+      +----------+      +----------+
//      | Rack 1   |      | Rack 2   |      | Rack N   |                      
//      +----------+      +----------+      +----------+
//      | Rack 1   |      | Rack 2   |      | Rack N   |
//      | Switch   |      | Switch   |      | Switch   |
//      +----------+      +----------+      +----------+
//           |                 |                 |
//      +----------+           .                 .
//      | box 1    |           .                 .
//      +----------+           .                 .
//      | session1 |           .                 .
//      | Timely 1 |           .                 .
//      +----------+
//      |    .     |
//      |    .     |
//      +----------+
//      | sessionN |
//      | Timely N |
//      +----------+
//      | Timing   |
//      | Wheel    |
//      +----------+
//           |      
//      +----------+
//      | box 2    |
//      +----------+
//      | session1 |
//      | Timely 1 |
//      +----------+
//      |    .     |
//      |    .     |
//      +----------+
//      | Timing   |
//      | Wheel    |
//      +----------+
//           .
//           .
// Focus on box1 in rack1. There are N sessions. Each session has its own Timely object with its most recent computed
// rate R_i. Each session also has new packets it wants to TX. To do this, it hands off its packets, R_i, and other
// information to the Carousel Wheel which writes packets onto the wire at the appropriate time.

#include <stdio.h>
#include <iostream>

namespace Experiment {

class Timely {
public:
  // CONSTANTS
  const double d_alpha = 0.875;                     // EWMA smoothing factor (needs research)
  const double d_beta = 0.8;                        // multiplicative decrease factor
  const double d_delta = 10000000.0;                // additive rate increase 10 million bytes/second

  const double d_minRttUs       = 10;               // minimum actual RTT (20us) not considered
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

  if (rttUs<=d_minRttUs) {
    // Do nothing
    return d_lineRateBps;
  }

  // Calculate difference in current and previous RTT
  const double newRttDiff = rttUs - d_prevRttUs;
  d_prevRttUs = rttUs;

  // Calculate radhika's 'delta_factor'. This is a unitless number requiring
  // all terms use the same units
  double deltaFactor = static_cast<double>(nowUs-d_prevTimeUs)/d_minRttUs;
  deltaFactor = std::min(deltaFactor, 1.0);
  d_prevTimeUs = nowUs;

  // Update weighted diff aka radhika's 'avg_rtt_diff_'
  d_weightedRttDiffUs = ((1-d_alpha)*d_weightedRttDiffUs) + (d_alpha*newRttDiff);

  if (rttUs < d_minModelRttUs) {
    d_lineRateBps += (deltaFactor*d_delta);
  } else if (rttUs > d_maxModelRttUs) {
    const double multDecreaseFactor = deltaFactor * d_beta;
    d_lineRateBps = d_lineRateBps * (1 - multDecreaseFactor*(1.0-(d_maxModelRttUs/rttUs)));
  } else {
    const double multDecreaseFactor = deltaFactor * d_beta;
    const double rttGradient = d_weightedRttDiffUs / d_minRttUs;
    double weight(-1.0);
    if (rttGradient <= -0.25) {
      weight = 0.0;
    } else if (rttGradient >= 0.25) {
      weight = 1.0;
    } else {
      weight = 2*rttGradient + 0.5;
    }
    double error = (rttUs-d_minRttUs) / d_minRttUs;
    d_lineRateBps = d_lineRateBps*(1.0 - (multDecreaseFactor*weight*error)) + deltaFactor*d_delta*(1-weight);
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
  stream << "    maxModelRttUs (mad model RTT model)  : " << d_maxModelRttUs         << std::endl;
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
