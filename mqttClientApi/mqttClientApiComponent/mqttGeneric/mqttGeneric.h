/*******************************************************************************************************************
 
 MQTT interface

	This layer encapsules the MQTT Paho client and exposes generic mqtt functions

	Communication with MQTT broker can performed with or without secured transport : TLS


	View of the stack :
	_________________________
	
	 mqttSampleAirVantage.c
	_________________________

	 mqttAirVantage interface  
	_________________________

	 mqttGeneric interface  <--- this file
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

#ifndef _MQTT_GENERIC_H_
#define _MQTT_GENERIC_H_

#include "MQTTClient.h"

#define MQTT_BROKER		"MqttBrokerUrl"
#define	MQTT_PORT		"MqttBrokerPort"
#define MQTT_USE_TLS	"MqttUseTLS"
#define MQTT_ENDPOINT	"MqttEndPointName"
#define MQTT_SECRET		"MqttSecret"
#define MQTT_KEEPALIVE	"MqttKeepAlive"
#define MQTT_QOS		"MqttQoS"

#define 	MAX_OUTBOUND_PAYLOAD_SIZE		1024	//Default payload buffer size
#define 	MAX_INBOUND_PAYLOAD_SIZE		1024	//Default payload buffer size

#define		SIZE_DEVICE_ID					256

typedef struct 
{
    char    serverUrl[256];
    int     serverPort;
    int     useTLS;
    char    tlsRootCA[128];
	char    tlsCertificate[128];
	char    tlsPrivateKey[128];

    char    deviceId[SIZE_DEVICE_ID];
    char    username[128];
    char    secret[512];
    int     keepAlive;
    int     qoS;
} mqtt_config_t;

#define MAX_USER_DATA       3

typedef struct {
	mqtt_config_t			mqttConfig;

	Network 				network;
	Client 					mqttClient;
	MQTTPacket_connectData	data;
	unsigned char			mqttBuffer[MAX_OUTBOUND_PAYLOAD_SIZE];
	unsigned char			mqttReadBuffer[MAX_INBOUND_PAYLOAD_SIZE];
	void*					userCtxData[MAX_USER_DATA];
} mqtt_instance_st;

typedef void (*incomingMessageHandler)(const char* topic, const char* key, const char* value, const char* timestamp, void* pUserContext);
typedef void (*softwareInstallRequestHandler)(const char* uid, const char* type, const char* revision, const char* url, const char* timestamp, void * pUserContext);

typedef struct {
	incomingMessageHandler			pfnUserCommandHandler;
	void*							pUserCommandContext;
	softwareInstallRequestHandler	pfnUserSWInstallHandler;
	void*							pUserSWInstallContext;
} mqtt_ctxData_t;

void mqtt_GetDefaultConfig(mqtt_config_t* mqttConfig);

mqtt_instance_st * mqtt_CreateInstance(mqtt_config_t* mqttConfig);
void mqtt_SetTls(mqtt_instance_st* mqttObject, const char* rootCAFile, const char* certificateFile, const char * privateKeyFile);
void mqtt_SetUserData(mqtt_instance_st* mqttObject, void * userCtxData, int index);
void* mqtt_GetUserData(mqtt_instance_st* mqttObject, int index);
mqtt_instance_st* mqtt_DeleteInstance(mqtt_instance_st* mqttObject);
void mqtt_GetConfig(mqtt_instance_st * mqttObject, mqtt_config_t* mqttConfig);

int mqtt_StartSession(mqtt_instance_st * mqttObject);
int mqtt_StopSession(mqtt_instance_st * mqttObject);
int mqtt_IsConnected(mqtt_instance_st * mqttObject);

int mqtt_SubscribeTopic(mqtt_instance_st * mqttObject, const char* topicName);
int mqtt_UnsubscribeTopic(mqtt_instance_st * mqttObject, const char* topicName);

int mqtt_ProcessEvent(mqtt_instance_st * mqttObject, unsigned waitDelayMs);

int  mqtt_PublishKeyValue(mqtt_instance_st * mqttObject, const char* szKey, const char* szValue, const char* topicName);
int  mqtt_PublishData(mqtt_instance_st * mqttObject, const char* data, size_t dataLen, const char* topicName);

void mqtt_SetCommandHandler(mqtt_instance_st * mqttObject, incomingMessageHandler pHandler, void * pUserContext);
void mqtt_SetSoftwareInstallRequestHandler(mqtt_instance_st * mqttObject, softwareInstallRequestHandler pHandler, void * pUserContext);

#endif	//_MQTT_GENERIC_H_
