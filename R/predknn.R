### make a prediction from a knn model

#print("********** New R Run **********")

### get zone data from R's arguements and make it relative to observation times
args = commandArgs(trailingOnly=TRUE)
obsTime <- as.integer(args[1])
#obsTime
zoneTimes <- lapply(strsplit(args[2], ","), as.numeric)[[1]]
#zoneTimes
zoneRelTimes <- zoneTimes - obsTime
#zoneRelTimes
#zoneRelActTimes <- zoneRelTimes[1:32]
#zoneRelActTimes
#zoneRelDeActTimes <- zoneRelTimes[33:64]
#zoneRelDeActTimes

### put observations into a data frame and add column labels
#oldw <- getOption("warn")
#options(warn = -1) # supress warnings in case not all zones have data
df <- data.frame(matrix(zoneRelTimes, nrow = 1, ncol = 64))
#options(warn = oldw) # turn back on warnings
colnames(df) <- c("za1","za2","za3","za4","za5","za6","za7","za8",
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

### select data set, apply temporal filter
keep = c(zaKeep, zdKeep)
testData = df[keep]
testData[testData < -120] <- -9999 # nothing older than 2 mins
#testData
#cat("testData: ", testData, "\n")

### load training data
df = read.csv("/home/pi/all/R/testFromSimpledb.csv")
#df = read.csv("/home/pi/all/R/knnTrain.csv")
#df = read.csv("/home/pi/dev/knnTrainMix.csv")

### define function to make a prediction for a specific pattern
predictPattern <- function(zaKeep, zdKeep, pattern, df) {

  ### condition training data for pattern
  keep = c(zaKeep, zdKeep, pattern)
  dfKeep = df[sample(nrow(df)), keep]
  trainData = dfKeep[, 1:(ncol(dfKeep)-1)]
  trainData[trainData < -120] <- -9999
  trainLabels = dfKeep[, ncol(dfKeep)]

  ### make prediction for pattern
  library(class)
  #cat("making prediction for pattern: ", pattern, "\n")
  knnPred = knn(train = trainData, test = testData, cl = trainLabels, k = 15, prob = TRUE)

  return(knnPred)

}

knnPred <- predictPattern(zaKeep, zdKeep, "pattern1", df)
cat("*** R *** prediction 1: ", knnPred, " prob: ", attr(knnPred, "prob"), "\n")

knnPred <- predictPattern(zaKeep, zdKeep, "pattern2", df)
cat("*** R *** prediction 2: ", knnPred, " prob: ", attr(knnPred, "prob"), "\n")

knnPred <- predictPattern(zaKeep, zdKeep, "pattern3", df)
cat("*** R *** prediction 3: ", knnPred, " prob: ", attr(knnPred, "prob"), "\n")

#print("********** End R Run **********")

