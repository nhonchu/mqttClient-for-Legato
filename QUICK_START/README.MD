MQTT Quick Start with command line (without coding)
==================================

This is a quick start guide to connect to various MQTT brokers, using the command line interface, without coding.

Refer to [this page](https://github.com/nhonchu/mqttClient-for-Legato#building-and-installing-applications) to install __mqttClient__ and __mqttCLI__ applications onto your target module. __mqttGoogleCLI__ applications is required to get started with Google Cloud Platform

Refer to [this page](https://github.com/nhonchu/mqttClient-for-Legato#accessing-command-line-interface-cli) to configure the access to Command Line interfaces.


AirVantage
--------------

MQTT default configuration is set to AirVantage server, below commands are usually not necessary when mqttCLI is restarted, except for setting the password.

- Configure MQTT broker endpoint as follow :

    ~~~
    mqtt config set broker eu.airvantage.net
    mqtt config set port 8883
    mqtt config set clientId <serial_number>
    mqtt config set password <your_password>
    ~~~


- Start the mqtt session with AirVantage, it is recommended to view the log in a different terminal using "logread -f"

    ~~~
    mqtt session start
    ~~~

- __mqttCLI__ automatically subscribe to topic "clientId/tasks/json"

- Publish messages (key-value) to topic "clientId/messages/json"

    ~~~
    mqtt avsend <key> <value> 
    ~~~


Eclipse Paho
--------------

- Configure MQTT broker endpoint as follow :

    ~~~
    mqtt config set broker iot.eclipse.org
    mqtt config set port 8883
    mqtt config set clientid <your_clientdId>
    mqtt config set password <your_password>
    ~~~


- Start the mqtt session with Eclipse Paho, it is recommended to view the log in a different terminal using "logread -f"

    ~~~
    mqtt session start
    ~~~


- Subscribe to topic or Publish messages

    ~~~
    mqtt sub <topicName>

    mqtt pub <topicName> <data>
    mqtt pubfile <topicName> <filename>
    ~~~


Amazon AWS-IoT
--------------

- Copy (scp) the certificates issued by Amazon to your target, and configure mqttClient's TLS as following example :

    ~~~
    mqtt tls set rootca /home/root/aws-iot/root-ca.pem
    mqtt tls set certificate /home/root/aws-iot/cert.pem.crt
    mqtt tls set privatekey /home/root/aws-iot/private.pem.key
    ~~~

    the command line tool will echo your setting, for instance:
    ~~~
    root@swi-mdm9x28-wp:~# mqtt tls set privatekey /home/root/aws-iot/private.pem.key 
    Current TLS setting is :
    rootCA : /home/root/aws-iot/root-ca.pem
    Certificate : /home/root/aws-iot/cert.pem.crt
    PrivateKey: /home/root/aws-iot/private.pem.key
    ~~~


- Set the MQTT broker endpoint as follow :

    ~~~
    mqtt config set broker <xyz>.iot.<us-east-2>.amazonaws.com
    mqtt config set port 8883
    mqtt config set clientid <xyz>
    ~~~


- Start the mqtt session with AWS-IoT, it is recommended to view the log in a different terminal using "logread -f"

    ~~~
    mqtt session start
    ~~~


- Subscribe to topic or Publish messages

    ~~~
    mqtt sub <topicName>

    mqtt pub <topicName> <data>
    mqtt pubfile <topicName> <filename>
    ~~~


Orange Live Objects
-------------------

- Configure MQTT broker endpoint as follow :

    ~~~
    mqtt config set broker liveobjects.orange-business.com
    mqtt config set port 8883
    mqtt config set username json+device
    mqtt config set clientid urn:lo:nsid:starterkit:<IMEI>
    mqtt config set password <apiKey>
    ~~~


- Start the mqtt session with LiveObjects, it is recommended to view the log in a different terminal using "logread -f"

    ~~~
    mqtt session start
    ~~~

- Subscribe to topic or Publish messages (refer to LiveObjects documentation for topics)

    ~~~
    mqtt sub dev/cfg/upd
    mqtt sub dev/cmd
    mqtt sub dev/rsc/upd

    mqtt pub dev/data <jsonData>
    mqtt pubfile dev/data/raw/none <filename>
    ~~~


Google Cloud Platform
---------------------

__mqttClient__ & __mqttGoogleCLI__  components should be installed on your target module.

- Configure GCP parameters as follow :

    ~~~
    gmqtt config set projectId <gcp_projectid>
    gmqtt config set location europe-west1
    gmqtt config set registry <gcp_registry>
    gmqtt config set deviceId <gcp_deviceid>
    gmqtt config set rsaPrivateKey <privateKeyFile>
    ~~~


- Start the mqtt session with GCP, it is recommended to view the log in a different terminal using "logread -f"

    ~~~
    gmqtt session start
    ~~~


- __mqttGoogleCLI__ automatically subscribe to topic "/devices/gcp_deviceId/config"

- Publish messages, the following command will publish data to topic "/devices/gcp_deviceId/state"

    ~~~
    gmqtt pub <data>
    ~~~

