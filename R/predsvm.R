### make a prediction from an svm model

cat("********** New R Run (svm) **********\n")

### setup
MODPATH <- "/home/pi/all/R/models/" # path to svm models
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
###   rnorm() is used in case values have 0 variance to make scale() work ok
limitNum <- function(x) {
  if (x < TEMPORAL_CUTOFF) {
    x <- rnorm(1, mean = TEMPORAL_VALUE, sd = 1)
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
zaKeep = c("za1","za16","za27","za28","za29","za30","za32")
#zaKeep = NULL
#zdKeep = c("zd1","zd16","zd27","zd28","zd29","zd30","zd32")
zdKeep = NULL

### select test data set, apply temporal filter
### note: not applying temporal filer to clock data
keep <- c(zaKeep, zdKeep)
rawTestData <- df[keep]
rawTestData[rawTestData < TEMPORAL_CUTOFF] <- TEMPORAL_VALUE

### add back in clock data column and output test observation
rawTestData <- merge(df["clock"], rawTestData)
print(rawTestData, row.names = FALSE)

### define function to make a prediction for a specific pattern
predictPattern <- function(pattern, useClk, newData) {
  
  ### load svm model for pattern
  ###  if model is found it will be called "svmOpt"
  modelFileName <- paste(pattern, sep = ".", "svm")
  pathToModel <- paste(MODPATH, sep = "", modelFileName)
  if (file.exists(pathToModel)) {
    load(pathToModel)
  } else {
    return("*")
  }

  ### create a dummy column
  dumColNum <- ncol(newData) + 1
  newData[dumColNum] <- FALSE
  colnames(newData)[dumColNum] <- pattern

  ### form test data set
  trainLabels = newData[, ncol(newData)]
  if (useClk == TRUE) {
    testData <- data.frame(x = newData[, 1:(ncol(newData)-1)], y = as.factor(trainLabels))
  } else if (useClk == FALSE) {
    testData <- data.frame(x = newData[, 2:(ncol(newData)-1)], y = as.factor(trainLabels))
  } else {
    return("*")
  }

  ### make prediction for pattern
  svmPred <- predict(svmOpt, testData, probability = TRUE)

  return(svmPred)
}

svmPred <- predictPattern("pattern1", FALSE, rawTestData)
cat("prediction 1: ", svmPred, " probability: ", attr(svmPred, "prob"), "\n")

svmPred <- predictPattern("pattern2", FALSE, rawTestData)
cat("prediction 2: ", svmPred, " probability: ", attr(svmPred, "prob"), "\n")

svmPred <- predictPattern("pattern3", FALSE, rawTestData)
cat("prediction 3: ", svmPred, " probability: ", attr(svmPred, "prob"), "\n")

svmPred <- predictPattern("pattern4", FALSE, rawTestData)
cat("prediction 4: ", svmPred, " probability: ", attr(svmPred, "prob"), "\n")

svmPred <- predictPattern("pattern5", FALSE, rawTestData)
cat("prediction 5: ", svmPred, " probability: ", attr(svmPred, "prob"), "\n")

svmPred <- predictPattern("pattern6", TRUE, rawTestData)
cat("prediction 6: ", svmPred, " probability: ", attr(svmPred, "prob"), "\n")

svmPred <- predictPattern("pattern7", TRUE, rawTestData)
cat("prediction 7: ", svmPred, " probability: ", attr(svmPred, "prob"), "\n")

svmPred <- predictPattern("pattern8", TRUE, rawTestData)
cat("prediction 8: ", svmPred, " probability: ", attr(svmPred, "prob"), "\n")

svmPred <- predictPattern("pattern9", FALSE, rawTestData)
cat("prediction 9: ", svmPred, " probability: ", attr(svmPred, "prob"), "\n")

svmPred <- predictPattern("pattern10", FALSE, rawTestData)
cat("prediction 10: ", svmPred, " probability: ", attr(svmPred, "prob"), "\n")

cat("********** End R Run (svm) **********\n")
