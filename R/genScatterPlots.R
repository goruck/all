### R script to generate scatterplots for sensor and clock data.
### (c) Lindo St. Angel 2016

### setup
remove(list=ls())
set.seed(1234)
df = read.csv("/home/lindo/dev/R/panelSimpledb-single.csv")
zaKeep = c("za1","za16","za27","za28","za29","za30","za32")
#zaKeep = NULL
#zdKeep = c("zd1","zd16","zd27","zd28","zd29","zd30","zd32")
zdKeep = NULL
patterns <- c("pattern1", "pattern2", "pattern3", "pattern4", "pattern5",
              "pattern6", "pattern7", "pattern8", "pattern9", "pattern10")
keep = c("clock", zaKeep, zdKeep, patterns)
dfKeep = df[sample(nrow(df)), keep]

### convert any pattern column NA's into FALSE's
dfKeep[patterns][is.na(dfKeep[patterns])] <- FALSE

### extract hour from timestamp, stay in UTC
extractHour2 <- function(dateTime) {
  op <- options(digits.secs = 3) # 3 digit precision on seconds
  td <- strptime(dateTime, format = "%Y-%m-%dT%H:%M:%OSZ", tz = "UTC")
  h <- as.POSIXlt(td)$hour #+ (as.POSIXlt(td)$min)/60
  options(op) # restore options
  return(h)
}
### replace date / time stamps with only observation hour in local time
dfKeep["clock"] <- lapply(dfKeep["clock"], extractHour2)

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

### apply the limit function to all elements except the clock and pattern columns
drops <- c("clock", patterns)
dfKeep[, !(names(dfKeep) %in% drops)] <- apply(dfKeep[, !(names(dfKeep) %in% drops)], c(1, 2), limitNum)
df2Keep[, !(names(df2Keep) %in% drops)] <- apply(df2Keep[, !(names(df2Keep) %in% drops)], c(1, 2), limitNum)

### look at scatterplot of sensors data
attach(dfKeep)
result <- as.factor(dfKeep[, "pattern6"]) # response shows up as red (true) or blk (false)
toPlot <- c("clock", "za29", "za27", "za28") #time & upstairs, front, hall motion
plot(dfKeep[toPlot], pch=19, col=result) #pch=19 plots solid circles
