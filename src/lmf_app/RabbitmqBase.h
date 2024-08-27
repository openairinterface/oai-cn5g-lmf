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

#ifndef RABBITMQ_BASE_H
#define RABBITMQ_BASE_H

#include <fstream>
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>
#include <SimpleAmqpClient/SimpleAmqpClient.h>

using namespace std;
using namespace AmqpClient;
using json = nlohmann::json;


class RabbitmqBase {
  private:
    string host;
    int port;
    bool useSSL;
    string username;
    string password;
    string vhost;
    int frame_max;

    Channel::OpenOpts  opts;
    Channel::ptr_t  channel;
    Envelope::ptr_t  envelope;

    string exchange;
    string queue;
    string consumer_tag;

  public:
    RabbitmqBase();
    ~RabbitmqBase();
    void loadConfiguration();
    void openConnection();
    void closeConnection();
    void setExchange(string exchange);
    void startConsumer();
    void startConsumer(string exchange);
    bool getMessage(string& msg_body);
    bool sendMessage(string& msg_body);

    //static string serializeTOAs(vector<float> toas);

    static string serializeTOAs(vector<float> toas) {
        int num_toas = toas.size();
        vector<string> toas_str (num_toas);
        for (int i=0; i<num_toas; i++) {
            toas_str[i] = to_string((long) toas[i]);
        }

        json data;
        data["toas"] = toas_str;

        string result = data.dump();
        std::cout << result << std::endl;
        return result;
    }
};

#endif
