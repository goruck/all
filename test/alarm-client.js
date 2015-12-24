//

var tls = require('tls');
var fs = require('fs');
var PORT = 4746;
var HOST = '192.168.1.14';
var num = "idle";

var options = {
        //cert: fs.readFileSync('certificate.crt'),
        //key: fs.readFileSync('privateKey.key'),
        host: HOST,
        port: PORT,
        rejectUnauthorized: false,
        //ca: [fs.readFileSync('rootCA.pem')],
        //checkServerIdentity: function (host, cert) { // doesnt work in node < 0.11
          //console.log(host, cert);
          //return true;
        //}
    };

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
    });

socket.on('data', function (data) {
   console.log(data.toString());
   socket.end;
});

socket.on('error', function(ex) {
        console.log("handled error");
        console.log(ex);
    });


