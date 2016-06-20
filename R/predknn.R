### make a prediction from a knn model

cat("********** New R Run **********\n")

### setup
TEMPORAL_CUTOFF <- -120 # 120 seconds ago
TEMPORAL_VALUE  <- -9999 # a long time ago
library(class) # for knn
### extract hour from timestamp (stay in UTC tz)
extractHour <- function(dateTime) {
  op <- options(digits.secs = 3) # 3 digit precision on seconds
  td <- strptime(dateTime, format = "%Y-%m-%dT%H:%M:%OSZ", tz = "UTC")
  h <- round(as.POSIXlt(td)$hour + as.POSIXlt(td)$min/60)
  options(op) # restore previous options
  return(h)
}
### function to limit values in the dataframe
limitNum <- function(num) {
  if (num < TEMPORAL_CUTOFF) num <- TEMPORAL_VALUE
  return(num)
}

### get zone data from R's arguements and condition it
args = commandArgs(trailingOnly=TRUE)
ts <- args[1] # observation timestamp in UTC format
hr <- extractHour(ts) # extract observation hour
obsTime <- as.integer(args[2]) # observation time in seconds (derived from linux system time)
zoneTimes <- lapply(strsplit(args[3], ","), as.numeric)[[1]] # abs zone act/deact times
zoneRelTimes <- zoneTimes - obsTime # calulate relative zone act/deact times
#zoneRelActTimes <- zoneRelTimes[1:32]
#zoneRelDeActTimes <- zoneRelTimes[33:64]

### put observations into a data frame and add column labels
oldw <- getOption("warn")
options(warn = -1) # supress warnings in case not all zones have data
df <- data.frame(matrix(c(hr, zoneRelTimes), nrow = 1, ncol = 66))
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

### select test data set, apply temporal filter, print observation
keep <- c(zaKeep, zdKeep)
rawTestData <- df[keep]
rawTestData[rawTestData < TEMPORAL_CUTOFF] <- TEMPORAL_VALUE
rawTestData <- merge(df["clock"], rawTestData)
print(rawTestData, row.names = FALSE)

### load training data and calculate k
df = read.csv("/home/pi/all/R/testFromSimpledb.csv")
#df = read.csv("/home/pi/all/R/knnTrain.csv")
#df = read.csv("/home/pi/dev/knnTrainMix.csv")
k <- floor(sqrt(nrow(df)))
#k <- 13 # generally a good default value for current data set
cat("k: ", k, "\n")

### define function to make a prediction for a specific pattern
predictPattern <- function(zaKeep, zdKeep, pattern, df, k) {

  ### apply filters to data and randomize rows
  keep <- c("clock", zaKeep, zdKeep, pattern)
  dfKeep <- df[sample(nrow(df)), keep] # randomize

  #dfKeepTrue <- dfKeep[dfKeep$pattern == TRUE, ]
  #x <- nrow(dfKeepTrue)
  #y <- dfKeep[dfKeep$pattern == FALSE, ]
  #dfKeepFalse <- y[1:x, ]
  #dfKeep <- rbind(dfKeepTrue, dfKeepFalse)

  ### replace date / time stamps with only observation hour
  dfKeep["clock"] <- lapply(dfKeep["clock"], extractHour)
  
  ### apply the limit function to all elements except the clock and pattern columns
  drops <- c("clocks", pattern)
  dfKeep[, !(names(dfKeep) %in% drops)] <- apply(dfKeep[, !(names(dfKeep) %in% drops)], c(1, 2), limitNum)

  ### merge the training and test data
  rawTestData[9] <- 0 # create a dummy col to make rbind work ok
  colnames(rawTestData)[9] <- pattern
  dfKeep <- rbind(dfKeep, rawTestData)

  ### scale the  combined data to have col mean = 0 and sd = 1
  drops <- pattern
  dfKeep[, !(names(dfKeep) %in% drops)] <- scale(dfKeep[, !(names(dfKeep) %in% drops)])

  ### form training and test data sets
  trainData = dfKeep[1:(nrow(dfKeep) - 1), 2:(ncol(dfKeep) - 1)] # 2 = do not include time
  testData = dfKeep[nrow(dfKeep), 2:(ncol(dfKeep) - 1)] # 2 = do not include time

  ### form training and test labels
  trainLabels = dfKeep[1:(nrow(dfKeep) - 1), ncol(dfKeep)]
  #testLabels = dfKeep[nrow(dfKeep), ncol(dfKeep)] # not required in normal mode

  #trainData <- dfKeep[, 1:(ncol(dfKeep)-1)]
  #trainData[trainData < TEMPORAL_CUTOFF] <- TEMPORAL_VALUE
  #trainLabels <- dfKeep[, ncol(dfKeep)]

  ### make prediction for pattern
  #cat("making prediction for pattern: ", pattern, "\n")
  knnPred <- knn(train = trainData, test = testData, cl = trainLabels, k = k, prob = TRUE)

  return(knnPred)

}

knnPred <- predictPattern(zaKeep, zdKeep, "pattern1", df, k)
cat("prediction 1: ", knnPred, " prob: ", attr(knnPred, "prob"), "\n")

knnPred <- predictPattern(zaKeep, zdKeep, "pattern2", df, k)
cat("prediction 2: ", knnPred, " prob: ", attr(knnPred, "prob"), "\n")

knnPred <- predictPattern(zaKeep, zdKeep, "pattern3", df, k)
cat("prediction 3: ", knnPred, " prob: ", attr(knnPred, "prob"), "\n")

knnPred <- predictPattern(zaKeep, zdKeep, "pattern4", df, k)
cat("prediction 4: ", knnPred, " prob: ", attr(knnPred, "prob"), "\n")

knnPred <- predictPattern(zaKeep, zdKeep, "pattern5", df, k)
cat("prediction 5: ", knnPred, " prob: ", attr(knnPred, "prob"), "\n")

knnPred <- predictPattern(zaKeep, zdKeep, "pattern6", df, k)
cat("prediction 6: ", knnPred, " prob: ", attr(knnPred, "prob"), "\n")

knnPred <- predictPattern(zaKeep, zdKeep, "pattern7", df, k)
cat("prediction 7: ", knnPred, " prob: ", attr(knnPred, "prob"), "\n")

knnPred <- predictPattern(zaKeep, zdKeep, "pattern8", df, k)
cat("prediction 8: ", knnPred, " prob: ", attr(knnPred, "prob"), "\n")

cat("********** End R Run **********\n")

