/*******************************************************************************************************************
 
 MQTT AirVantage interface

	This layer provides simple interface to :
		- Start a mqtt session with AirVantage
		- Publish message
		- Receive AirVantage commands along with command parameters
		- Receive Software/Firmware Installation (FOTA/SOTA) Request from AirVantage
		- ACKing the SW installation request

	Communication with AirVantage performed over MQTT prococol, with ot without secured transport : TLS

	Refer to mqttSampleAirVantage.c for how to use this interface

	View of the stack :
	_________________________
	
	 mqttSampleAirVantage.c
	_________________________

	 mqttAirVantage interface  <--- this file
	_________________________

	 mqttGeneric interface
	_________________________

	 paho
	_________________________

	 TLSinterface
	_________________________

	 Mbed TLS
	_________________________


	N. Chu
	June 2018

*******************************************************************************************************************/

#ifndef _MQTT_AV_INTERFACE_H_
#define _MQTT_AV_INTERFACE_H_

#include "mqttGeneric.h"




void mqtt_avGetDefaultConfig(mqtt_config_t* mqttConfig);

int mqtt_avIsAirVantageBroker(mqtt_instance_st * mqttObject);

void mqtt_avSetCommandHandler(mqtt_instance_st * mqttObject, incomingMessageHandler pHandler, void * pUserContext);
void mqtt_avSetSoftwareInstallRequestHandler(mqtt_instance_st * mqttObject, softwareInstallRequestHandler pHandler, void * pUserContext);

void mqtt_avSubscribeAirVantageTopic(mqtt_instance_st* mqttObject);
int mqtt_avPublishAck(mqtt_instance_st * mqttObject, const char* szUid, int nAck, const char* szMessage);
int mqtt_avPublishData(mqtt_instance_st * mqttObject, const char* szKey, const char* szValue);

/*
  you can use functions in mqttGeneric interface :
   mqtt_StartSession
   mqtt_StopSession
   mqtt_GetConfig
   mqtt_IsConnected
   etc
*/

#endif	//_MQTT_AV_INTERFACE_H_