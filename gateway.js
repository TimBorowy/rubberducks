const express = require('express');
const app = express();
const fs = require('fs');
const http = require('http').Server(app);
const io = require('socket.io')(http);
const bodyParser = require('body-parser');

const axios = require("axios");
const serverPort = 1337;

const events = [];
const ducks = [
    {
        device_id: "duck_tape",
        ip_address: "192.168.1.35",
        state: 0,
        person: "Tim Borowy"
    },
    {
        device_id: "duck_two",
        ip_address: "192.168.1.36",
        state: 0,
        person: "Peter Parker"
    }
];

app.use(bodyParser.json());       // to support JSON-encoded bodies
app.use(bodyParser.urlencoded({     // to support URL-encoded bodies
    extended: true
}));

app.use(express.static('public'))

app.post('/log_action', function (req, res) {
    // Event Request
    console.log(req.body);

    let data = {
        device_id: req.body.device_id,
        shake: req.body.shake,
        signal: req.body.signal,
        light_state: req.body.light_state,
        time: Date.now(),
    };
    // Emit event to ws client
    io.emit('update_log_list', data);

    // Finish request
    res.writeHead(200);
    res.end();

    // Log event data
    events.push(data);
    logEvent(data);

    // Filter event data for current device_id and shake
    const filteredData = events.filter(d => d.device_id === data.device_id && d.shake == true);
    // Sort by timestamp desc
    const sortedData = filteredData.sort((a, b) => b.time - a.time)
    // calc time between begining and and of latest 3 shakes
    if (sortedData.length > 3) {
        const timeBetween = sortedData[0].time - sortedData[2].time
        console.log("timeBetween: ", timeBetween)

        // When 3 shakes occur in 1 minute raise mental state
        if (timeBetween <= 60 * 1000) {
            let index = ducks.findIndex((duck) => duck.device_id === data.device_id)
            // Never go higher than level 4
            if (ducks[index].state < 4) {
                ducks[index].state++
            }

            console.log("3 shakes detected!! Raising state");
            io.emit('mental_state', ducks);
        }
    }


    toggleLight(data.device_id);
    // Deactivate after 3 seconds
    setTimeout(() => { toggleLight(data.device_id) }, 3000);
});

app.get('/', function (req, res) {
    res.sendFile(__dirname + '/public/index.html');
});

app.get('/data', function (req, res) {
    res.sendFile(__dirname + '/public/data.html');
});


io.on('connection', function (socket) {

    socket.emit('log_list', events);
    socket.emit('mental_state', ducks);

    socket.on('toggleLight', (data) => {
        toggleLight(data.device_id);
    })

    socket.on('disconnect', function () {
        console.log('user disconnected');
    });
});


http.listen(serverPort, function () {
    console.log(`Gateway running on port ${serverPort}!`)
    console.log(`http://localhost:${serverPort}/`)
});

// Every 60 sec
setInterval(resetState, 1000 * 60)

// Resets emotion state after some time
function resetState() {
    let changed = false

    for (let duck of ducks) {
        const filteredData = events.filter(d => d.device_id === duck.device_id && d.shake == true);
        if(filteredData.length != 0){
            // Sort by timestamp desc
            const sortedData = filteredData.sort((a, b) => b.time - a.time)

            // Last detection was more that a minute ago
            if (Date.now() - sortedData[0].time > 1000 * 60 && duck.state != 0) {
                duck.state--
                changed = true
            }
        } 
    }
    if(changed){
        io.emit('mental_state', ducks);
    }
}

function logEvent(data) {
    fs.appendFile('events.log', "date: " + new Date().toISOString().replace(/T/, ' ').replace(/\..+/, '') + JSON.stringify(data), function (err) {
        if (err) {
            console.log(err);
        }
    });
}


function toggleLight(device_id) {
    console.log("toggle event device_id: ", device_id)

    // Match device_id to duck to find ip adress
    let duck = ducks.find(d => d.device_id === device_id)

    if (duck) {
        axios.get(`http://${duck.ip_address}/toggle_light`)
            .then((res) => {
                let data = {
                    device_id: res.data.device_id,
                    shake: res.data.shake,
                    signal: res.data.signal,
                    light_state: res.data.light_state,
                    time: Date.now(),
                };

                events.push(data);

                io.emit('update_log_list', data);
            })
            .catch((err) => { console.log(err) })

    } else {
        return false
    }
}