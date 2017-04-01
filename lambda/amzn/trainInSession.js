/*
 * Tag observations for machine learning. Store in AWS SimpleDB database.
 * TODO: add more error checking.
 * 
 */

var shared = require('../shared.js');

var trainInSession = function(intent, session, callback) {
    var cardTitle = intent.name;
    var patternSlot = intent.slots.Pattern; // names of pattern to train
    var stateSlot = intent.slots.State; // states of observation
    var sessionAttributes = {};
    var repromptText = "";
    var shouldEndSession = true;
    var speechOutput = "";
    const NUM_OF_PATTERNS = 10; // max number of patterns to train
    
    var AWS = require('aws-sdk');
    AWS.config.region = 'us-west-2';
    var simpledb = new AWS.SimpleDB({apiVersion: '2009-04-15'});

    if (patternSlot.Value === 0) {
        speechOutput = "error, pattern 0 is the null case so you cannot tag it";
        callback(sessionAttributes,
                 shared.buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
    } else if (patternSlot.Value > NUM_OF_PATTERNS) {
        speechOutput = "error, you cannot tag pattern greater than "+NUM_OF_PATTERNS;
        callback(sessionAttributes,
                 shared.buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
    } else {
        shared.getPanelStatus('sendJSON', function writeObsToDb(panelStatus) { // 'sendJSON' returns zone status as JSON
            var d = new Date();
            var n = d.toISOString(); // use ISO format to enable lexicographical sort in SimpleDB
            var obj = JSON.parse(panelStatus);
            var obsTime = obj.obsTime;
            var zoneAct = obj.zoneAct;
            var relZoneAct = shared.calcRelT(obsTime, zoneAct);
            var zoneDeAct = obj.zoneDeAct;
            var relZoneDeAct = shared.calcRelT(obsTime, zoneDeAct);

            // build SimpleDB attributes
            var att = [{Name:'clock',Value:n},{Name:'sample',Value:obsTime.toString()}];
            for (var i = 0; i < relZoneAct.length; i++) { // zone active times
                var name = 'za'+(i+1).toString();
                var value = relZoneAct[i].toString();
                var obj = {Name:name,Value:value};
                att.push(obj);
            }
            for (i = 0; i < relZoneDeAct.length; i++) { // zone deactivation times
                name = 'zd'+(i+1).toString();
                value = relZoneDeAct[i].toString();
                obj = {Name:name,Value:value};
                att.push(obj);
            }
            for (i = 0; i < NUM_OF_PATTERNS; i++) { // observation state
                name = 'zzpattern'+(i+1).toString(); // prepend 'zz' to get ordering right
                if (patternSlot.value == (i+1)) { // use '==' to force type conversion
                    // no guarantee that Alexa ASR will return value in lower case
                    (stateSlot.value.toLowerCase() === 'true') ? value = 'TRUE' : value = 'FALSE';
                } else {
                    value = 'NA'; // set value to NA for all other patterns
                }
                obj = {Name:name,Value:value};
                att.push(obj);
            }

            // write observations to database
            var params = {
                Attributes: att,
                DomainName: 'panelSimpledb',
                ItemName: n
            };
            simpledb.putAttributes(params, function(err, data) {
                if (err) { 
                    console.log(err, err.stack);
                    speechOutput = "error, unable to tag observation";
                } else {
                    console.log(data);
                    speechOutput = "tagged pattern,"+patternSlot.value+",as,"+stateSlot.value;
                }
                callback(sessionAttributes,
                         shared.buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
            });
        });
    }
}

// Export function so modules can use it externally.
module.exports.trainInSession = trainInSession;
