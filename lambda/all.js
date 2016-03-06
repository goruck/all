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
 * Lindo St. Angel 2015/16.
 */

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
            getPanelStatus(function callback(panelStatus) { // get panel status, then process intent
                onIntent(panelStatus,
                         event.request,
                         event.session,
                         function callback(sessionAttributes, speechletResponse) {
                             context.succeed(buildResponse(sessionAttributes, speechletResponse));
                         });
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

    // Dispatch to your skill's launch.
    getWelcomeResponse(callback);
}

/**
 * Called when the user specifies an intent for this skill.
 */
function onIntent(panelStatus, intentRequest, session, callback) {
    console.log("panelStatus=" + panelStatus +
                ", onIntent requestId=" + intentRequest.requestId +
                ", sessionId=" + session.sessionId +
                ", intentName=" + intentRequest.intent.name);

    var intent = intentRequest.intent,
        intentName = intentRequest.intent.name;

    // Dispatch to your skill's intent handlers
    if ("MyNumIsIntent" === intentName) {
        sendKeyInSession(panelStatus, intent, session, callback);
    } else if ("MyCodeIsIntent" === intentName) {
        sendCodeInSession(intent, session, callback);
    } else if ("WhatsMyStatusIntent" === intentName) {
        getStatusFromSession(panelStatus, intent, session, callback);
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
             buildSpeechletResponse(cardTitle, speechOutput, repromptText, shouldEndSession));
}

function getWelcomeResponse(callback) {
    // If we wanted to initialize the session to have some attributes we could add those here.
    var sessionAttributes = {};
    var cardTitle = "Welcome";
    var speechOutput = "Please ask the security system its status, or give it a command," +
                       "valid commands are the names of a keypad button," +
                       "or a 4 digit code";
    // If the user either does not reply to the welcome message, they will be prompted again.
    var repromptText = "To get the system's status, say status," +
                       "to activate a keypad button, say the name of a button from 0 to 9," +
                       "stay, away, pound, or star, for example, to activate pound, say pound, "+
                       "to send a 4 digit code, say the code, for example 1 2 3 4,";
    var shouldEndSession = false;

    callback(sessionAttributes,
             buildSpeechletResponse(cardTitle, speechOutput, repromptText, shouldEndSession));
}

/*
 * Gets the panel keypress from the user in the session.
 * Check to make sure keypress is valid.
 * Prepares the speech to reply to the user.
 * If valid, sends the keypress to the panel over a TLS TCP socket.
 */
function sendKeyInSession(panelStatus, intent, session, callback) {
    var cardTitle = intent.name;
    var KeysSlot = intent.slots.Keys;
    var sessionAttributes = {};
    var repromptText = "";
    var shouldEndSession = true; // end session after sending keypress
    var speechOutput = "";

    var ValidValues = ['0','1', '2', '3', '4', '5', '6', '7', '8', '9', 'stay', 'away', 'star', 'pound'];
    var num = KeysSlot.value;
    var isValidValue = ValidValues.indexOf(num) > -1; // true if a valid value was passed
    
    if (KeysSlot) {
        if (!isValidValue) {
            speechOutput = num + ",is an invalid command," +
                                 "valid commands are the names of a keypad button," +
                                 "status, or a 4 digit code";
            callback(sessionAttributes,
                     buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
        } else {
            if ((num === 'stay' || num === 'away') && isArmed(panelStatus)) {
                speechOutput = "System is already armed,";
                callback(sessionAttributes,
                         buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
            } else if ((num === 'stay' || num === 'away') && !zonesNotActive(panelStatus)) {
                var placesNotReady = findPlacesNotReady(panelStatus); // get friendly names of zones not ready
                speechOutput = "System cannot be armed, because these zones are not ready," +placesNotReady;
                callback(sessionAttributes,
                         buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
            } else {
                var socket = tls.connect(socketOptions, function() {
                    console.log('sendKeyInSession socket connected to host: ' +HOST);
                    socket.write(num +'\n');
                    console.log('wrote ' +num);
                });

                socket.on('data', function(data) {
                    var dummy; // read status from server to get FIN packet
                    dummy += data.toString();
                });

                socket.on('close', function() { // wait for FIN packet from server
                    console.log('sendKeyInSession socket disconnected from host: ' +HOST);
                    if (!(num === 'stay' || num === 'away')) { // a key that doesn't need verification
                        speechOutput = 'sent,' +num;
                        callback(sessionAttributes,
                                 buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
                    } else {
                        setTimeout(function verifyArmCmd() { // verify stay or away arm command succeeded
                            getPanelStatus(function checkIfArmed(panelStatus) {
                                if (isArmed(panelStatus)) {
                                    speechOutput = 'sent,' +num +',system was armed,';
                                } else {
                                    speechOutput = 'sent,' +num +',error,, system could not be armed,';
                                }
                                callback(sessionAttributes,
                                         buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
                            });
                        }, 1000); // wait 1 sec for command to take effect
                    }
                });

                socket.on('error', function(ex) {
                    console.log("sendKeyInSession handled error");
                    console.log(ex);
                });
            }
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
                buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
        }
        else {
            var socket = tls.connect(socketOptions, function() {
                console.log('sendCodeInSession socket connected to host: ' +HOST);
                socket.write(num +'\n');
                console.log('wrote ' +num);
            });

            socket.on('data', function(data) {
                var dummy;
                dummy += data.toString();
            });
	
            socket.on('close', function () {
	        console.log('sendCodeInSession socket disconnected from host: ' +HOST);
                speechOutput = "sending, " +num;
	        callback(sessionAttributes,
                         buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
            });

            socket.on('error', function(ex) {
                console.log("sendCodeInSession handled error");
                console.log(ex);
            });
        }
    } else {
        console.log('error in SendCodeInSession');
    }
}

/*
 * Sends panel status to the user and ends session.
 */
function getStatusFromSession(panelStatus, intent, session, callback) {
    var cardTitle = intent.name,
        repromptText = "",
        sessionAttributes = {},
        shouldEndSession = true,
        speechOutput = "";

    if (isArmed(panelStatus)) {
        isBypassed(panelStatus) ? speechOutput = 'system is armed and bypassed' : speechOutput = 'system is armed';
    } else if (zonesNotActive(panelStatus)) { // no zones are reporting activity or are tripped
        hasError(panelStatus) ? speechOutput = 'system is ready but has an error' : speechOutput = 'system is ready';
    } else { // system must not be ready
        var placesNotReady = findPlacesNotReady(panelStatus); // get friendly names of zones not ready;
        speechOutput = 'these zones are not ready,' +placesNotReady;
    }

    callback(sessionAttributes,
             buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
}

// --------------- global variables and commonly used functions -----------------------

var tls = require('tls'),
    fs = require('fs'),
    PORT = fs.readFileSync('port.txt').toString("utf-8", 0, 5),
    HOST = fs.readFileSync('host.txt').toString("utf-8", 0, 14),
    CERT = fs.readFileSync('client.crt'),
    KEY  = fs.readFileSync('client.key'),
    CA   = fs.readFileSync('ca.crt');

var socketOptions = {
    host: HOST,
    port: PORT,
    cert: CERT,
    key: KEY,
    ca: CA,
    rejectUnauthorized: true
};

/*
 * Gets the panel status to be used in the intent handlers.
 */
function getPanelStatus (callback) {
    var panelStatus = "";
    var serverCmd = 'idle'; // send idle to server which is a noop

    var socket = tls.connect(socketOptions, function() {
        console.log('getPanelStatus socket connected to host: ' +HOST);
        socket.write(serverCmd +'\n');
    });

    socket.on('data', function(data) {
        panelStatus += data.toString();
    });
	
    socket.on('close', function () {
	console.log('getPanelStatus socket disconnected from host: ' +HOST);
	callback(panelStatus);
    });
	
    socket.on('error', function(ex) {
	console.log("handled getPanelStatus socket error");
	console.log(ex);
    });
}

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
 */
function findPlacesNotReady(panelStatus) {
    var zoneRegex = /(Zone\d \d{1,2})((, \d{1,2}){1,8})?/g;
    var zonesNotReady = panelStatus.match(zoneRegex); // array with zones not ready
    var placesNotReady = "";
    for(i = 0; i < zonesNotReady.length; i++) {
        var zoneNotReady = zonesNotReady[i].slice(0,5);
        var sensorRegex = /\d{1,2}/g;
        var sensorsNotReady = zonesNotReady[i].slice(6).match(sensorRegex); // array with sensors in zone not ready
        for(j = 0; j < sensorsNotReady.length; j++) {
            var sensorNotReady = sensorsNotReady[j];
	    placesNotReady += zoneToPlace(zoneNotReady, sensorNotReady) +',';
        }
    }
    return placesNotReady;
}

function createNumberAttributes(key) {
    return {
        key : key
    };
}

// --------------- Helpers that build all of the responses -----------------------

function buildSpeechletResponse(title, output, repromptText, shouldEndSession) {
    return {
        outputSpeech: {
            type: "PlainText",
            text: output
        },
        card: {
            type: "Simple",
            title: "SessionSpeechlet - " + title,
            content: "SessionSpeechlet - " + output
        },
        reprompt: {
            outputSpeech: {
                type: "PlainText",
                text: repromptText
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
