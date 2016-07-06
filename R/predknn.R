### make a prediction from a knn model

cat("********** New R Run **********\n")

### setup
TEMPORAL_CUTOFF <- -120 # time limit in secs
TEMPORAL_VALUE  <- -120 # time limit value in secs
library(class) # for knn
### function to extract elasped secs in day from UTC timestamp
extractES <- function(dateTime) {
  op <- options(digits.secs = 3) # 3 digit precision on seconds
  td <- strptime(dateTime, format = "%Y-%m-%dT%H:%M:%OSZ", tz = "UTC")
  es <- 3600*as.POSIXlt(td)$hour + 60*as.POSIXlt(td)$min + as.POSIXlt(td)$sec
  options(op) # restore previous options
  return(es)
}
### function to limit values in the dataframe
### rnorm() is used in case values have 0 variance to make scale() work
limitNum <- function(num) {
  if (num < TEMPORAL_CUTOFF) {
    num <- rnorm(1, mean = TEMPORAL_VALUE, sd = 1)
  }
  return(num)
}

### get zone data from R's arguements and condition it
args = commandArgs(trailingOnly=TRUE)
ts <- args[1] # observation timestamp in UTC format
cat("observation date and time (UTC): ", ts, "\n")
es <- extractES(ts) # extract observation elasped secs
obsTime <- as.integer(args[2]) # observation time in seconds (derived from linux system time)
zoneTimes <- lapply(strsplit(args[3], ","), as.numeric)[[1]] # abs zone act/deact times
zoneRelTimes <- zoneTimes - obsTime # calulate relative zone act/deact times
#zoneRelActTimes <- zoneRelTimes[1:32]
#zoneRelDeActTimes <- zoneRelTimes[33:64]

### put observations into a dataframe and add column labels
oldw <- getOption("warn")
options(warn = -1) # supress warnings in case not all zones have data
clkAndZones <- c(es, zoneRelTimes) # combine clock and zone data
df <- data.frame(matrix(clkAndZones, nrow = 1, ncol = 65))
options(warn = oldw) # turn back on warnings
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

### load training data
df = read.csv("/home/pi/all/R/panelSimpledb.csv")
#df = read.csv("/home/pi/all/R/testFromSimpledb.csv")
#df = read.csv("/home/pi/all/R/knnTrain.csv")
#df = read.csv("/home/pi/dev/knnTrainMix.csv")

### calculate k
#n <- nrow(df) # number of observations
#k <- floor(sqrt(n)) # calculate k for knn
#k <- 13 # generally a good default value for current data set
#cat("n: ", n, "k: ", k, "\n")

### define function to make a prediction for a specific pattern
predictPattern <- function(zaKeep, zdKeep, pattern, df, k, useClk) {
  ### apply filters to data and randomize rows
  keep <- c("clock", zaKeep, zdKeep, pattern)
  dfKeep <- df[sample(nrow(df)), keep] # randomize rows

  ### replace any pattern NA's with FALSE
  #dfKeep[pattern][is.na(dfKeep[pattern])] <- FALSE

  ### select only TRUE and FALSE values, ignore NAs from other observations
  dfKeep  <- dfKeep[!is.na(dfKeep[pattern]), ]

  ### calculate number of observations and k for knn
  n <- nrow(dfKeep)
  if (n == 0) {
    retList <- list("knnPred" = "NA", "n" = n, "knnK" = "NA")
    return(retList)
  }
  knnK <- floor(sqrt(n))

  ### sample data to limit size of data set
  ###   make number of true obs == false obs
  ###   use if data set is getting too large
  #dfKeepTrue <- dfKeep[dfKeep[pattern] == TRUE, ]
  #x <- nrow(dfKeepTrue)
  #if (x == 0) return("*") # exit if there are no TRUE entries
  #y <- dfKeep[dfKeep[pattern] == FALSE, ] # get rows with pattern == FALSE
  #dfKeepFalse <- y[1:x, ] # take the first x of those rows
  #dfKeep <- rbind(dfKeepTrue, dfKeepFalse) # combine TRUE and FALSE rows
  #dfKeep <- dfKeep[sample(nrow(dfKeep)), ] # randomize rows
  #n <- 2*x # total number of observations
  #knnK <- floor(sqrt(n)) # calculate k for knn
  #cat("n: ", n, "k: ", knnK, " ")

  ### replace date / time stamps with only elasped secs in day
  dfKeep["clock"] <- lapply(dfKeep["clock"], extractES)
  
  ### apply the limit function to all elements except the clock and pattern columns
  drops <- c("clocks", pattern)
  dfKeep[, !(names(dfKeep) %in% drops)] <- apply(dfKeep[, !(names(dfKeep) %in% drops)], c(1, 2), limitNum)

  ### merge the training and test data
  dumColNum <- ncol(rawTestData) + 1
  rawTestData[dumColNum] <- 0 # create a dummy col to make rbind work ok
  colnames(rawTestData)[dumColNum] <- pattern
  dfKeep <- rbind(dfKeep, rawTestData) # note: converts pattern logical values to int

  ### scale the  combined data to have col mean = 0 and sd = 1
  drops <- pattern
  dfKeep[, !(names(dfKeep) %in% drops)] <- scale(dfKeep[, !(names(dfKeep) %in% drops)])

  ### form training and test data sets
  if (useClk == TRUE) {
    trainData = dfKeep[1:(nrow(dfKeep) - 1), 1:(ncol(dfKeep) - 1)]
    testData = dfKeep[nrow(dfKeep), 1:(ncol(dfKeep) - 1)]
  } else if (useClk == FALSE) {
    trainData = dfKeep[1:(nrow(dfKeep) - 1), 2:(ncol(dfKeep) - 1)]
    testData = dfKeep[nrow(dfKeep), 2:(ncol(dfKeep) - 1)]
  } else {
    retList <- list("knnPred" = "NA", "n" = n, "knnK" = knnK)
    return(retList)
  }

  ### form training and test labels
  trainLabels = dfKeep[1:(nrow(dfKeep) - 1), ncol(dfKeep)]
  #testLabels = dfKeep[nrow(dfKeep), ncol(dfKeep)] # not required in normal mode

  ### make prediction for pattern
  #cat("making prediction for pattern: ", pattern, "\n")
  knnPred <- knn(train = trainData, test = testData, cl = trainLabels, k = knnK, prob = TRUE)

  retList <- list("knnPred" = knnPred, "n" = n, "knnK" = knnK)
  return(retList)
  #return(knnPred)
}

