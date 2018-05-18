mqttClient for Legato
============

This MQTT-client enables you to easily send data to or receive data from Sierra Wireless AirVantage server, over MQTT protocol.

Components and sample have been updated and tested with WP76 module running Legato18.04.

There are 3 components:
	- mqttMain : encapsulates MQTT-Client interacting with a MQTT Broker (default is AirVantage). It exposes key features to other Legato apps over IPC (mqtt.api)
	- mqttCmd  : this a MQTT Command Line Interface. You can easily connect to a broker, publish messages, or view received messages, using command line. This component is relying on mqttMain component.
	- mqttTest : In case you want to develop a Legato application to pushing messages over MQTT, you can start with this sample app. It makes use of mqttMain component (mqtt.api) to achieve your mqtt features.



Build the application
---------------------
~~~
make wp76xx

cd mqttTest
make wp76xx
~~~


Install the app on target (MangOH/WP76xx)
-----------------------------------------
~~~
app install mqttClient.wp76xx.update <IP address of target>

app install mqttTest.wp76xx.update <IP address of target>
~~~



MQTT Command Line Interface
-----------------------------------------
Open a Terminal and ssh to target then launch the app as follow:
~~~
app runProc mqttClient mqttMain
~~~


Open a Second Terminal and ssh to target then launch the app as follow:
~~~
app runProc mqttClient mqttCmd 
~~~
This will display the commandusage:
~~~
MQTT client command line usage:

    mqtt config get - to display current config
    mqtt config set broker/port/kalive/qos/username/password [newValue]

    mqtt session start  - start connection
    mqtt session stop   - close connection
    mqtt session status - get connection status

    mqtt send <dataPath> <value> - send data

    mqtt received - display incoming message

    mqtt quit
~~~


Configure mqttClient
--------------------
For example, execute the following command to set a password
~~~
app runProc mqttClient mqttCmd -- config set password sierra
~~~


Start MQTT session
------------------
~~~
app runProc mqttClient mqttCmd -- session start
~~~


Send MQTT message
-----------------
Wait for a successful MQTT connection (refer to previous section) before sending messages.
~~~
app runProc mqttClient mqttCmd -- send <dataKey> <dataValue>
	e.g. app runProc mqttClient mqttCmd -- send temperature 20.4
~~~


Receiving MQTT message from AirVantage
--------------------------------------
Incoming MQTT message should be visible in the log (logread -f)
for instance:
~~~
Jan  4 08:09:01 swi-mdm9x15 user.info Legato:  INFO | mqttClient[16755]/mqttClientComp T=main | mqttMain.c onIncomingMessage() 278 |  Incoming data: [{"uid":"59976b814d0b45d1b12371e66fc87d1f","timestamp":1445527665027,"command":{"id":"home.BedRoom1","params":{"Light":true,
Jan  4 08:09:01 swi-mdm9x15 user.info Legato:  INFO | mqttClient[16755]/mqttClientComp T=main | mqttMain.c onIncomingMessage() 305 | --> Incoming message from AirVantage: home.BedRoom1.Light = true @ 1445527665027
Jan  4 08:09:01 swi-mdm9x15 user.info Legato:  INFO | mqttClient[16755]/mqttClientComp T=main | mqttMain.c onIncomingMessage() 305 | --> Incoming message from AirVantage: home.BedRoom1.AirConditioner =  @ 1445527665027
Jan  4 08:09:01 swi-mdm9x15 user.info Legato:  INFO | mqttClient[16755]/mqttClientComp T=main | mqttMain.c onIncomingMessage() 305 | --> Incoming message from AirVantage: home.BedRoom1.Shutters = true @ 1445527665027
Jan  4 08:09:01 swi-mdm9x15 user.info Legato:  INFO | mqttClient[16755]/mqttClientComp T=main | mqttMain.c publishAckCmd() 235 | [ACK Message] [{"uid": "59976b814d0b45d1b12371e66fc87d1f", "status" : "OK"}]
Jan  4 08:09:01 swi-mdm9x15 user.info Legato:  INFO | mqttClient[16755]/mqttClientComp T=main | mqttMain.c publishAckCmd() 247 | Publish on 359377060002226/acks/json
~~~
However, you can use the following command line to check for incoming messages :
~~~
app runProc mqttClient mqttCmd -- received
~~~

Close MQTT session
-----------------
~~~
app runProc mqttClient mqttCmd -- session stop
	or
app runProc mqttClient mqttCmd -- quit
~~~


Using MQTT-Client within your Legato app
-----------------------------------------
The above sections are for Command Line Interface, if you need to develop a Legato application to send/receive MQTT message, then you can use the provided mqttTest sample as a starting point.
This sample app starts with registering for 2 events : MQTT connectivity state change and incoming messages.
Using these handlers, the app just sends a simple message to the broker upon mqtt connection being established. And when the app receives a message from the broker, it just quit the app to end the demo.
