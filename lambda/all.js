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
 * Lindo St. Angel 2015.
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

    // Dispatch to your skill's launch.
    getWelcomeResponse(callback);
}

/**
 * Called when the user specifies an intent for this skill.
 */
function onIntent(intentRequest, session, callback) {
    console.log("onIntent requestId=" + intentRequest.requestId +
            ", sessionId=" + session.sessionId);

    var intent = intentRequest.intent,
        intentName = intentRequest.intent.name;

    // Dispatch to your skill's intent handlers
    if ("MyNumIsIntent" === intentName) {
        sendKeyInSession(intent, session, callback);
    } else if ("WhatsMyStatusIntent" === intentName) {
        getStatusFromSession(intent, session, callback);
    } else if ("AMAZON.HelpIntent" === intentName) {
        getWelcomeResponse(callback);
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

function getWelcomeResponse(callback) {
    // If we wanted to initialize the session to have some attributes we could add those here.
    var sessionAttributes = {};
    var cardTitle = "Welcome";
    var speechOutput = "Hi, the security system is ready for input,";
    // If the user either does not reply to the welcome message or says something that is not
    // understood, they will be prompted again with this text.
    var repromptText = "Please tell the security system a command, or ask its status," +
                       "Valid commands are the names of any keypad button," +
                       "After a status update, the session will end";
    var shouldEndSession = false;

    callback(sessionAttributes,
             buildSpeechletResponse(cardTitle, speechOutput, repromptText, shouldEndSession));
}

/*
 * Gets the panel keypress from the user in the session.
 * Prepares the speech to reply to the user.
 * Sends the keypress to the panel over a TLS TCP socket.
 */
function sendKeyInSession(intent, session, callback) {
    var cardTitle = intent.name;
    var KeysSlot = intent.slots.Keys;
    var repromptText = "";
    var sessionAttributes = {};
    var shouldEndSession = false;
    var speechOutput = "";
    var tls = require('tls');
    var PORT = 4746;
    var HOST = '98.207.176.132'; // todo: use FQDN instead of IP
    
    var options = {
        host: HOST,
        port: PORT,
        rejectUnauthorized: false, // danger - MITM attack possible - todo: fix
        key:"", // add for client-side auth - todo: add
        cert:"",
        ca:""
    };
    
    if (KeysSlot) {
        var num = KeysSlot.value;
        sessionAttributes = createNumberAttributes(num);
        speechOutput = "sending, " +num;
        repromptText = "Please tell the security system a command, or ask its status," +
                       "Valid commands are the names of any keypad button," +
                       "After a status update, the session will end";
    } else {
        speechOutput = "";
        repromptText = "";
    }
    
    var socket = tls.connect(options, function() {
        if(socket.authorized){
          console.log('authorized');
        }
        else{
          console.log('cert auth error: ', socket.authorizationError);
        }
        socket.write(num +'\n');
        console.log('wrote ' +num);
        socket.end;
        console.log('disconnected from host ' +HOST);
        callback(sessionAttributes,
             buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
    });
       
    socket.on('error', function(ex) {
        console.log("handled error");
        console.log(ex);
    });

}

function createNumberAttributes(key) {
    return {
        key : key
    };
}

/**
 * Gets the panel status via a TLS TCP socket.
 * Sends the resulting speech to the user and ends session.
 * 
 * todo: resolve bug that results in any utterence triggering a status response.
 */
function getStatusFromSession(intent, session, callback) {
    var cardTitle = intent.name;
    // Setting repromptText to null signifies that we do not want to reprompt the user.
    // If the user does not respond or says something that is not understood, the session
    // will end.
    var repromptText = "";
    var sessionAttributes = {};
    var shouldEndSession = true;
    var speechOutput = "";
    var read = "";
    var tls = require('tls');
    var PORT = 4746;
    var HOST = '98.207.176.132'; // todo: use FQDN instead of IP
    
    var options = {
        host: HOST,
        port: PORT,
        rejectUnauthorized: false, // danger - MITM attack possible - todo: fix
        key:"", // add for client-side auth - todo: add
        cert:"",
        ca:""
    };
    
    var socket = tls.connect(options, function() {
        if(socket.authorized){
          console.log('authorized');
        }
        else{
          console.log('cert auth error: ', socket.authorizationError);
        }
        console.log('connected to host ' +HOST);
        socket.write('idle\n');
    });
       
    socket.on('data', function(data) {
        read += data.toString();
    });
       
    socket.on('end', function() {
        socket.end;
        console.log('disconnected from host ' +HOST);
        console.log('host data read: ' +read);
        speechOutput = read;
        callback(sessionAttributes,
             buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
    });
       
    socket.on('error', function(ex) {
        console.log("handled error");
        console.log(ex);
    });
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
