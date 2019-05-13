mydata <- read.table("\home\pfoytik\Documents\Blockchain\simulation\raft_node_processTimes.csv", header=TRUE, 
                     sep=",", row.names="id")
##Windows
windowsFonts(A=windowsFont("Times New Roman"))
par(family="A")

##Linux
par(family="Times New Roman")

calibrated3 <- read.csv(file="c:/Users/pfoytik/Documents/BlockchainPresentations/3-nodes_simbobs.csv", header=TRUE, sep=",")
calibrated4 <- read.csv(file="c:/Users/pfoytik/Documents/BlockchainPresentations/4-nodes_simbobs.csv", header=TRUE, sep=",")
calibrated5 <- read.csv(file="c:/Users/pfoytik/Documents/BlockchainPresentations/5-nodes_simbobs.csv", header=TRUE, sep=",")

avg_1mess <- c(1.32E+09,1.51E+09,1.65E+09,2.17E+09,2.58E+09)
avg_4mess <- c(1.42E+09,1.58E+09,1.72E+09,2.21E+09,2.60E+09)
avg_10mess <- c(1.51E+09,1.64E+09,1.77E+09,2.25E+09,2.62E+09)

##Plot single average with error bars
plot(avg_1mess, type="o", col="blue", xaxt="n", yaxt="n", pch="0", lty=1, xlab="number of peers", ylab="Simulated Time for 1000 Consensus ns")
xdata <- c(3, 4, 5, 10 ,16)
yax <- c("1.4", "1.8", "2.2", "2.4", "( x 10^09 )")
axis(2, at=c(1.4e09, 1.8e09, 2.2e09, 2.4e09, 2.62e09), labels=yax)
axis(1, at=1:5, labels=xdata)
arrows(x, min_1mess, x, max_1mess, length=0.05, angle=90, code=3, col="black")
legend(1,2.4e+09,legend=c("1message"), col=c("blue"), pch=c("o"),lty=c(1,2,3), ncol=1)

##Plot multiple average values
plot(avg_1mess, type="o", col="blue", cex.lab=1.5, xaxt="n", yaxt="n", pch="0", lwd=3, lty=1, xlab="Number of Peers", ylab="Simulated Time for 1000 Consensus (milliseconds)")
legend(1,2.4e+09,legend=c("1Message","4Message","10Message"), , bty="n", col=c("blue","red","black"), pch=c("o","*","+"),lty=c(1,2,3), lwd=c(3,3,3), ncol=1)
xdata <- c(3, 4, 5, 10 ,16)
yax <- c("1400", "1800", "2200", "2600")
axis(1, at=1:5, labels=xdata, cex.axis=1.5)
axis(2, at=c(1.4e09, 1.8e09, 2.2e09, 2.6e09), labels=yax, cex.axis=1.5)
points(avg_4mess, col="red", pch="*")
lines(avg_4mess, col="red", lwd=3, lty=2)
points(avg_10mess, col="black", pch="+")
lines(avg_10mess, col="black", lwd=3, lty=3)

##Scatter plots for calibration results
par(mfrow=c(1,2))
xdata3 <- c("1", "3", "5")
xdata5 <- c("5", "10", "15")
yax3 <- c("2", "4", "6", "8", "10")
yax5 <- c("2", "4", "6", "8", "10", "12")
plot(calibrated3[,1], calibrated3[,2], main="3 Node", cex.lab=1.5, xaxt="n", yaxt="n", xlab="Simulated (milliseconds)", ylab="Observed (milliseconds)", pch=19)
abline(lm(calibrated3[,1]~calibrated3[,2]), col="red", lwd=2)
axis(1, at=c(1e06, 3e06, 5e06), labels=xdata3, cex.axis=1.5)
axis(2, at=c(2e06, 4e06, 6e06, 8e06, 10e06), labels=yax3, cex.axis=1.5)
plot(calibrated5[,1], calibrated5[,2], main="5 Node", cex.lab=1.5, xaxt="n", yaxt="n", xlab="Simulated (milliseconds)", ylab="Observed (milliseconds)", pch=19)
abline(lm(calibrated5[,1]~calibrated5[,2]), col="red", lwd=2)
axis(1, at=c(5e06, 10e06, 15e06), labels=xdata5, cex.axis=1.5)
axis(2, at=c(2e06, 4e06, 6e06, 8e06, 10e06, 12e06), labels=yax5, cex.axis=1.5)

##Node Process Data and Distribution fitting
limited_mydata <- read.table("/home/pfoytik/Documents/Blockchain/simulation/raft_node_processTimes_limited.csv", header=TRUE, sep=",")
plot.legend <- c("weibull", "normal", "lognormal")
fit_n <- fitdist(limited_mydata[,1], "norm")
fit_ln <- fitdist(limited_mydata[,1], "lnorm")
fit_w <- fitdist(limited_mydata[,1], "weibull")
xdata <- c("0", "0.1", "0.4", "0.6", "0.8", "1")
ydata <- c("0", "0.001", "0.002", "0.003", "0.004")
denscomp(list(fit_w, fit_n, fit_ln), xaxt="n", yaxt="n", breaks=100, lwd=2, legendtext=plot.legend, xlim=c(0,1000000), cex.lab=1.5, xlab="Data (millisecond)")
legend(6e+05,4e-06,legend=c("weibull", "normal", "lognormal"), col=c("red", "green", "blue"), lty=c(1,2,3), ncol=1)
axis(1, at=c(0, 2e+05, 4e+05, 6e+05, 8e+05, 10e05), labels=xdata, cex.axis=1.5)
axis(2, at=c(0, 1e-06, 2e-06, 3e-06, 4e-06), labels=ydata, cex.axis=1.5)