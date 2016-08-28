/*
 * Read out new observations from SimpleDB and update database and models.
 *
 * (c) Lindo St. Angel 2016
 */

var AWS = require('/usr/lib/node_modules/aws-sdk');
AWS.config.region = 'us-west-2';
var simpledb = new AWS.SimpleDB({apiVersion: '2009-04-15'});
var fs = require('fs');

// form pattern for SimpleDB query
var d = new Date();
var n = '"'+d.toISOString()+'"';
var str = 'SELECT * FROM panelSimpledb WHERE clock < '+n+' ORDER BY clock DESC';

var params = {
  SelectExpression: str
};

simpledb.select(params, function(err, data) {
  if (err) {
    console.log(err, err.stack);
  } else {
    // read timestamp of last observation
    var lastItem = fs.readFileSync('/home/pi/all/nodejs/lastItem.txt').toString();
    var newData = false;

    for (i = 0; i < data.Items.length; i++) {
      if (data.Items[i].Name > lastItem) { // there's new data
        newData = true;
        // alphanumeric sort
        var sortedItem = data.Items[i].Attributes.sort(function (a, b) {
          var reA = /[^a-zA-Z]/g;
          var reN = /[^0-9]/g;
          var nameA = a.Name.toUpperCase(); // ignore case
          var nameB = b.Name.toUpperCase(); // ignore case
          var aA = nameA.replace(reA, "");
          var bA = nameB.replace(reA, "");
          if (aA === bA) {
            var aN = parseInt(nameA.replace(reN, ""), 10);
            var bN = parseInt(nameB.replace(reN, ""), 10);
            return aN === bN ? 0 : aN > bN ? 1 : -1;
          } else {
            return aA > bA ? 1 : -1;
          }
        });

        // convert to array
        var arr = [];
        for (j = 0; j < sortedItem.length; j++) {
          arr.push(sortedItem[j].Value);
        }

        // write to file
        var options = {flag: 'a'};
        fs.writeFileSync('/home/pi/all/R/panelSimpledb.csv', arr+'\n', options);
        console.log(i+' wrote: '+arr);
      }
    }

    if (newData) {
      // save timestamp of last observation for next run
      options = {flag: 'w'};
      fs.writeFileSync('/home/pi/all/nodejs/lastItem.txt', data.Items[0].Name, options);
      console.log('last obs time: ' +data.Items[0].Name);

      // regenerate svm models with new data
      var exec = require('child_process').exec;
      var cmd = '/home/pi/R_HOME/R-3.1.2/bin/Rscript --vanilla /home/pi/all/R/genSvmModels2.R';

      exec(cmd, function(error, stdout, stderr) {
        console.log('stdout: ' + stdout);
        console.log('stderr: ' + stderr);
        if (error !== null) {
          console.log('exec error: ' + error);
        }
      });
    }
  }
});
