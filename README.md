# Purpose
The purpose of this repository is to provide a *Practitioner's Guide* to implementing a userspace only network packet congestion control scheme to be used with a low-latency UDP messaging library on intra-corporate networks. This work shall be complete when,

* Concepts for congestion control, packet drop/loss are well explained
* Implementation choices are well described
* Code is provided to concretely demonstrate concepts and implementation
* A guide is provided on how congestion control might be incorporated into a DPDK messaging library

# Papers
The following papers are a good starting place. Many papers are co-authored by Google, Intel, and Microsoft and/or are used in production. Packets moving between data centers through WAN is not in scope. This work is for packets moving inside a corporate network. No code uses the kernel; this is all user-space work typically done with DPDK/RDMA.

* [1] [ECN or Delay: Lessons Learnt from Analysis of DCQCN and TIMELY](http://yibozhu.com/doc/ecndelay-conext16.pdf)
* [2] [ECN Github](https://github.com/jitupadhye-zz/rdma)
* [3] [TIMELY: RTT-based Congestion Control for the Datacenter](https://conferences.sigcomm.org/sigcomm/2015/pdf/papers/p537.pdf)
* [4] [TIMELY Power Point Slides](http://radhikam.web.illinois.edu/TIMELY-sigcomm-talk.pdf)
* [5] [TIMELY Source Code Snippet](http://people.eecs.berkeley.edu/~radhika/timely-code-snippet.cc) referenced and used in eRPC
* [6] [Datacenter RPCs can be General and Fast](https://www.usenix.org/system/files/nsdi19-kalia.pdf) aka **eRPC**
* [7] [eRPC Source Code](https://github.com/erpc-io/eRPC)
* [8] [Carousel: Scalable Traffic Shaping at End Hosts](https://saeed.github.io/files/carousel-sigcomm17.pdf)
* [9] [Receiver-Driven RDMA Congestion Control by Differentiating Congestion Types in Datacenter Networks](https://icnp21.cs.ucr.edu/papers/icnp21camera-paper45.pdf)

Points of orientation:

* [1] contains a correction to [3] and also describes DCQCN with ECN but not Carousel. ECN requires switches/routers to be ECN enabled. Equinix does not or cannot do ECN, for example.
* [1] concludes DCQCN is better than Timely
* [9] describes another approach handily beating Timely, however, the "deployment of RCC relies on high precision clock synchronization throughout the datacenter network. Some recent research efforts can reduce the upper bound of clock synchronization within a datacenter to a few hundred nanoseconds, which is sufficient for our work."
* Timely congestion control therefore is the least proscriptive, and probably worst performing. I would note most DPDK/RDMA work like [RAMCloud](https://dl.acm.org/doi/pdf/10.1145/2806887) rely on lossless packet hardware atypical in corporate data centers. This is another reason why Timely is in scope
* eRPC [6,7] uses Carousel [8] and Timely [3,4,5] to good effect

# Build Code
In an empty directory do the following assuming you have a C++ tool chain and cmake installed:

1. git clone https://github.com/gshanemiller/congestion.git
2. mkdir build && cd build
3. cmake ..

All tasks/libraries can be found in the './build' directory

# R Plots
Some test programs produce data plotted with R (freeware stats program) using a provided R script. See individual READMEs for details. You might encounter `ggplot2` unknown or not found. To fix missing R dependencies, run the following commands in the R CLI:

```
# R packages used here
install.packages("ggplot2")
install.packages("gridExtra")
```

You only need to do this once. R is smart enough to find external source repositories and install. R does not need to be restarted.

# Milestones
Congestion control involves four major problems:

* Detect and correct packet drop/loss. Detection usually involves timestamps and sequence numbers. Resending packets is more involved
* Detect and respond to congestion by not sending too much data too soon e.g. [1-5, 9]
* And with (2) determine when to send new data without exasperbating congestion e.g. [8]
* Do all of the above without wasting CPU

I suggest the following milestone trajectory. It's valid *provided* Timely is the goto congestion control method. I report other methods above, however, they arguably impose impactical constraints.

```
Milestones
0                        
|----------------------------------------------------------------------------->
Provide theoretical motivation for
Timely based on [1,3]


Milestones (cont)
1                        2                          3
|------------------------+--------------------------+------------------------->
Simulate Timely in C++   Figure out timestamps.     When to use Timely?
The goal is to get a     eRPC uses rdtsc. Would     eRPC has a complicated
good impl of the model   NIC timestamps be better?  way to selectively
                         For Mellanox NICs, how     ignore Timely or use it
                         where are they?


Milestones (cont)
4                        5                          6
|------------------------+--------------------------+------------------------->
Combine 1-3 into a impl  Extend (4) adding kernel   Extend (5) adding
closer to production     UDP sockets and eRPC       sequence numbers to detect
code                     packet level pacing. ACKs  packet drop/reorder. Figure
                         here form RTT. Run sender/ out a way to resend lost
                         receiver pair to test      data in order


Milestones (cont)
7                        8                          9
|------------------------+--------------------------+------------------------->
Describe Carousel arch.  Document arch to add       Implement design in (8)
Delineate where work in  Carousel to (6)            and test/validate it.
(6) stops and Carousel
starts.


Milestones (cont)
10                       11
|------------------------+--------------------------+------------------------->
Document how to bring    Implement (10) using code
the work in (9) into a   in Reinvent library
DPDK setting
```

# Milestone#0: Theoretical Motivation for Timely
[3] gives the intuition for Timely's use of RTTs (Round Trip Times):

```
Delay-based congestion control algorithms such as FAST TCP [46] and Compound TCP [44] are inspired by the seminal work
of TCP Vegas [16]. These interpret RTT increase above a baseline as indicative of congestion: they reduce the sending
rate if delay is further increased[.]
```

Increasing RTTs imply there's a congestion point between the data sender and data receiver e.g. top of rack router. Timely's contribution to end-point RTT measurement and application for congestion is rate change:

```
However, Kelly et al. [33] argue that it is not possible to control the queue size when it is shorter in time than the
control loop delay ... The most any algorithm can do in these circumstances is to control the distribution of the queue
occupancy ... TIMELY’s congestion controller achieves low latencies by reacting to the delay gradient or derivative of
the queuing with respect to time, instead of trying to maintain a standing queue ...  Delay gradient is a proxy for
the rate mismatch at the bottleneck queue.
```

It should be noted that Timely work never directly engages the number of congestion points. Senders move packets through one switch minimum if the destination is in the same subnet. If the destination is in another subnet, packets typically move through two switches and a router. There's additional contributions in the destination NIC. Therefore Timely is best contextualized as either point to point communication with a single congestion point in the middle, or the congestion point is an aggregation of all the intervening queues between senders and receivers.

eRPC [7] uses Intel's `rdtsc()` to take time stamps eventually converting those measurements to microseconds when Timely's update function is run. [1,3] uses NIC provided timestamps. What you use and how you use it depends on eliminating the serialization time. HW timestamps are strongly preferred because Timely only works if RTTs are accurate. RTT is defined in [3] section 3.1:

```
RTT = t_completion - t_send - Seg/NLR
    
where
    t_completion = timestamp when transmitter got response packet
    t_send       = timestamp when transmitter sent request packet
    Seg          = #of bytes in transmitted packet
    NLR          = NIC line rate (bytes/sec) at which bytes are written onto the wire
```

In otherwords, RTT is just end-start minus the time it takes serialize bytes onto the wire. For example, it takes 52 µs to serialize 64KB on a 10 Gbps (10 Giga bits/second) NIC connection:

```
    1/(10*1000*1000*1000) sec/bits * (64*1024*8) bits = .000052428sec ~= 52us or .8ns/byte
```

RTTs at the same order as serialization penalize Timely because this time does not reflect congestion outside the transmitting box.

The Timely event loop is:

1. Fix initial sending rate R
2. Send data at rate R
3. Measure the RTT 'r' for data just sent
4. Compute new rate R = timely(R, r)
5. Goto 2

[3] section 4.2 describes the model's equations. It envisions N end hosts, or what eRPC calls connected sessions, all sending data simulteanously with a combined rate `y(t) bytes/sec`. The N transmitters share a congestion point e.g. a bottle neck queue. Suppose this CP (Congestion Point) empties its queue at rate `C bytes/sec`, and further suppose CP's queuing delay in time is `q(t)` (units time). If at any point in time `t` we have `y(t)>C` the congestion point gets worse by the amount `y(t)-C` and vice-versa. Putting these pieces together the change in the queuing delay is:

```
dq(t) sec    y(t)-C   bytes/sec
----  ---  = ------   ---------
 dt   sec      C      bytes/sec
```

Ideally Timely will converge on what ECN [1] calls a fixed point where the change is zero. This means senders and CP balance. Some observations are in order. Consider a time instant `t`:

* On the right hand side `y(t)-C` represents the excess bytes above (or below) what C can do. If `y(t)<C` CP can drain the queue faster than incoming data. This makes the rate negative, and vice versa
* The right hand side divides by `C` making a unitless number representing the ratio of excesss (or deficiency) bytes CP can handle
* And thus `dq(t)/t` is just the previous point by another name through equality

Timely introduces RTTs by asserting:

```
dRTT    dq(t)
---- =  -----
 dt      dt
```

Crucially, this assertion eliminates knowing `C`.

Recall Timely claims "it is not possible to control the queue size." `q(t)` is the time ("queuing delay") to deplete the queue not queue size according to Timely [3]. But [1] table 2 says `q(t)` is "queue size." Unfortuantely, Timely articles and code do not thoroughly explicate units, which is a silly unforced error. I'll return to this issue below.

The Timely model is equipped with a RTT bound called `[T_low, T_High]` which selects how the TX rate is updated:

```
0               minModelRtt              maxModelRtt
+--------------------+----------------------+---------------->
| additive increase  | gradient based +/-   | multiplicative
| to new rate        | change to new rate   | decrease in new rate
```

When RTTs are in `[minModelRtt, maxModelRtt]` individual sender rates are computed through a derivative related to `dq(t)/t`. If `RTT>maxModelRtt` the rate is decreased through a multiplicative factor. Finally, If the `RTT<minModelRtt` the new rate is just the old rate plus an additive factor. 

This establishes Timely basics. Putting aside the simple additve case, and before addressing the computations for the other two cases, we need to explain Timely's alpha and normalized gradient terms. 

## EWMA (Alpha)
EWMA (Exponentially Weight Moving Average) is a variation on a simple moving average where recent terms have more impact. Since Timely is based on a *time series of RTT changes*, it first smooths change jitter. This is called `rttDiff` in [1,3] and what I call `emwaDiff` to differentiate between it from `currentRtt-oldRtt`.

```
emwaDiff_0 = k_0
emwaDiff_n = alpha*rttDiff_n + (1-alpha)*emwaDiff_(n-1)

where:
       n         (subscript) is the iteration in recurrence 0,1,2,...
       k_0       is the initial value for emwaDiff typically 0
       alpha     is in [0,1]. lower values smooth more and 1 is no smoothing
       rttDiff_n is the difference in RTTs at the nth step e.g. rtt_n-rtt_(n-1)
```

[Wikipedia gives the same forumua](https://en.wikipedia.org/wiki/Exponential_smoothing) used in Timely. As the recurrence proceeds n=0,1,2,3... later terms contribute more. Example table for alpha=.48 appears below. Note `emwaDiff` changes less drastically compared to the actual RTT difference:

```
alpha = 0.48
+----------+---------------+----------+-----------+
| RTT (us) | RTT diff (us) | emwaDiff | Iteration |
+----------+---------------+----------+-----------+
| 351      | 0             | 0        | 0         |
+----------+---------------+----------+-----------+
| 50       | -301          | -144.48  | 1         |
+----------+---------------+----------+-----------+
| 352      | +302          | +69.83   | 2         |
+----------+---------------+----------+-----------+
| 83       | -269          | -92.81   | 3         |
+----------+---------------+----------+-----------+
| 86       | +3            | -46.82   | 4         |
+----------+---------------+----------+-----------+
```

## Normalized Gradient
The normalized gradient is just `emwaDiff` divided by `D_(minRTT)`. [3] section 4.3 writes:

```
To compute the delay gradient, TIMELY computes the difference between two consecutive RTT samples. We normalize this
difference by dividing it by the minimum RTT, obtaining a dimensionless quantity. In practice, the exact value of the
minimum RTT does not matter since we only need to determine if the queue is growing or receding. We therefore use a
fixed value representing the wire propagation delay across the datacenter network, which is known ahead of time.
```

Algorithms divide `emwaDiff` not `rttDiff` by `D_(minRTT)`. Based on what I can see in code it's one of:

* the smallest measureable RTT say 5us
* the smallest elapsed time allowed between Timely updates

## Case: `RTT>maxModelRtt`
This case applies when the measured RTT exceeds the Timely model maximum called `T_high`. When RTTs are too high, Timely reduces the rate multiplying down through a factor based on `beta`. [3 p543] section 4.3 notes:

```
We use the instantaneous rather than smoothed RTT. While this may seem unusual, we can slow down in response to a
single overly large RTT because we can be confident that it signals congestion, and our priority is to maintain low
packet latencies and avoid loss."
```

The reference to "smoothed RTT" is a typo. As explained above, RTT differences are smoothed not RTTs. The formula reads:

```
R = R(1 - beta(1 - T_high ) )
                   ------
                    RTT

where:
       R      is the current rate (bytes/sec) assigned to R on the left making new rate
       beta   is a dimensionless fixed constant in [0,1] and (0,1) in practice
       T_high is the maximum model RTT (us) used to select this case
       RTT    is the current RTT (us)
```

When RTT exceeds `T_high` the term `1-T_high/RTT` gives a value in `(0,1)`. This term is decreased by `beta` through multiplication. In this way, the worse the RTT is, the smaller the new `R` becomes through the two `1-` expressions. And vice-versa: if RTTs are barely above `T_high`, the new `R` is decreased less.

## Case: `RTT in [T_low, T_High]`
This case applies when the current RTT is in `[T_low, T_High]`. It is the most complicated computation. And this is where [1] and [3] fundamentally differ. In this write-up I'll follow [1] because [1] claims its algoritm achieves `dq/dt=0` while [3] does not. [7] codes for this correction too.

## Timely Algorithm Summary
The Timely event loop is:

1. Fix initial sending rate R
2. Send data at rate R
3. Measure the RTT 'r' for data just sent
4. Compute new rate R = timely(R, r)

[7] sets initial rate R to the NIC's bandwidth; [1] section 4 sets it to `C/(n+1)` where 'n' is the number of connected sessions sharing the same NIC. `C` is the NIC's bandwidth. [1] also describes `C` in section 3.1 as "bandwidth of bottleneck link"; I assume section 4 is a typo. Use HW generated RTTs being careful to exclude serialization time. 

HW rate limiters, if any, should be disabled. Quoting [1] section 4.2:

```
This analysis begs the question – why does TIMELY work well in practice, as reported in [21]? The answer appears to lie
in the fact that the TIMELY implementation does not use hardware rate limiters. Instead, the TIMELY implementation
controls rate by adjusting delay between successive transmissions of chunks that can be as large as 64KB. Each chunk is
sent out at or near line rate.
```

Each Timely update falls into one of three cases by comparing the current, actual RTT with two model bounds:

```
                  T_low or                T_high or
0               minModelRtt              maxModelRtt
+--------------------+----------------------+---------------->
| additive increase  | gradient based +/-   | multiplicative
| to new rate        | change to new rate   | decrease in new rate
```

Timely uses the following constants for its computations. In code they are represented as `double`s. I provide illustrative defaults. The min/max rates at bottom are optional. They can be use to bound the rate used by application code once the raw Timely value is computed. For the most part Timely code and papers use bytes/sec for rates and microseconds (us) for RTTs:

```
const double d_alpha = 0.46;              // EWMA smoothing factor (dimensionless in [0,1])
const double d_beta = 0.26;               // multiplicative decrease factor (dimensionless in [0,1])
const double d_delta = 5*1000*1000.0;     // additive rate increase 5 million bytes/second

const double d_minRttUs       = 2;        // RTT <= (2us) not considered; state unchanged. Can double as D_(minRTT)
const double d_minModelRttUs  = 50;       // minimum model RTT limit (50us) (T_low) for choosing which update case to run
const double d_maxModelRttUs = 1000;      // maximum model RTT limit (500 us) (T_high) for choosing which update case to run

const double d_maxNicBps;                 // Maximum NIC bandwidth (bytes/sec) shared by connected sessions

const double d_minRateBps = 15*1000*1000; // minimum transmit rate bytes/sec; application rate must >= this limit (optional)
const double d_maxRateBps = d_maxNicBps;  // maximum transmit rate bytes/sec; application rate must <= this limit (optional)
```

Timely also maintains the following state held in C++ class members or C-struct:

```
double d_lineRateBps;                     // calculated, bounded TX rate (bytes-per-second) applications use
double d_rawLineRateBps;                  // calculated TX rate (bytes-per-second) before bounding
double d_prevRttUs;                       // last RTT (us) provided to 'update'
double d_emwaDiffUs;                      // last EWMA RTT difference
```

## Fluid Confusion
A major problem with [1] is its putative Timely algorithm in section 4.1 or 4.3 bears small resemblence to its fluid equations in figure 7. The paper does not engage the question of whether the fluid model could form the basis of a Timely implementation in code. It makes many references to `C, q(t)` which in general sends don't know. [1] also describes `q(t)` as queue length (bytes) whereas Timely [3] describes it as the time required to drain the congestion point's queue. In this section I analyze the fluid equations.

[1] Equation 20 reads:

```
dq(t) bytes                      
----          = sum(R_i(t)) - C  bytes/sec
 dt   sec
```

which just conveys the queue size in bytes at the congestion point changes in time equal to the difference between all the sender rates `R_i(t))` and `C` at time `t`. `C` is the rate at which the congestion point drains its queue. 

The change in the computed rate (equation 21) builds on `\tau_{i}^*`, and `\tau^'`:

```
   *
tau  = max(Seg/R_i, D_minRtt)  rate update interval (units = time)
   i


   '   q(t)    MTU
tau  = ---- +  --- + D_prop    feed back delay      (units = time)
        C       C
```

`\tau_{i}^*` is the minimum time to put something on to the wire for sender `i` at rate `R_i`. `D_minRtt` enforces a maximum update frequency. Timely updates can start only after this period elapses. `\tau^'` is misnamed. It should reference RTT. When a sender puts new data onto the wire, it's enqueued at the end of the congestion point's buffer. `\tau^' is the time it takes to flush all the earlier packets plus last packets adding `D_prop` to get a signal back. As such the term `t-`\tau^'` models the time `now` minus the last RTT.

The rate update equation 21 gives the amount by which the rate `R_i` for sender `i` changes time. It's a multi-function selecting an update formula depending on how RTTs compare to model bounds `[T_low, T_high]`. I now focus on the formulas for selecting a case.

F





 







doc erpc changes to basic timely
doc fluid model [1] v. timely [3]




# Milestone#1: Basic Timely Model
[1] probably does the best job at describing the Timely model update function. It fixes a bug in the original algorithm described in [3]. The fluid models in [1] can be found in [2]. However, things get complicated from here. First, eRPC [7] adds factors (`delta_factor, md_factor`) that do not appear in [3]. Its alpha/beta parameters are considerably different compared to [1]. The fluid models [2] are mathlab simulation code. Although easy to follow, the input to the Timely update method takes queue length not RTT. And it's not clear that the fluid model algorithm is a valid for use in code. [1] writes:

```
The fluid model (by its very nature) essentially assumes smooth and continuous transmission of data. The TIMELY
implementation is more bursty, since rate is adjusted by modulating gaps between transmission of 16 or 64KB
chunks.
```

Note that eRPC [6,7] unlike the Timely paper [3] is not sending prepared large chunks of data e.g. 16 or 64Kb. Most RPCs have small requests probably in one packet less than 1K. Responses, however, could be large and variable.

## Timely Basic Model
[This example implements the patched Timely model from ECN paper](https://github.com/gshanemiller/congestion/blob/main/experiment/timely_basic/timely.h). It periodically computes negative rates e.g. what the code calls raw values. Based on this code, the model parameters [do not look quite right](https://github.com/gshanemiller/congestion/blob/main/experiment/timely_basic/data.png):

* Starting in the top-right graph, RTTs start at 225us model mid-point increasing until the model max is hit. Then RTTs descreases only until the TX rate is 80% of NIC bandwidth. It takes a good deal of time for the algorithm to recover to NIC bandwidth and does so very quickly.
* The bottom-right graph shows what happens then the RTT is centered on 45us under the model minimum and sproatically exceeding the minimum. As long there a good number of packets under RTTs under the minimum, the TX can recover. Periodic RTTs over the min make the overall trend bound around.
* Bottom-left shows what happens when RTTs are centered on 60us which is between the model min/max but closer to the min
* Top-left shows what happens when RTTs are centered and the model mid-point

## Timely Model As Implemented in eRPC
[This example implements the eRPC Timely Model with ECN's patch](https://github.com/gshanemiller/congestion/blob/main/experiment/timely_erpc/timely.h). It's alpha/beta/deltas are rather different. eRPC mixes in additional factors (`md_factor, ai_factor, delta_factor`) not in the basic model. eRPC also imposes several limits on the new calculated rate once Timely computations are done. [The resulting TX rates look better](https://github.com/gshanemiller/congestion/blob/main/experiment/timely_erpc/data.png):

* Starting in the top-right graph, RTTs start at 475us model mid-point increasing until the model max is hit. Then RTTs descreases only until the TX rate is 80% of NIC bandwidth. It takes a good deal of time for the algorithm to recover to NIC bandwidth but does so gradually.
* The bottom-right graph shows what happens then the RTT is centered on 45us under the model minimum and sproatically exceeding the minimum. Unlike the basic model, the TX rate stays higher.
* Bottom-left shows what happens when RTTs are centered on 60us which is between the model min/max but closer to the min
* Top-left shows what happens when RTTs are centered under the model minimum but exceed the model minimum (stddev=4) which higher frequency