#knnPred <- predictPattern(zaKeep, zdKeep, "pattern1", df, k, useClk = FALSE)
#cat("prediction 1: ", knnPred, " prob: ", attr(knnPred, "prob"), "\n")
ret <- predictPattern(zaKeep, zdKeep, "pattern1", df, k, useClk = FALSE)
cat("prediction 1: ", ret$knnPred, " prob: ", attr(ret$knnPred, "prob"), "k: ", ret$knnK, "n: ", ret$n, "\n")

#knnPred <- predictPattern(zaKeep, zdKeep, "pattern2", df, k, useClk = FALSE)
#cat("prediction 2: ", knnPred, " prob: ", attr(knnPred, "prob"), "\n")
ret <- predictPattern(zaKeep, zdKeep, "pattern2", df, k, useClk = FALSE)
cat("prediction 2: ", ret$knnPred, " prob: ", attr(ret$knnPred, "prob"), "k: ", ret$knnK, "n: ", ret$n, "\n")

#knnPred <- predictPattern(zaKeep, zdKeep, "pattern3", df, k, useClk = FALSE)
#cat("prediction 3: ", knnPred, " prob: ", attr(knnPred, "prob"), "\n")
ret <- predictPattern(zaKeep, zdKeep, "pattern3", df, k, useClk = FALSE)
cat("prediction 3: ", ret$knnPred, " prob: ", attr(ret$knnPred, "prob"), "k: ", ret$knnK, "n: ", ret$n, "\n")

#knnPred <- predictPattern(zaKeep, zdKeep, "pattern4", df, k, useClk = FALSE)
#cat("prediction 4: ", knnPred, " prob: ", attr(knnPred, "prob"), "\n")
ret <- predictPattern(zaKeep, zdKeep, "pattern4", df, k, useClk = FALSE)
cat("prediction 4: ", ret$knnPred, " prob: ", attr(ret$knnPred, "prob"), "k: ", ret$knnK, "n: ", ret$n, "\n")

#knnPred <- predictPattern(zaKeep, zdKeep, "pattern5", df, k, useClk = FALSE)
#cat("prediction 5: ", knnPred, " prob: ", attr(knnPred, "prob"), "\n")
ret <- predictPattern(zaKeep, zdKeep, "pattern5", df, k, useClk = FALSE)
cat("prediction 5: ", ret$knnPred, " prob: ", attr(ret$knnPred, "prob"), "k: ", ret$knnK, "n: ", ret$n, "\n")

#knnPred <- predictPattern(zaKeep, zdKeep, "pattern6", df, k, useClk = TRUE)
#cat("prediction 6: ", knnPred, " prob: ", attr(knnPred, "prob"), "\n")
ret <- predictPattern(zaKeep, zdKeep, "pattern6", df, k, useClk = TRUE)
cat("prediction 6: ", ret$knnPred, " prob: ", attr(ret$knnPred, "prob"), "k: ", ret$knnK, "n: ", ret$n, "\n")

#knnPred <- predictPattern(zaKeep, zdKeep, "pattern7", df, k, useClk = TRUE)
#cat("prediction 7: ", knnPred, " prob: ", attr(knnPred, "prob"), "\n")
ret <- predictPattern(zaKeep, zdKeep, "pattern7", df, k, useClk = TRUE)
cat("prediction 7: ", ret$knnPred, " prob: ", attr(ret$knnPred, "prob"), "k: ", ret$knnK, "n: ", ret$n, "\n")

#knnPred <- predictPattern(zaKeep, zdKeep, "pattern8", df, k, useClk = TRUE)
#cat("prediction 8: ", knnPred, " prob: ", attr(knnPred, "prob"), "\n")
ret <- predictPattern(zaKeep, zdKeep, "pattern8", df, k, useClk = TRUE)
cat("prediction 8: ", ret$knnPred, " prob: ", attr(ret$knnPred, "prob"), "k: ", ret$knnK, "n: ", ret$n, "\n")

ret <- predictPattern(zaKeep, zdKeep, "pattern9", df, k, useClk = TRUE)
cat("prediction 9: ", ret$knnPred, " prob: ", attr(ret$knnPred, "prob"), "k: ", ret$knnK, "n: ", ret$n, "\n")

ret <- predictPattern(zaKeep, zdKeep, "pattern10", df, k, useClk = TRUE)
cat("prediction 10: ", ret$knnPred, " prob: ", attr(ret$knnPred, "prob"), "k: ", ret$knnK, "n: ", ret$n, "\n")

cat("********** End R Run **********\n")

