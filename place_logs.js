
var express = require('express');
var app = express();
var fs = require('fs');

var http = require('http').Server(app);
var io = require('socket.io')(http);
var bodyParser = require('body-parser');


var place_action_log = [];

app.use( bodyParser.json() );       // to support JSON-encoded bodies
app.use(bodyParser.urlencoded({     // to support URL-encoded bodies
  extended: true
}));

app.post('/log_action', function(req, res) {
    // Request
    console.log(req.body);

    var bla = {
    	Username: req.body.Username,
    	X: req.body.X,
    	Y: req.body.Y,
    	NewColor: req.body.NewColor,
    	OldColor: req.body.OldColor,
    };

    place_action_log.push(bla);

    console.log(place_action_log);

    io.emit('update_log_list', bla);

    fs.appendFile('placelog.txt', "date: "+ new Date().toISOString().replace(/T/, ' ').replace(/\..+/, '')+JSON.stringify(bla), function (err) {
        if(err){
            console.log(err);
        }
    });


    res.writeHead(200);
    res.end();
});

app.get('/place_logs', function(req, res){
	res.sendFile(__dirname + '/place_index.html');
});


io.on('connection', function(socket){

	socket.emit('log_list', place_action_log);
    socket.broadcast.emit('log_list', place_action_log);

    socket.on('disconnect', function(){

        console.log('user disconnected');
    });

});


http.listen(1337, function () {
  console.log('place log app running on port 1337!')
});