/**
 * Wrapper class for handling RabbitMQ connections.
 *
 * 
 * Fraunhofer Institute for Integrated Circuits IIS
 * Nordostpark 84, 90411 Nuernberg, Germany
 * 
 * Copyright (C) 2023
 * All rights reserved
 * 
 * www:    https://www.iis.fraunhofer.de/
 * 
 * ...........................................................................
 * ... Author(s):  andreas.eidloth@iis.fraunhofer.de
 * ...........................................................................
 */


#include "RabbitmqBase.h"


RabbitmqBase::RabbitmqBase()
{
    host = "localhost";
    port = 5672;
    useSSL = false;
    username = "guest";
    password = "guest";
    vhost = "/";
    frame_max = 131072;

    exchange = "";
    queue = "";
    consumer_tag = "";
}

RabbitmqBase::~RabbitmqBase()
{
}


void RabbitmqBase::loadConfiguration() {
    /**
     * Load RabbitMQ settings file
     */

    bool isExternalConfig = false;
    json data;

    try {
        ifstream f("RabbitMQConfig.json");
        data = json::parse(f);
        isExternalConfig = true;
    } catch (...) {
        cout << "Can not load RabbitMQConfig from file. Using default settings." << endl;
    }
    
    if (isExternalConfig) {
        try {
            json rabbitMQ = data["rabbitMQ"];

            host = rabbitMQ["host"];
            port = rabbitMQ["port"];
            useSSL = rabbitMQ["useSSL"];
            username = rabbitMQ["username"];
            password = rabbitMQ["password"];
            vhost = rabbitMQ["virtualHost"];
        } catch (...) {
            cout << "Can't parse json configuration from RabbitMQConfig. Using default settings." << endl;
        }
    }

    /**
     * Generate RabbitMQ connection options
    */
    opts.host = host;
    opts.vhost = vhost;
    opts.port = port;
    opts.frame_max = frame_max;
    opts.auth = Channel::OpenOpts::BasicAuth(username, password);
    if (useSSL) {
        opts.tls_params = Channel::OpenOpts::TLSParams();
        opts.tls_params->verify_hostname = false;
        opts.tls_params->verify_peer = false;
    }
    cout << "Configuration loaded." << endl;
}

void RabbitmqBase::openConnection() {
    /**
     * Open RabbitMQ channel
    */
    cout << "Open connection" << endl;
    cout << "Host: " << opts.host << endl;
    channel = Channel::Open(opts);
    cout << "Connection is open." << endl;
    
}

void RabbitmqBase::closeConnection() {
    /**
     * Close RabbitMQ connection
    */
    cout << "Closing connection" << endl;
    channel = boost::shared_ptr<Channel>();

    // reset properties
    exchange = "";
    queue = "";
    consumer_tag = "";
    envelope = boost::shared_ptr<Envelope>();
}


void RabbitmqBase::setExchange(string exchangeName) {
    exchange = exchangeName;
}


void RabbitmqBase::startConsumer() {
    startConsumer("positions");
}

void RabbitmqBase::startConsumer(string exchangeName) {
    // Create queue
    queue = channel->DeclareQueue("");

    // Set exchange
    RabbitmqBase::setExchange(exchangeName);

    // Bind queue to "positions" exchange
    channel->BindQueue(queue, exchange, "");

    // Start consuming
    consumer_tag = channel->BasicConsume(queue, "");
}

bool RabbitmqBase::getMessage(string& msg_body) {
    bool hasReceived = channel->BasicConsumeMessage(consumer_tag, envelope, 100);
    
    if (hasReceived) {
        msg_body = envelope->Message()->Body();
    }

    return hasReceived;
}

bool RabbitmqBase::sendMessage(string& msg_body) {
    BasicMessage::ptr_t msg_out = BasicMessage::Create();
    msg_out->Body(msg_body);

    channel->BasicPublish(exchange, "", msg_out);

    return 1;
}
