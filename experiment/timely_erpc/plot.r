library(gridExtra)
library(ggplot2)

tableBasic1 <- read.table("test1.dat", header=TRUE, sep=",")
frameBasic1 <- as.data.frame(tableBasic1)
plotBasic1 <- ggplot(frameBasic1, aes(x=Time/1000/1000, y=Rate/1000/1000/1000)) + geom_point(size=.1)
plotBasic1 = plotBasic1 + labs(x="Time (sec)")
plotBasic1 = plotBasic1 + labs(y="TX Rate (GB/sec)")
plotBasic1 = plotBasic1 + labs(title="Timely Simulated Rate from eRPC w/Patch")
plotBasic1 = plotBasic1 + labs(subtitle="NIC 10GB/sec, alpha=.46, beta=.26, delta=5MB/sec, modelRttMinUs/Max=[50,1000]")
plotBasic1 = plotBasic1 + labs(caption="RTTs sampled from Normal Dist mean=48us, stddev=4")

tableBasic2 <- read.table("test2.dat", header=TRUE, sep=",")
frameBasic2 <- as.data.frame(tableBasic2)
plotBasic2 <- ggplot(frameBasic2, aes(x=Time/1000/1000, y=Rate/1000/1000/1000)) + geom_point(size=.1)
plotBasic2 = plotBasic2 + labs(x="Time (sec)")
plotBasic2 = plotBasic2 + labs(y="TX Rate (GB/sec)")
plotBasic2 = plotBasic2 + labs(title="Timely Simulated Rate from eRPC w/Patch")
plotBasic2 = plotBasic2 + labs(subtitle="NIC 10GB/sec, alpha=.46, beta=.26, delta=5MB/sec, modelRttMinUs/Max=[50,1000]")
plotBasic2 = plotBasic2 + labs(caption="RTTs increase to model max then decrease until 80% NIC hit")

tableBasic3 <- read.table("test3.dat", header=TRUE, sep=",")
frameBasic3 <- as.data.frame(tableBasic3)
plotBasic3 <- ggplot(frameBasic3, aes(x=Time/1000/1000, y=Rate/1000/1000/1000)) + geom_point(size=.1)
plotBasic3 = plotBasic3 + labs(x="Time (sec)")
plotBasic3 = plotBasic3 + labs(y="TX Rate (GB/sec)")
plotBasic3 = plotBasic3 + labs(title="Timely Simulated Rate from eRPC w/Patch")
plotBasic3 = plotBasic3 + labs(subtitle="NIC 10GB/sec, alpha=.46, beta=.26, delta=5MB/sec, modelRttMinUs/Max=[50,1000]")
plotBasic3 = plotBasic3 + labs(caption="RTTs sampled from Normal Dist mean=60us, stddev=4")

tableBasic4 <- read.table("test4.dat", header=TRUE, sep=",")
frameBasic4 <- as.data.frame(tableBasic4)
plotBasic4 <- ggplot(frameBasic4, aes(x=Time/1000/1000, y=Rate/1000/1000/1000)) + geom_point(size=.1)
plotBasic4 = plotBasic4 + labs(x="Time (sec)")
plotBasic4 = plotBasic4 + labs(y="TX Rate (GB/sec)")
plotBasic4 = plotBasic4 + labs(title="Timely Simulated Rate from eRPC w/Patch")
plotBasic4 = plotBasic4 + labs(subtitle="NIC 10GB/sec, alpha=.46, beta=.26, delta=5MB/sec, modelRttMinUs/Max=[50,1000]")
plotBasic4 = plotBasic4 + labs(caption="RTTs sampled from Normal Dist mean=45us, stddev=2")

grid.arrange(plotBasic1, plotBasic2, plotBasic3, plotBasic4)