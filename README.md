# Purpose
The purpose of this repository is to provide a *Practitioner's Guide* to implementing a userspace only network packet congestion control scheme to be used with low-latency a messaging library on intra-corporate networks. This work shall be complete when,

* Concepts for congestion control, packet drop/loss are well explained
* Implementation choices are well described
* Code is provided to concretely demonstrate concepts and implementation
* A guide is provided on how congestion control might be incorporate into a DPDK messaging library

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

* [1] contains a correction to [3] and also describes DCQCN with ECN but not Carousel. ECN requires intravening switches/routers to be ECN enabled. Equinix does not or cannot do ECN, for example.
* [1] concludes DCQCN is better than Timely
* [9] describes another approach handily beating Timely, however, the "deployment of RCC relies on high precision clock synchronization throughout the datacenter network. Some recent research efforts can reduce the upper bound of clock synchronization within a datacenter to a few hundred nanoseconds, which is sufficient for our work."
* Timely congestion control therefore is the least proscriptive, and probably worst performing
* eRPC [6,7] uses Carousel [8] and Timely [3,4,5] to good effect
* It's not clear at the time of this writing exactly where Timely stops and Carousel roles and responsibilities start

# Problem Areas
Congestion control involves four major problems and their solutions:

1. Detect and correct packet drop/loss. Detection usually involves sequence numbers while resending data is more involved
2. Detect and respond to congestion by not sending too much data too soon e.g. [1-5, 9]
3. And with (2) determine when to send new data without exasperbating congestion e.g. [8]
4. Do all of the above without wasting CPU

# Build Code
In an empty directory do the following assuming you have a C++ tool chain and cmake installed:

1. git clone https://github.com/gshanemiller/congestion.git
2. mkdir build
3. cmake ..

All tasks/libraries can be found in the './build' directory
