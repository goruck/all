### generate svm models for panel sensory data
### note: this version generates one model for all patterns
### (c) Lindo St. Angel 2016

### setup
remove(list=ls())
library(e1071)
MODFILE <- "/home/pi/all/R/models/pattern.svm" # svm model path
USECLK <- TRUE # use clock in predictions?
df = read.csv("/home/pi/all/R/panelSimpledb.csv") # data file of observations
zaKeep = c("za1","za16","za27","za28","za29","za30","za32")
#zaKeep = NULL
#zdKeep = c("zd1","zd16","zd27","zd28","zd29","zd30","zd32")
zdKeep = NULL
patterns <- c("pattern1", "pattern2", "pattern3", "pattern4", "pattern5",
              "pattern6", "pattern7", "pattern8", "pattern9", "pattern10")
keep = c("clock", zaKeep, zdKeep, patterns)
### shuffle data and select columns of interest
dfKeep = df[sample(nrow(df)), keep]

### function to extract hour from timestamp, stay in UTC
extractHour <- function(dateTime) {
  op <- options(digits.secs = 3) # 3 digit precision on seconds
  td <- strptime(dateTime, format = "%Y-%m-%dT%H:%M:%OSZ", tz = "UTC")
  h <- as.POSIXlt(td)$hour # + (as.POSIXlt(td)$min)/60
  options(op) # restore options
  return(h)
}

### function to limit values in the dataframe
TEMPORAL_CUTOFF <- -120 # 2 minutes ago
TEMPORAL_VALUE  <- -120 # limit to 2 minutes
limitNum <- function(num) {
  if (num < TEMPORAL_CUTOFF) {
    num <- TEMPORAL_VALUE
  }
  return(num)
}

### function to find the pattern number of a true observation
findPatNum <- function(v) {
  return(match("TRUE", v, nomatch = 0))
}

### convert any pattern column NA's into FALSE's
dfKeep[patterns][is.na(dfKeep[patterns])] <- FALSE

### replace date / time stamps with only observation hour in local time
dfKeep["clock"] <- lapply(dfKeep["clock"], extractHour)

### apply the limit function to all elements except the clock and pattern columns
drops <- c("clock", patterns)
dfKeep[, !(names(dfKeep) %in% drops)] <- apply(dfKeep[, !(names(dfKeep) %in% drops)], c(1, 2), limitNum)

### create training data and labels
trainLabels <- apply(dfKeep[patterns], 1, findPatNum) # create new vector w/pattern number of true obs
keep <- c("clock", zaKeep, zdKeep) # drop individual patterns from dataset
dfKeep <- dfKeep[, keep]
if (USECLK == TRUE) {
  trainData = data.frame(x = dfKeep[, 1:ncol(dfKeep)], y = as.factor(trainLabels))
} else if (USECLK == FALSE) {
  trainData = data.frame(x = dfKeep[, 2:ncol(dfKeep)], y = as.factor(trainLabels))
}

### calculate optimal svm model
svmTuneOut = tune(svm, y~., data = trainData, kernel = "radial", scale = TRUE, probability = TRUE,
                  ranges = list(cost = c(0.1, 1, 10, 100, 1000), gamma = c(0.5, 1, 2, 3, 4)))

svmOpt = svm(y~., data = trainData, kernel = "radial", gamma = svmTuneOut$best.model$gamma, cost = svmTuneOut$best.model$cost,
             scale = TRUE, decision.values = FALSE, probability = TRUE)

### save svm model
save(svmOpt, file = MODFILE)
