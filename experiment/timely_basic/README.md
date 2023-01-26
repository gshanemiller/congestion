# Purpose
A minimal, complete example of Timely TX estimation computation. After making a Timely object, its update function is repeatedly called with example RTTs for 1 client-session. A R-stats compatible file is generated containing the resulting data, which can be used with a script (provided) to plot time vs. TX rate. There are no networking calls, ports, or sockets used.
