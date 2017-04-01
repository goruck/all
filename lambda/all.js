/**
 * Lambda function for security system monitoring and control triggred by Alexa.
 * 
 * This sample demonstrates a simple skill built with the Amazon Alexa Skills Kit.
 * 
 * For details see https://github.com/goruck.
 *
 * For additional samples, visit the Alexa Skills Kit Getting Started guide at
 * http://amzn.to/1LGWsLG.
 * 
 * Copyright (c) 2016 Lindo St. Angel
 */

// External modules. 
var shared = require('./shared.js');
var amzn = require('./amzn/trainInSession.js');

// Route the incoming request based on type (LaunchRequest, IntentRequest,
// etc.) The JSON body of the request is provided in the event parameter.
exports.handler = function (event, context) {
    try {
        console.log("event.session.application.applicationId=" + event.session.application.applicationId);

        /**
         * Prevents someone else from configuring a skill that sends requests to this function.
         */
        if (event.session.application.applicationId !== "amzn1.echo-sdk-ams.app.7d25022e-92dd-4a1c-8814-c6ec6caaaa9d") {
             context.fail("Invalid Application ID");
         }

        if (event.session.new) {
            onSessionStarted({requestId: event.request.requestId}, event.session);
        }

        if (event.request.type === "LaunchRequest") {
            onLaunch(event.request,
                     event.session,
                     function callback(sessionAttributes, speechletResponse) {
                        context.succeed(buildResponse(sessionAttributes, speechletResponse));
                     });
        }  else if (event.request.type === "IntentRequest") {
            onIntent(event.request,
                     event.session,
                     function callback(sessionAttributes, speechletResponse) {
                         context.succeed(buildResponse(sessionAttributes, speechletResponse));
                     });
        } else if (event.request.type === "SessionEndedRequest") {
            onSessionEnded(event.request, event.session);
            context.succeed();
        }
    } catch (e) {
        context.fail("Exception: " + e);
    }
};

/**
 * Called when the session starts.
 */
function onSessionStarted(sessionStartedRequest, session) {
    console.log("onSessionStarted requestId=" + sessionStartedRequest.requestId +
                ", sessionId=" + session.sessionId);
}

/**
 * Called when the user launches the skill without specifying what they want.
 */
function onLaunch(launchRequest, session, callback) {
    console.log("onLaunch requestId=" + launchRequest.requestId +
                ", sessionId=" + session.sessionId);

    // Dispatch to skill's launch.
    getWelcomeResponse(callback);
}

/**
 * Called when the user specifies an intent for this skill.
 */
function onIntent(intentRequest, session, callback) {
    console.log("onIntent requestId=" + intentRequest.requestId +
                ", sessionId=" + session.sessionId +
                ", intentName=" + intentRequest.intent.name);

    var intent = intentRequest.intent,
        intentName = intentRequest.intent.name;

    // Dispatch to skill's intent handlers
    if ("MyNumIsIntent" === intentName) {
        sendKeyInSession(intent, session, callback);
    } else if ("MyCodeIsIntent" === intentName) {
        sendCodeInSession(intent, session, callback);
    } else if ("WhatsMyStatusIntent" === intentName) {
        getStatusFromSession(intent, session, callback);
    } else if ("PollyIsIntent" === intentName) {
        sendPolyInSession(intent, session, callback);
    } else if ("TrainIsIntent" === intentName) {
        // Tag observations for machine learning. Store in AWS SimpleDB database.
        amzn.trainInSession(intent, session, callback);
    } else if ("OccupancyIsIntent" === intentName) {
        anyoneHomeInSession(intent, session, callback);
    } else if ("PredIsIntent" === intentName) {
        predInSession(intent, session, callback);
    } else if ("AMAZON.HelpIntent" === intentName) {
        getWelcomeResponse(callback);
    } else if ("AMAZON.StopIntent" === intentName) {
        stopSession(callback);
    } else if ("AMAZON.CancelIntent" === intentName) {
        stopSession(callback);
    } else {
        throw "Invalid intent";
    }
}

/**
 * Called when the user ends the session.
 * Is not called when the skill returns shouldEndSession=true.
 */
function onSessionEnded(sessionEndedRequest, session) {
    console.log("onSessionEnded requestId=" + sessionEndedRequest.requestId +
            ", sessionId=" + session.sessionId);
    // Add cleanup logic here
}

