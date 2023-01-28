# Purpose
The purpose of this repository is to provide a *Practitioner's Guide* to implementing a userspace only network packet congestion control scheme to be used with a low-latency UDP messaging library on intra-corporate networks. This work shall be complete when,

* Concepts for congestion control, packet drop/loss are well explained
* Implementation choices are well described
* Code is provided to concretely demonstrate concepts and implementation
* A guide is provided on how congestion control might be incorporate into a DPDK messaging library

# Invitation
This work is open to all interested parties although I will pursue it regardless to completion. Consider joining Discord Chat Topic #NetworkCongestionDevs, or reach me directly at '7532yahoo @ gmail.com' or create issue/PR. Congestion is brand new to me; I can use any help from smarter people.

# Papers
The following papers are a good starting place. Many papers are co-authored by Google, Intel, and Microsoft and/or are used in production. Packets moving between data centers through WAN is not in scope. This work is for packets moving inside a corporate network. No code uses the kernel; this is all user-space typically done inconjunction with DPDK/RDMA.

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
* Timely congestion control therefore is the least proscriptive, and probably worst performing
* eRPC [6,7] uses Carousel [8] and Timely [3,4,5] to good effect
* It's not clear at the time of this writing exactly where Timely stops and Carousel roles and responsibilities start

# Build Code
In an empty directory do the following assuming you have a C++ tool chain and cmake installed:

1. git clone https://github.com/gshanemiller/congestion.git
2. mkdir build
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

I suggest the following trajectory. It's valid provided Timely is the goto congestion control method. I report other methods above, howeber, they impose impactical constraints.

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

eRPC [3] uses Intel's `rdtsc()` to take time stamps eventually converting those measurements to microseconds when Timely's update function is run. [1] uses NIC provided timestamps. RTT is defined in [3] section 3.1:

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
    1/(10*1000*1000*1000) sec/bits * (64*1024*8) bits = .000052428sec ~= 52us
```

If RTTs are of the same order of 10 as the serialization time, RTT should clearly not be penalized for serialization delays. This time does not reflect congestion outside the transmitting box.

The Timely event loop is:

1. Fix initial sending rate R to preset value
2. Send data at rate R
3. Measure the RTT for data just sent
4. Compute new rate: R = timely(R, r)
5. Goto 2

The Timely model is equipped with a RTT bound called `[T_low, T_High]` which chooses how the TX rate is updated with a new RTT measure:

```
0               minModelRtt              maxModelRtt
+--------------------+----------------------+---------------->
| additive increase  | gradient based +/-   | multiplicative
| to new rate        | change to new rate   | decrease in new rate
```








# Milestone#1: Basic Timely Model
[1] probably does the best job at describing the Timely model update function. It fixes a bug in the original algorithm described in [4]. The fluid models in [1] can be found in [2]. However, things get complicated from here. First, eRPC [7] adds factors (`delta_factor, md_factor`) that do not appear in [1]. Its alpha/beta parameters are considerably different. The fluid models [2] are mathlab simulation code. Although easy to follow, the input to the Timely update method takes queue length not RTT. And it's not clear that the fluid model algorithm is a valid for use in code. [1] writes:

```
The fluid model (by its very nature) essentially assumes smooth and continuous transmission of data. The TIMELY
implementation is more bursty, since rate is adjusted by modulating gaps between transmission of 16 or 64KB
chunks.
```

Note that eRPC [6,7] unlike the Timely paper [3] is not sending prepared large chunks of data e.g. 16 or 64Kb. Most RPCs have small requests probably in one packet less than 1K. Responses, however, could be large. 

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
