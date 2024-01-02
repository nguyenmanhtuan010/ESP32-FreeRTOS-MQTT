const express = require('express');
const app = express();

var server = require("http").Server(app);
var io = require("socket.io")(server);
server.listen(3000);

app.use('/static', express.static(__dirname + "/static"));
app.get('/',function(req, res){
    res.sendfile(__dirname + '/static/main.html');
});
var mqtt = require('mqtt');
var Topic1 = 'temp';
var Topic2 = 'humi';
var Topic3 = 'sw';
var Topic4 = 'sw_state';
var Broker_URL = 'mqtt://192.168.137.207';
var Database_URL = '192.168.137.207';

var options = {
    clientId: 'MyMQTT',
    port: 1883,
    keepalive: 60
};

var client = mqtt.connect(Broker_URL, options);
client.on('connect', mqtt_connect); 
client.on('reconnect', mqtt_reconnect);
client.on('error', mqtt_error);
client.on('message', mqtt_messsageReceived);
client.on('close', mqtt_close);

function mqtt_connect() {
    //console.log("Connecting MQTT"); 
    client.subscribe(Topic1, mqtt_subscribe);
    client.subscribe(Topic2, mqtt_subscribe);
    client.subscribe(Topic3, mqtt_subscribe);
    client.subscribe(Topic4, mqtt_subscribe);
}

function mqtt_subscribe(err, granted) {
    console.log("Subscribed to " + Topic1);
    console.log("Subscribed to " + Topic2);
    console.log("Subscribed to " + Topic3);
    console.log("Subscribed to " + Topic4);
    if (err) { console.log(err); }
}

function mqtt_reconnect(err) {
    //console.log("Reconnect MQTT");
    //if (err) {console.log(err);}
    client = mqtt.connect(Broker_URL, options);
}

function mqtt_error(err) {
    //console.log("Error!");
    //if (err) {console.log(err);}
}

function after_publish() {
    //do nothing
}

function mqtt_messsageReceived(topic, message, packet) {
    //console.log('Message received = ' + message);
    insert_message(topic, message, packet);
}


function mqtt_close() {
    //console.log("Close MQTT");
}

function insert_message(topic, message, packet) {

    io.sockets.emit(topic, message.toString())
};
io.on("connection", function(socket) {
    console.log("co nguoi ket noi:" + socket.id);
    socket.on("disconnect", function() {
        console.log(socket.id + "disconnect");
    });

    socket.on("ON1", function(data) {
        client.publish(Topic3, data);
    })
    socket.on("OFF1", function(data) {
        client.publish(Topic3, data);
    })
});