/*
 * Modules that are shared between all lambda functions.
 *
 * Copyright (c) 2016 Lindo St. Angel
 *
 */

/*
 * Gets the panel status to be used in the intent handlers.
 * This function is also used to send commands to the server.
 * serverCmd = 'sendJSON' returns status as JSON.
 * serverCmd = 'idle' returns status as text (legacy mode).
 *
 */
var tls = require('tls'),
    fs = require('fs'),
    PORT = fs.readFileSync('./port.txt').toString("utf-8", 0, 5),
    HOST = fs.readFileSync('./host.txt').toString("utf-8", 0, 14),
    CERT = fs.readFileSync('./client.crt'),
    KEY  = fs.readFileSync('./client.key'),
    CA   = fs.readFileSync('./ca.crt');

var socketOptions = {
    host: HOST,
    port: PORT,
    cert: CERT,
    key: KEY,
    ca: CA,
    rejectUnauthorized: true
};

var getPanelStatus = function (serverCmd, callback) {
    var panelStatus = "";

    var socket = tls.connect(socketOptions, function() {
        console.log('getPanelStatus socket connected to host: ' +HOST);
        socket.write(serverCmd +'\n');
        console.log('getPanelStatus wrote: '+serverCmd);
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
// Export function so it can be used external to this module.
module.exports.getPanelStatus = getPanelStatus;

/*
 * Calculate zone changes relative to last observation time.
 *
 */
var calcRelT = function (time, value) {
    var relT = [];
    for (i = 0; i < value.length; i++) {
        relT[i] = value[i] - time;
    }
    return relT;
}
// Export function so it can be used external to this module.
module.exports.calcRelT = calcRelT;

/*
 * Helper to build all of the Alexa responses.
 *
 */
var buildSpeechletResponse = function (title, output, repromptText, shouldEndSession) {
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
// Export function so it can be used external to this module.
module.exports.buildSpeechletResponse = buildSpeechletResponse;
