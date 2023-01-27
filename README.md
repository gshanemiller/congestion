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

# Milestones
Congestion control involves four major problems:

* Detect and correct packet drop/loss. Detection usually involves timestamps and sequence numbers. Resending packets is more involved
* Detect and respond to congestion by not sending too much data too soon e.g. [1-5, 9]
* And with (2) determine when to send new data without exasperbating congestion e.g. [8]
* Do all of the above without wasting CPU

I suggest the following trajectory. It's valid provided Timely is the goto congestion control method. I report other methods above, howeber, they impose impactical constraints.

```
Milestones
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

# Milestone#1: Basic Timely Model
[1] probably does the best job at describing the Timely model update function. It fixes a bug in the original algorithm described in [4]. The fluid models in [1] can be found in [2]. However, things get complicated from here. First, eRPC [7] adds factors (`delta_factor, md_factor`) that do not appear in [1]. Its alpha/beta parameters are considerably different. The fluid models [2] are mathlab simulation code. Although easy to follow, the input the Timely update method is takes inputs of queue length not RTT. And it's not clear that the fluid model algorithm is a valid for use in code. [1] writes:

```
      The fluid model (by its very nature) essentially assumes smooth and continuous transmission of data. The TIMELY
      implementation is more bursty, since rate is adjusted by modulating gaps between transmission of 16 or 64KB
      chunks.
```

[This repository contains a basic Timely model](https://github.com/gshanemiller/congestion/blob/main/experiment/timely_basic/timely.h). It periodically computes negative rates e.g. what the code calls raw values. The raw value is coerced into a valid value by applying min/max comparisons just before returning. The code in [7] is more complicated. It also coerces the raw value by bounding increases/decreases in rate.

So at the time of this writing it's not entirely clear what the algorithm really is. Ideally it'd be possible nail down Timely without getting drawn into a long benchmarking, model-fitting work where its parameters are reverse estimated.

Based on the code as it now stands, the model parameters [do not look quite right](https://github.com/gshanemiller/congestion/blob/main/experiment/timely_basic/data.png):

* In the second (middle) plot we see that when the starting RTT (here 225us) far off the min, the TX rate drops to the minimum immediately. Once the max RTT is hit (500us at t=0.02sec) it takes until ~0.25sec for the rate to recover even though RTTs are slowly decreasing only
* In the third (bottom) plot RTTs are sampled at a mean of 60us which is only 10us higher than the model minimum. Even though all RTTs hug the lower end of the model, the TX rate never exceeds 0.04GB/sec which way off the NIC max. The problem here is the RTTs uses the gradient calculation. It never adds only because the RTTs only improbably drop under the modelMin of 50us. And since the RTTs fluctuate around 60us going high and lower, the TX rate never budges.
* And as I point out above, there are zillions of raw rates < 0
