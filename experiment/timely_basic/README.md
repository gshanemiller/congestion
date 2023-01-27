# Purpose
A minimal, complete example of Timely TX estimation computation. After making a Timely object, its update function is repeatedly called with example RTTs for 1 client-session. A R-stats compatible file is generated containing the resulting data, which can be used with a script (provided) to plot time vs. TX rate. There are no networking calls, ports, or sockets used.

# Usage
After building, run the code from this directory. It will produce 3 files test1.dat, test2.dat, test3.dat

Next, run R stats program. Set it's working directory to this directory. Then load and run `plot.r`. Alternatively in R console `source('./plot.r')`