// --------------- Functions that control the skill's behavior -----------------------

function stopSession(callback) {
    var sessionAttributes = {};
    var cardTitle = "Goodbye";
    var speechOutput = "goodbye";
    var shouldEndSession = true;
    var repromptText = "";

    callback(sessionAttributes,
             shared.buildSpeechletResponse(cardTitle, speechOutput, repromptText, shouldEndSession));
}

function getWelcomeResponse(callback) {
    // If we wanted to initialize the session to have some attributes we could add those here.
    var sessionAttributes = {};
    var cardTitle = "Welcome";
    var speechOutput = "Please ask the security system its status, or give it a command," +
                       "valid commands are the names of a keypad button," +
                       "a 4 digit code, or say polly to bypass hallway motion and arm system";
    // If the user either does not reply to the welcome message, they will be prompted again.
    var repromptText = "To get the system's status, say status," +
                       "to activate a keypad button, say the name of a button from 0 to 9," +
                       "stay, away, pound, or star, for example, to activate pound, say pound, "+
                       "to send a 4 digit code, say the code, for example 1 2 3 4,";
    var shouldEndSession = false;

    callback(sessionAttributes,
             shared.buildSpeechletResponse(cardTitle, speechOutput, repromptText, shouldEndSession));
}

/*
 * Gets the panel keypress from the user in the session.
 * Check to make sure keypress is valid.
 * Prepares the speech to reply to the user.
 * If valid, sends the keypress to the panel over a TLS TCP socket.
 */
