
const express = require('express');
const app = express();
const fs = require('fs');

const http = require('http').Server(app);
const io = require('socket.io')(http);
const bodyParser = require('body-parser');

const axios = require("axios");


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

    socket.on('toggleLight', (stuff)=>{
        console.log('send req for ')
        console.log(stuff)

        axios.get("http://192.168.1.35/toggle_light")
        .then((res)=> {
            console.log(res.data)

            let data = {
                time: Date.now(),
                shake: res.data.shake,
                signal: res.data.signal,
                device_id: res.data.device_id,
                light_state: res.data.light_state
            };
        
            events.push(data);
        
            io.emit('update_log_list', data);


        })
    })

    socket.on('disconnect', function () {

        console.log('user disconnected');
    });



});


http.listen(1337, function () {
    console.log('place log app running on port 1337!')
});