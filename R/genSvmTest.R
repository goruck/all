### R script to generate an SVM model from sensor and clock data.
### Copyright (c) Lindo St. Angel 2016

### setup
remove(list=ls())
set.seed(5634)
library(e1071) # for svm
df <- read.csv("/home/lindo/dev/R/panelSimpledb.csv")
zaKeep <- c("za1","za16","za27","za28","za29","za30","za32")
#zaKeep <- NULL
#zdKeep <- c("zd1","zd16","zd27","zd28","zd29","zd30","zd32")
zdKeep <- NULL
patterns <- c("pattern1", "pattern2", "pattern3", "pattern4", "pattern5",
              "pattern6", "pattern7", "pattern8", "pattern9", "pattern10")
keep <- c("clock", zaKeep, zdKeep, patterns)
dfKeep <- df[sample(nrow(df)), keep]

### convert any pattern column NA's into FALSE's
dfKeep[patterns][is.na(dfKeep[patterns])] <- FALSE

### function to find the pattern number of a true observation
findPatNum <- function(v) {
  return(match("TRUE", v, nomatch = 0))
}

### extract hour from timestamp, stay in UTC
extractHour <- function(dateTime) {
  op <- options(digits.secs = 3) # 3 digit precision on seconds
  td <- strptime(dateTime, format = "%Y-%m-%dT%H:%M:%OSZ", tz = "UTC")
  h <- as.POSIXlt(td)$hour #+ (as.POSIXlt(td)$min)/60
  options(op) # restore options
  return(h)
}
### replace date / time stamps with only observation hour in UTC
dfKeep["clock"] <- lapply(dfKeep["clock"], extractHour)

### function to limit values in the dataframe
TEMPORAL_CUTOFF <- -120 # 2 minutes ago
TEMPORAL_VALUE  <- -120 # limit to 2 minutes
limitNum <- function(num) {
  if (num < TEMPORAL_CUTOFF) {
    num <- TEMPORAL_VALUE
  }
  return(num)
}
### apply the limit function to all elements except the clock and pattern columns
drops <- c("clock", patterns)
dfKeep[, !(names(dfKeep) %in% drops)] <- apply(dfKeep[, !(names(dfKeep) %in% drops)], c(1, 2), limitNum)

### Divide data into training (67%) and test (33%) sets
ind <- sample(2, nrow(dfKeep), replace=TRUE, prob=c(0.67, 0.33))

### create labels for data sets
patNum <- apply(dfKeep[patterns], 1, findPatNum)
trainLabels <- patNum[ind == 1]
testLabels <- patNum[ind == 2]

### drop individual patterns from dataset
keep = c("clock", zaKeep, zdKeep) 
dfKeep = dfKeep[, keep]

### calculate optimal svm model w/o clock as a predictor
trainData = data.frame(x = dfKeep[ind==1, 2:ncol(dfKeep)], y = as.factor(trainLabels))
testData = data.frame(x = dfKeep[ind==2, 2:ncol(dfKeep)], y = as.factor(testLabels))

svmTuneOut = tune(svm, y~., data = trainData, kernel = "radial", scale = TRUE, probability = TRUE,
                  ranges = list(cost = c(0.1, 1, 10, 100, 1000), gamma = c(0.5, 1, 2, 3, 4)))

svmOpt = svm(y~., data = trainData, kernel = "radial", gamma = svmTuneOut$best.model$gamma, cost = svmTuneOut$best.model$cost,
             scale = TRUE, decision.values = FALSE, probability = TRUE)

### training error of optimal svm model w/o clock as a predictor
svmPred = predict(svmOpt, trainData, probability = TRUE)
table(predict = svmPred, truth = trainLabels)
### test error of optimal svm model w/o clock as a predictor
svmPred = predict(svmOpt, testData, probability = TRUE)
table(predict = svmPred, truth = testLabels)

### calculate optimal svm model with clock as a predictor
trainData = data.frame(x = dfKeep[ind==1, 1:ncol(dfKeep)], y = as.factor(trainLabels))
testData = data.frame(x = dfKeep[ind==2, 1:ncol(dfKeep)], y = as.factor(testLabels))

svmTuneOut = tune(svm, y~., data = trainData, kernel = "radial", scale = TRUE, probability = TRUE,
                  ranges = list(cost = c(0.1, 1, 10, 100, 1000), gamma = c(0.5, 1, 2, 3, 4)))

svmOpt = svm(y~., data = trainData, kernel = "radial", gamma = svmTuneOut$best.model$gamma, cost = svmTuneOut$best.model$cost,
             scale = TRUE, decision.values = FALSE, probability = TRUE)

### training error of optimal svm model with clock as a predictor
svmPred = predict(svmOpt, trainData, probability = TRUE)
table(predict = svmPred, truth = trainLabels)
### test error of optimal svm model with clock as a predictor
svmPred = predict(svmOpt, testData, probability = TRUE)
table(predict = svmPred, truth = testLabels)
###