function sendKeyInSession(intent, session, callback) {
    var cardTitle = intent.name;
    var KeysSlot = intent.slots.Keys;
    var sessionAttributes = {};
    var repromptText = "";
    var shouldEndSession = true; // end session after sending keypress
    var speechOutput = "";

    var ValidValues = ['0','1', '2', '3', '4', '5', '6', '7', '8', '9', 'stay', 'away', 'star', 'pound'];
    var num = KeysSlot.value.toLowerCase(); // no guarantee that Alexa ASR will return value in lower case 
    var isValidValue = ValidValues.indexOf(num) > -1; // true if a valid value was passed
    
    if (KeysSlot) {
        if (!isValidValue) {
            speechOutput = num + ",is an invalid command," +
                                 "valid commands are the names of a keypad button," +
                                 "status, or a 4 digit code";
            callback(sessionAttributes,
                     shared.buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
        } else {
            shared.getPanelStatus('idle', function (panelStatus) { // check status first
                if (!panelStatus) { // panelStatus should never be an empty string...
                    speechOutput = "Error, request could not be completed,";
                    callback(sessionAttributes,
                             shared.buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
                } else if ((num === 'stay' || num === 'away') && isArmed(panelStatus)) {
                    speechOutput = "System is already armed,";
                    callback(sessionAttributes,
                             shared.buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
                } else if ((num === 'stay' || num === 'away') && findPlacesNotReady(panelStatus)) {
                    var placesNotReady = findPlacesNotReady(panelStatus); // get friendly names of zones not ready
                    speechOutput = "System cannot be armed, because these zones are not ready," +placesNotReady;
                    callback(sessionAttributes,
                             shared.buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
                } else {
                    shared.getPanelStatus(num, function(panelStatus) { // write num to panel and check return status
                        if (!(num === 'stay' || num === 'away')) { // a key that doesn't need verification
                            speechOutput = 'sent,' +num;
                            callback(sessionAttributes,
                                     shared.buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
                        } else {
                            setTimeout(function verifyArmCmd() { // verify stay or away arm command succeeded
                                shared.getPanelStatus('idle', function checkIfArmed(panelStatus) {
                                    if (isArmed(panelStatus)) {
                                        speechOutput = 'sent,' +num +',system was armed,';
                                    } else {
                                        speechOutput = 'sent,' +num +',error,, system could not be armed,';
                                    }
                                    callback(sessionAttributes,
                                             shared.buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
                                });
                            }, 1000); // wait 1 sec for command to take effect
                        }
                    });
                }
            });
        }
    } else {
        console.log('error in SendKeyInSession');
    }
}

/*
 * Gets the a 4 digit code from the user in the session.
 * Check for validity. 
 * Prepares the speech to reply to the user.
 * If valid, sends the code to the panel over a TLS TCP socket.
 */
function sendCodeInSession(intent, session, callback) {
    var cardTitle = intent.name;
    var CodeSlot = intent.slots.Code;
    var sessionAttributes = {};
    var repromptText = "";
    var shouldEndSession = true; // end session after sending code
    var speechOutput = "";
    
    if (CodeSlot) {
        var num = CodeSlot.value;
        sessionAttributes = createNumberAttributes(num);
        if(num === '?' || num > 9999) {
            speechOutput = num + ", is an invalid code," +
                                 "codes must be positive 4 digit integers, " +
                                 "not greater than 9999";
            callback(sessionAttributes,
                     shared.buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
        } else {
            shared.getPanelStatus(num, function() {
                speechOutput = "sent, " +num;
                callback(sessionAttributes,
                         shared.buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
            });
        }
    } else {
        console.log('error in SendCodeInSession');
    }
}

/*
 * Sends panel status to the user and ends session.
 */
function getStatusFromSession(intent, session, callback) {
    var cardTitle = intent.name,
        repromptText = "",
        sessionAttributes = {},
        shouldEndSession = true,
        speechOutput = "";

    shared.getPanelStatus('idle', function (panelStatus) {
        if (!panelStatus) { // panelStatus should never be an empty string...
            speechOutput = "Error, request could not be completed,";
        } else if (isArmed(panelStatus)) {
            isBypassed(panelStatus) ? speechOutput = 'system is armed and bypassed' : speechOutput = 'system is armed';
        } else if (!findPlacesNotReady(panelStatus)) { // no zones are reporting activity or are tripped
            hasError(panelStatus) ? speechOutput = 'system is ready but has an error' : speechOutput = 'system is ready';
        } else { // system must not be ready
            var placesNotReady = findPlacesNotReady(panelStatus); // get friendly names of zones not ready
            speechOutput = 'these zones are not ready,' +placesNotReady;
        }

        callback(sessionAttributes,
                 shared.buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
    });
}

/*
 * Condition alarm for the dog to stay at home.
 * Bypasses the hallway motion sensor and sets 'away' arm mode.
 * Prepares the speech to reply to the user.
 * Sends commands to the panel over a TLS TCP socket.
 */
function sendPolyInSession(intent, session, callback) {
    var cardTitle = intent.name;
    var sessionAttributes = {};
    var repromptText = "";
    var shouldEndSession = true; // end session after sending keypresses
    var speechOutput = "";
    var panelStatus = "";
    const MODE = 1; // bypass mode
    const ZONE = 28; // hall motion zone
    
    shared.getPanelStatus('idle', function (panelStatus) { // check status first
        if (!panelStatus) { // panelStatus should never be an empty string...
            speechOutput = "Error, request could not be completed,";
            callback(sessionAttributes,
                     shared.buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
        } else if (isArmed(panelStatus)) {
            speechOutput = "System is already armed,";
            callback(sessionAttributes,
                     shared.buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
        } else if (findPlacesNotReady(panelStatus)) {
            var placesNotReady = findPlacesNotReady(panelStatus); // get friendly names of zones not ready
            speechOutput = "System cannot be armed, because these zones are not ready," +placesNotReady;
            callback(sessionAttributes,
                     shared.buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
        } else {
            shared.getPanelStatus('star', function enterCommandMode() { // enter command mode
                shared.getPanelStatus(MODE, function setMode() { // set bypass mode
                    setTimeout(function() {
                        shared.getPanelStatus(ZONE, function bypassZone() { // bypass zone
                            setTimeout(function() {
                                shared.getPanelStatus('pound', function () { // exit command mode
                                    setTimeout(function() {
                                        shared.getPanelStatus('away', function armAway() { // set away arm mode
                                            setTimeout(function verifyArmCmd() { // verify stay or away arm command succeeded
                                                shared.getPanelStatus('idle', function checkIfArmed(panelStatus) {
                                                    if (isArmed(panelStatus)) {
                                                        speechOutput = 'system was armed with hallway motion sensor bypassed';
                                                    } else {
                                                        speechOutput = 'error, system could not be armed';
                                                    }
                                                    callback(sessionAttributes,
                                                             shared.buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
                                                });
                                            }, 1000); // wait 1 s for arm command to take effect
                                        });
                                    }, 500); // wait 500 ms to exit bypass mode
                                });
                            }, 500); // wait 500 ms for zone to bypass
                        });
                    }, 1000); // wait 1 s for bypass mode to take effect 
                });
            });
        }
    });
}

/*
 * Check to see if anyone is at home.
 *
 */
function anyoneHomeInSession(intent, session, callback) {
    var cardTitle = intent.name;
    var sessionAttributes = {};
    var repromptText = "";
    var shouldEndSession = true;
    var speechOutput = "";

    shared.getPanelStatus('sendJSON', function (panelStatus) { // 'sendJSON' returns zone status as JSON
        var obj = JSON.parse(panelStatus);
        var numOcc = obj.numOcc;

        if (!numOcc) {
            speechOutput = "there is probably no one at home right now";
        } else if (numOcc === 1) {
            speechOutput = "there is probably at least one person at home right now";
        } else if (numOcc > 1) {
            speechOutput = "there are probably at least "+numOcc.toString()+" people at home right now";
        }
        callback(sessionAttributes,
                 shared.buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
    });
}

/*
 * Return the last true prediction.
 *
 */
function predInSession(intent, session, callback) {
    var cardTitle = intent.name;
    var sessionAttributes = {};
    var repromptText = "";
    var shouldEndSession = true;
    var speechOutput = "";
    const offset = -420; // -7 * 60 mins offset between PT and UTC

    shared.getPanelStatus('sendJSON', function (panelStatus) { // 'sendJSON' returns zone status as JSON
        var obj = JSON.parse(panelStatus);
        var lastTruePred = obj.lastTruePred; // array containing timestamps of true predictions

        // find the prediction that was last true
        var maxms = 0,
            lastTrueIdx = NaN, // array index of last true prediction in lastTruePred
            ms = 0;
        for (var i = 0; i < lastTruePred.length; i++) {
            if (lastTruePred[i] != "") {
                ms = Date.parse(lastTruePred[i]); // milliseconds since January 1, 1970, 00:00:00 UTC
                if (ms > maxms) {
                    lastTrueIdx = i;
                    maxms = ms;
                }  
            }
        }

        if (lastTrueIdx === NaN) {
            speechOutput = "no true predictions have been made";
        } else {
            var timeStamp = lastTruePred[lastTrueIdx]; // timestamp of last true prediction

            // Convert timestamp in UTC to local time  
            var utcms = Date.parse(timeStamp); // milliseconds since January 1, 1970, 00:00:00 UTC
            var localms = utcms + (offset * 60000); // local time in ms
            var local = new Date(localms); // local time in date / time format
            var mo = (local.getMonth() + 1).toString();
            if (mo.length === 1) {
                mo = "0"+mo;
            }
            var d  = local.getDate().toString();
            if (d.length === 1) {
                d = "0"+d;
            }
            var h  = local.getHours().toString();
            var m  = local.getMinutes().toString();
            if (m.length === 1) {
                m = "O"+m;
            }

            speechOutput = predToPatt(lastTrueIdx)+" at "+h+" "+m+" on <say-as interpret-as='date'>????"+mo+d+"</say-as>";
        }

        callback(sessionAttributes,
                 buildSpeechletResponseSSML(intent.name, speechOutput, repromptText, shouldEndSession));
    });
}

// --------------- global variables and commonly used functions -----------------------

function isReady(panelStatus) {
    return (panelStatus.indexOf('LED Status Ready') > -1) ? true : false; // true if system is ready
}

function isArmed(panelStatus) {
    return (panelStatus.indexOf('Armed') > -1) ? true : false; // true if system is armed
}

function hasError(panelStatus) {
    return (panelStatus.indexOf('Error') > -1) ? true : false; // true if system has flagged an error condition
}

function isBypassed(panelStatus) {
    return (panelStatus.indexOf('Bypass') > -1) ? true : false; // // true if one or more zones are bypassed
}

/*
 * This is a more reliable way of determining if the system is ready to be armed.
 * isReady() can also be used but is prone to falsing because the motion sensors cause the system
 * to momentarly report a not ready condition which causes problems later on when finding out which zones aren't ready.
 *
 * NOTE: function replaced by findPlacesNotReady() since it ignores motion sensors which desired to check before arming. 
 */
function zonesNotActive(panelStatus) {
    var zoneRegex = /(Zone\d \d{1,2})((, \d{1,2}){1,8})?/g; // find zones with numbers, indicating activity
    var zonesActive = panelStatus.match(zoneRegex); // array with active zones
    return (zonesActive === null) ? true : false; // true if zones report no activity
}

/*
 * Mapping from alarm zones to friendly place names.
 */
function zoneToPlace(zone, sensor) {
    var places = {
        "Zone1": {
            "1":  "front door",
	    "2":  "living room left window",
	    "3":  "living room right window",
	    "4":  "dining room left window",
	    "5":  "dining room center left window",
	    "6":  "dining room center right window",
	    "7":  "dining room right window",
	    "8":  "kitchen left window"
        },
        "Zone2": {
            "1":  "kitchen right window",
	    "2":  "breakfast nook left window",
	    "3":  "breakfast nook side right window",
	    "4":  "breakfast nook left window",
	    "5":  "breakfast nook center left window",
	    "6":  "breakfast nook center right window",
	    "7":  "breakfast nook right window",
	    "8":  "family room slider door"
        },
        "Zone3": {
            "1":  "family room left window",
	    "2":  "family room center window",
	    "3":  "family room right window",
            "4":  "guest bedroom left window",
	    "5":  "guest bedroom center left window",
	    "6":  "guest bedroom center right window",
	    "7":  "guest bedroom right window",
	    "8":  "first floor bathroom window"
        },
        "Zone4": {
            "1":  "front landing window",
	    "2":  "upstairs slider",
	    "3":  "front motion",
	    "4":  "hall motion",
	    "5":  "upstairs motion",
	    "6":  "playroom motion",
	    "7":  "playroom window",
	    "8":  "playroom door"
        }
    };
    return places[zone][sensor];
}

/*
 * Find friendly names of places not ready to be armed.
 * Assumes at least one zone is not ready. Will fail otherwise.
 * Ignores motion sensors since they aren't relevant for arming.
 * Returns empty string if all zones are ready.
 */
function findPlacesNotReady(panelStatus) {
    var placesNotReady = '';
    var zoneRegex = /(Zone\d \d{1,2})((, \d{1,2}){1,8})?/g;
    var zonesNotReady = panelStatus.match(zoneRegex); // array with zones not ready
    if (zonesNotReady) { // one or more zones aren't ready...find out which one(s)
        for (var i = 0; i < zonesNotReady.length; i++) {
            var zoneNotReady = zonesNotReady[i].slice(0,5);
            var sensorRegex = /\d{1,2}/g;
            var sensorsNotReady = zonesNotReady[i].slice(6).match(sensorRegex); // array with sensors in zone not ready
            for (var j = 0; j < sensorsNotReady.length; j++) {
                var sensorNotReady = sensorsNotReady[j];
                var place = zoneToPlace(zoneNotReady, sensorNotReady);
                if (place.indexOf("motion") === -1) { // ignore motion sensors
	            placesNotReady += place +',';
                }
            }
        }
    }
    return placesNotReady;
}

/*
 * Mapping from prediction number to friendly pattern names.
 */
function predToPatt(predNum) {
    var pattern = {
        1: "from front door to playroom via hallway",
        2: "from outside to playroom",
        3: "from family room to playroom",
        4: "slow walk from kid's rooms to playroom via hallway",
        5: "fast walk from kid's rooms to playroom via hallway",
        6: "from master bedroom to kitchen in the early morning",
        7: "undefined",
        8: "from playroom to kid's rooms via hallway",
        9: "undefined",
        10:"undefined"
    }
    return pattern[predNum];
}

function createNumberAttributes(key) {
    return {
        key : key
    };
}

// --------------- Helpers that build all of the responses -----------------------

function buildSpeechletResponseSSML(title, output, repromptText, shouldEndSession) {
    return {
        outputSpeech: {
            type: "SSML",
            "ssml": "<speak>"+output+"</speak>"
        },
        card: {
            type: "Simple",
            title: "SessionSpeechlet - " + title,
            content: "SessionSpeechlet - " + output
        },
        reprompt: {
            outputSpeech: {
                type: "SSML",
                "ssml": "<speak>"+repromptText+"</speak>"
            }
        },
        shouldEndSession: shouldEndSession
    };
}

function buildResponse(sessionAttributes, speechletResponse) {
    return {
        version: "1.0",
        sessionAttributes: sessionAttributes,
        response: speechletResponse
    };
}
