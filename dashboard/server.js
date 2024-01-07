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
var Topic3 = 'sw1';
var Topic4 = 'sw2';
var Topic5 = 'sw1_state';
var Topic6 = 'sw2_state';
var Broker_URL = 'mqtt://your ip';
var Database_URL = 'your ip';

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
    client.subscribe(Topic5, mqtt_subscribe);
    client.subscribe(Topic6, mqtt_subscribe);
}

function mqtt_subscribe(err, granted) {
    console.log("Subscribed to " + Topic1);
    console.log("Subscribed to " + Topic2);
    console.log("Subscribed to " + Topic3);
    console.log("Subscribed to " + Topic4);
    console.log("Subscribed to " + Topic5);
    console.log("Subscribed to " + Topic6);
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

var mysql = require('mysql');

//Create Connection
var connection = mysql.createConnection({
    host: Database_URL,
    user: "newuser",
    password: "mypassword",
    database: "mydb"
});

connection.connect(function(err) {
    if (err) throw err;
    //console.log("Database Connected!");
});
var TEMP = 0;
var HUMI = 0;
function insert_message(topic, message, packet) {
    // var message_arr = extract_string(message_str); //split a string into an array

    if (topic == Topic1) {
        TEMP = message.toString();
    }
    if (topic == Topic2) {
        HUMI = message.toString();
    }
    console.log(TEMP);
    console.log(HUMI);
    var sql = "INSERT INTO ?? (??,??) VALUES (?,?)";
    var params = ['dlcb', 'TEMP', 'HUMI', TEMP, HUMI];
    sql = mysql.format(sql, params);

    connection.query(sql, function(error, results) {
        if (error) throw error;
        //console.log(message.toString());
    });
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

    socket.on("ON2", function(data) {
        client.publish(Topic4, data);
    })
    socket.on("OFF2", function(data) {
        client.publish(Topic4, data);
    })
});