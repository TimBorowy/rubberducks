
const express = require('express');
const app = express();
const fs = require('fs');

const http = require('http').Server(app);
const io = require('socket.io')(http);
const bodyParser = require('body-parser');


const events = [];

app.use(bodyParser.json());       // to support JSON-encoded bodies
app.use(bodyParser.urlencoded({     // to support URL-encoded bodies
    extended: true
}));

app.post('/log_action', function (req, res) {
    // Request
    console.log(req.body);

    let data = {
        time: Date.now(),
        shake: req.body.shake,
        signal: req.body.signal,
        device_id: req.body.device_id,
        light_state: req.body.light_state
    };

    events.push(data);

    console.log(events);

    io.emit('update_log_list', data);

    fs.appendFile('placelog.txt', "date: " + new Date().toISOString().replace(/T/, ' ').replace(/\..+/, '') + JSON.stringify(data), function (err) {
        if (err) {
            console.log(err);
        }
    });


    res.writeHead(200);
    res.end();
});

app.get('/', function (req, res) {
    res.sendFile(__dirname + '/index.html');
});


io.on('connection', function (socket) {

    socket.emit('log_list', events);
    socket.broadcast.emit('log_list', events);

    socket.on('disconnect', function () {

        console.log('user disconnected');
    });

});


http.listen(1337, function () {
    console.log('place log app running on port 1337!')
});