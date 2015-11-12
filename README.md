mqttClient for Legato
============

This [Legato](http://www.legato.io/) Application enables you to easily send data to or receive data from Sierra Wireless AirVantage server, over MQTT protocol.

This application has been developed on a Linux PC (Ubuntu) and tested on Sierra Wireless WP8548 module running on MangOH board.

Setup Dev Host PC
-----------------
Makefile is provided for Command-Line building mode, please refer to [Setting up Command-Line mode dev host](http://www.legato.io/legato-docs/15_08/getstarted_c_linstall_main.html)


Build the application
---------------------
~~~
make wp85
~~~


Install the app on target (MangOH/WP8548)
-----------------------------------------
~~~
instapp mqttClient.wp85 <IP address of target>
~~~


Start the app on target (MangOH/WP8548)
-----------------------------------------
ssh to target then launch the app as follow:
~~~
app start mqttClient
~~~


Configure mqttClient
--------------------
~~~
to display options:
	execInApp mqttClient config -h

execInApp mqttClient config -b <mqtt broker url> -p <port number> -k <keep alive (seconds)> -q <QoS (0,1 or 2)>

to display current setting in the log (use logread -f in a separate console to visualize live output log):
	execInApp mqttClient config
~~~


Start MQTT session
------------------
~~~
to display options
	execInApp mqttClient connect -h

execInApp mqttClient connect -u <username> -p <password>
~~~
use logread -f in a separate console to visualize live output log, a successful connection is indicated by the following log:
- "MQTT connection status= 0"
- "MQTT connected..."


Send MQTT message
-----------------
~~~
execInApp mqttClient send <dataKey> <dataValue>
	e.g. execInApp mqttClient send time 9:37
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
