### Make a prediction on new sensor data; this version uses two svm models
### One model with clock as predictor and one model w/o it
### Each model is multi-classifier SVM that predicts all patterns
### This output should be processed by rules to decide which
### prediction to keep. 
### (c) Lindo St. Angel 2016

cat("********** New R Run (svm2) **********\n")

### setup
MOD_NO_CLK_FN <- "/home/pi/all/R/models/pattern-nc.svm" # file name of svm model w/o clock predictor
MOD_CLK_FN <- "/home/pi/all/R/models/pattern-c.svm" # file name of svm model w/ clock predictor
TEMPORAL_CUTOFF <- -120 # time limit in secs
TEMPORAL_VALUE  <- -120 # time limit value in secs
library(e1071) # for svm
### function to extract hour from UTC timestamp
extractHr <- function(dateTime) {
  op <- options(digits.secs = 3) # 3 digit precision on seconds
  td <- strptime(dateTime, format = "%Y-%m-%dT%H:%M:%OSZ", tz = "UTC")
  hr <- as.POSIXlt(td)$hour # + (as.POSIXlt(td)$min)/60
  options(op) # restore previous options
  return(hr)
}
### function to limit values in the dataframe
limitNum <- function(x) {
  if (x < TEMPORAL_CUTOFF) {
    x <- TEMPORAL_VALUE
  }
  return(x)
}

### get zone data from R's arguements and condition it
args = commandArgs(trailingOnly=TRUE)
ts <- args[1] # observation timestamp in UTC format
cat("timestamp:", ts, "\n")
hr <- extractHr(ts) # extract observation hour
obsTime <- as.integer(args[2]) # observation time in seconds (derived from linux system time)
zoneTimes <- lapply(strsplit(args[3], ","), as.numeric)[[1]] # abs zone act/deact times
zoneRelTimes <- zoneTimes - obsTime # calulate relative zone act/deact times

### put observations into a dataframe and add column labels
clkAndZones <- c(hr, zoneRelTimes) # combine clock and zone data
df <- data.frame(matrix(clkAndZones, nrow = 1, ncol = 65))
colnames(df) <- c("clock","za1","za2","za3","za4","za5","za6","za7","za8",
                  "za9","za10","za11","za12","za13","za14","za15","za16",
                  "za17","za18","za19","za20","za21","za22","za23","za24",
                  "za25","za26","za27","za28","za29","za30","za31","za32",
                  "zd1","zd2","zd3","zd4","zd5","zd6","zd7","zd8",
                  "zd9","zd10","zd11","zd12","zd13","zd14","zd15","zd16",
                  "zd17","zd18","zd19","zd20","zd21","zd22","zd23","zd24",
                  "zd25","zd26","zd27","zd28","zd29","zd30","zd31","zd32")

### filters to select zone data of interest
### z1 = front door; z16 = family room slider; z27 = front motion
### z28 = hall motion; z29 = upstairs motion; z30 = playroom motion; z32 = playroom door
zaKeep = c("za1","za16","za27","za28","za29","za30","za32")
#zaKeep = NULL
#zdKeep = c("zd1","zd16","zd27","zd28","zd29","zd30","zd32")
zdKeep = NULL

### select test data set, apply temporal filter
### note: not applying temporal filer to clock data
keep <- c(zaKeep, zdKeep)
newData <- df[keep]
newData[newData < TEMPORAL_CUTOFF] <- TEMPORAL_VALUE

### add back in clock data column and output test observation
newData <- merge(df["clock"], newData)
print(newData, row.names = FALSE)
  
### load svm model for patterns, it will be called "svmOpt"
### then run a prediction on the new data and output results
### two models are used, one that has clock as a predictor and one that does not

### load model w/ clock as a predictor
load(MOD_CLK_FN)

### form test data set w/ clock data
testData <- data.frame(x = newData[, 1:ncol(newData)], y = as.factor(0))

### make predictions
svmPred <- predict(svmOpt, testData, probability = TRUE)

predNumClk <- as.character(svmPred) # convert factor to character
probsClk <- attributes(svmPred)$probabilities
predNumProbClk <- probsClk[1, colnames(probsClk) == predNumClk]

### load model w/o clock as a predictor
load(MOD_NO_CLK_FN)

### form test data set w/o clock data
testData <- data.frame(x = newData[, 2:ncol(newData)], y = as.factor(0))

### make predictions
svmPred <- predict(svmOpt, testData, probability = TRUE)

predNum <- as.character(svmPred) # convert factor to character
probs <- attributes(svmPred)$probabilities
predNumProb <- probs[1, colnames(probs) == predNum]

### output prediction number and its probability
### note: pred of 0 indicates no pattern was identified
### note: leading 0 is removed from the prob estimate and only 2 sig digits returned
cat("pred:", predNum, "prob:", sub("^0.", "\\1.", sprintf("%.2f", predNumProb)), "(w/o clk) |",
    "pred:", predNumClk, "prob:", sub("^0.", "\\1.", sprintf("%.2f", predNumProbClk)), "(w/clk)\n")

cat("********** End R Run (svm2) **********\n")
