const express = require('express');
const app = express();
const fs = require('fs');
const config = require('./config');
const http = require('http').Server(app);
const io = require('socket.io')(http);
const bodyParser = require('body-parser');
const mqtt = require('mqtt');
const mqttClient = mqtt.connect({
    host: config.mqtt.host, 
    port: config.mqtt.port, 
    username: config.mqtt.username, 
    password: config.mqtt.password
});
const serverPort = config.app.port;

const events = [];
const ducks = [
    {
        device_id: "Duck_tape",
        state: 0,
        person: "Tim Borowy"
    },
    {
        device_id: "Duck_tales",
        state: 0,
        person: "Peter Parker"
    }
];

app.use(bodyParser.json());       // to support JSON-encoded bodies
app.use(bodyParser.urlencoded({     // to support URL-encoded bodies
    extended: true
}));

app.use(express.static('public'));

mqttClient.on('connect', function () {
    mqttClient.subscribe('ducks/shake', function (err) {
        if (!err) {
            mqttClient.publish('presence', 'Hello mqtt');
        } else {
            console.error(err);
        }
    })
})

mqttClient.on('error', function (err) {
    console.error(err)
})

mqttClient.on('message', function (topic, message) {
    if (topic == "ducks/shake") {

        // message is Buffer
        mqttMsg = JSON.parse(message.toString());

        // Event Request
        console.log(mqttMsg);

        let data = {
            device_id: mqttMsg.device_id,
            shake: mqttMsg.shake,
            signal: mqttMsg.signal,
            light_state: mqttMsg.light_state,
            time: Date.now(),
        };
        // Emit event to ws client
        io.emit('update_log_list', data);

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


    }
    //mqttClient.end()
})

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
        if (filteredData.length != 0) {
            // Sort by timestamp desc
            const sortedData = filteredData.sort((a, b) => b.time - a.time)

            // Last detection was more that a minute ago
            if (Date.now() - sortedData[0].time > 1000 * 60 && duck.state != 0) {
                duck.state--
                changed = true
            }
        }
    }
    if (changed) {
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
    if(device_id){
        console.log("toggle event device_id: ", device_id)
        mqttClient.publish(`ducks/${device_id.toString()}/flash`, 'pls flash XD')
    } else{
        // publish to all connected ducks a flash command 
        mqttClient.publish('ducks/flash', 'pls flash XD')
    }
    // should ducks respond with a status change?

    // Match device_id to duck to find ip adress
    let duck = ducks.find(d => d.device_id === device_id)
}