# Purpose
A minimal, complete example of Timely TX estimation computation. After making a Timely object, its update function is repeatedly called There are no networking calls, ports, or sockets used.

# Algorithm
This code uses [Datacenter RPCs can be General and Fast](https://www.usenix.org/system/files/nsdi19-kalia.pdf) as implemented in its [source code](https://github.com/erpc-io/eRPC)

# Usage
After building, run the code from this directory. It will produce four files `test1.dat, test2.dat, test3.dat, test4.dat`. For each test, the program prints the Timely state at the end of the test, plus a histogram of all the RTTs used in the simulation.

Next, run R stats program. Set its working directory to this directory. Then load and run `./plot.r`. Alternatively in R console `source('./plot.r')`. You'll get one graph per file.

# Credit: Histogram Code
I simplified and repurposed code in https://github.com/rtahmasbi/Histogram.git for use here
