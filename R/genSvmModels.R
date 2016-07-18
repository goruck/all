### generate svm models for panel sensory data

### setup
remove(list=ls())
library(e1071)
MODPATH <- "/home/pi/all/R/models/"
set.seed(9878)
df = read.csv("/home/pi/all/R/panelSimpledb.csv")
zaKeep = c("za1","za16","za27","za28","za29","za30","za32")
#zaKeep = NULL
#zdKeep = c("zd1","zd16","zd27","zd28","zd29","zd30","zd32")
zdKeep = NULL
dfKeep = df[sample(nrow(df)), ]

### extract hour from timestamp, stay in UTC
### this version uses the base package - loads faster
extractHour <- function(dateTime) {
  op <- options(digits.secs = 3) # 3 digit precision on seconds
  td <- strptime(dateTime, format = "%Y-%m-%dT%H:%M:%OSZ", tz = "UTC")
  h <- as.POSIXlt(td)$hour # + (as.POSIXlt(td)$min)/60
  options(op) # restore options
  return(h)
}

### function to limit values in the dataframe
### rnorm() is used in case values have 0 variance to make scale() work
TEMPORAL_CUTOFF <- -120 # 2 minutes ago
TEMPORAL_VALUE  <- -120 # limit to 2 minutes
limitNum <- function(num) {
  if (num < TEMPORAL_CUTOFF) {
    num <- rnorm(1, mean = TEMPORAL_VALUE, sd = 1)
  }
  return(num)
}

### function to generate svm models for each pattern number supplied
genSvmMod <- function(pattern, useClk) {

  ### select data of interest
  keep = c("clock", zaKeep, zdKeep, pattern)
  dfKeep <- dfKeep[, keep]

  ### convert any pattern column NA's into FALSE's
  dfKeep[pattern][is.na(dfKeep[pattern])] <- FALSE

  ### make sure data has both TRUE and FALSE levels
  ###   return if not because model can't be generated if not
  dfKeepT <- dfKeep[dfKeep[pattern] == TRUE, ]
  dfKeepF <- dfKeep[dfKeep[pattern] == FALSE, ]
  if (nrow(dfKeepT) == 0 || nrow(dfKeepF) == 0) {
    return("*")
  }

  ### replace date / time stamps with only observation hour in local time
  dfKeep["clock"] <- lapply(dfKeep["clock"], extractHour)

  ### apply the limit function to all elements except the clock and pattern columns
  drops <- c("clock", pattern)
  dfKeep[, !(names(dfKeep) %in% drops)] <- apply(dfKeep[, !(names(dfKeep) %in% drops)], c(1, 2), limitNum)

  ### create training data and labels
  trainLabels = dfKeep[, ncol(dfKeep)]
  if (useClk == TRUE) {
    trainData = data.frame(x = dfKeep[, 1:(ncol(dfKeep)-1)], y = as.factor(trainLabels))
  } else if (useClk == FALSE) {
    trainData = data.frame(x = dfKeep[, 2:(ncol(dfKeep)-1)], y = as.factor(trainLabels))
  } else {
    return("*")
  }

  ### find optimal svm model for the pattern
  svmTuneOut = tune(svm, y~., data = trainData, kernel = "radial", scale = TRUE, probability = TRUE,
                    ranges = list(cost = c(0.1, 1, 10, 100, 1000), gamma = c(0.5, 1, 2, 3, 4)))

  ### generate and save the model
  svmOpt = svm(y~., data = trainData, kernel = "radial", probability = TRUE, scale = TRUE,
               gamma = svmTuneOut$best.model$gamma, cost = svmTuneOut$best.model$cost)
  modelFileName <- paste(pattern, sep = ".", "svm")
  save(svmOpt, file = paste(MODPATH, sep = "", modelFileName))

}

### generate the models
genSvmMod("pattern1", FALSE)
genSvmMod("pattern2", FALSE)
genSvmMod("pattern3", FALSE)
genSvmMod("pattern4", FALSE)
genSvmMod("pattern5", FALSE)
genSvmMod("pattern6", TRUE)
genSvmMod("pattern7", TRUE)
genSvmMod("pattern8", TRUE)
genSvmMod("pattern9", FALSE)
genSvmMod("pattern10", FALSE)
