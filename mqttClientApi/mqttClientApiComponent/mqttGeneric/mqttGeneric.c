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

#include <stdio.h>
#include <memory.h>

#include "mqttGeneric.h"

/*---------- Default parameters ---------------------------------*/
#define 	TIMEOUT_MS					5000	//time-out for MQTT connection
#define 	MQTT_VERSION				4		//3

#define		DEFAULT_BROKER				"iot.eclipse.org"
#define		DEFAULT_PORT				1883
#define		DEFAULT_KEEP_ALIVE			30
#define		DEFAULT_QOS					QOS0
#define		DEFAULT_USE_TLS				0
#define		DEFAULT_YIELD_TIMEOUT		1000	//allow 1 second for checking incoming packet & sending keep-alive
#define		DEFAULT_DEVICE_NAME			"mqttGeneric"
#define		DEFAULT_USER_NAME			"username"
#define		DEFAULT_SECRET				"noSecret"

#define		USER_DATA_INDEX				0

void mqtt_GetDefaultConfig(mqtt_config_t* mqttConfig)
{
	strcpy(mqttConfig->serverUrl, DEFAULT_BROKER);
    mqttConfig->serverPort = DEFAULT_PORT;
    mqttConfig->useTLS = DEFAULT_USE_TLS;
    strcpy(mqttConfig->deviceId, DEFAULT_DEVICE_NAME);
    strcpy(mqttConfig->username, DEFAULT_USER_NAME);
    strcpy(mqttConfig->secret, DEFAULT_SECRET);
    mqttConfig->keepAlive = DEFAULT_KEEP_ALIVE;
    mqttConfig->qoS = DEFAULT_QOS;
}

//-------------------------------------------------------------------------------------------------------
mqtt_instance_st * mqtt_CreateInstance(mqtt_config_t* mqttConfig)
{
	mqtt_instance_st * mqttObject = (mqtt_instance_st *) malloc(sizeof(mqtt_instance_st));

	memset(mqttObject, 0, sizeof(mqtt_instance_st));

	
	if (mqttConfig->serverPort <= 0)
	{
		mqttConfig->serverPort = DEFAULT_PORT;
	}

	if (mqttConfig->useTLS < 0)
	{
		mqttConfig->useTLS = DEFAULT_USE_TLS;
	}

	if (mqttConfig->keepAlive <= 0)
	{
		mqttConfig->keepAlive = DEFAULT_KEEP_ALIVE;	
	}

	if (mqttConfig->qoS <= 0 || mqttConfig->qoS > QOS2)
	{
		mqttConfig->qoS = DEFAULT_QOS;	
	}

	memcpy(&mqttObject->mqttConfig, mqttConfig, sizeof(mqtt_config_t));

	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;

	memcpy(&mqttObject->data, &data, sizeof(MQTTPacket_connectData));

	return mqttObject;
}

//-------------------------------------------------------------------------------------------------------
void mqtt_SetUserData(mqtt_instance_st* mqttObject, void * userCtxData, int index)
{
	if (index < 0 || index > MAX_USER_DATA)
	{
		return;
	}
	mqttObject->userCtxData[index] = userCtxData;
}

//-------------------------------------------------------------------------------------------------------
void* mqtt_GetUserData(mqtt_instance_st* mqttObject, int index)
{
	if (index < 0 || index > MAX_USER_DATA)
	{
		return NULL;
	}

	return mqttObject->userCtxData[index];
}

//-------------------------------------------------------------------------------------------------------
mqtt_instance_st* mqtt_DeleteInstance(mqtt_instance_st* mqttObject)
{
	if (mqttObject)
	{
		mqtt_StopSession(mqttObject);

		int i;
		for (i=0; i<MAX_USER_DATA; i++)
		{
			mqtt_ctxData_t * userData = mqtt_GetUserData(mqttObject, i);

			if (userData)
			{
				//fprintf(stdout, "mqtt_DeleteInstance : freeing userData %p", userData);
				//fflush(stdout);
				free(userData);
			}
		}
		//fprintf(stdout, "mqtt_DeleteInstance : freeing instance %p", mqttObject);
		//fflush(stdout);
		free(mqttObject);
	}

	return NULL;
}
//-------------------------------------------------------------------------------------------------------
int mqtt_PublishData(mqtt_instance_st * mqttObject, const char* data, size_t dataLen, const char* topicName)
{

	//printf("Sending Data: %s\n", data);

	MQTTMessage		msg;
	msg.qos = mqttObject->mqttConfig.qoS;
	msg.retained = 0;
	msg.dup = 0;
	msg.id = 0;
	msg.payload = (void *) data;
	msg.payloadlen = dataLen;

	fprintf(stdout, "Publishing data on %s : %s ... ", topicName, data);
	fflush(stdout);

	int rc = MQTTPublish(&mqttObject->mqttClient, topicName, &msg);
	if (rc != SUCCESS)
	{
		fprintf(stdout, "publish error: %d\n", rc);
		
	}
	else
	{
		fprintf(stdout, "OK\n");
	}
	fflush(stdout);

	return rc;
}

//-------------------------------------------------------------------------------------------------------
int  mqtt_PublishKeyValue(mqtt_instance_st * mqttObject, const char* szKey, const char* szValue, const char* topicName)
{
	char * message = malloc(strlen(szKey) + strlen(szValue) + 10);

	sprintf(message, "{\"%s\":\"%s\"}", szKey, szValue);	

	int rc = mqtt_PublishData(mqttObject, message, strlen(message), topicName);

	free(message);

	return rc;
}

//-------------------------------------------------------------------------------------------------------
void mqtt_GetConfig(mqtt_instance_st * mqttObject, mqtt_config_t* mqttConfig)
{

	memcpy(mqttConfig, &mqttObject->mqttConfig, sizeof(mqtt_config_t));
}

//-------------------------------------------------------------------------------------------------------
int mqtt_ProcessEvent(mqtt_instance_st * mqttObject, unsigned waitDelayMs)
{
	int timeout = DEFAULT_YIELD_TIMEOUT;

	if (waitDelayMs > 0)
	{
		timeout = (int) waitDelayMs;
	}

	return MQTTYield(&mqttObject->mqttClient, timeout);
}

//-------------------------------------------------------------------------------------------------------
int mqtt_StartSession(mqtt_instance_st * mqttObject)
{
	int 			rc = 0;
	
	int				nMaxRetry = 3;
	int				nRetry = 0;


	NewNetwork(&mqttObject->network, mqttObject->mqttConfig.useTLS);
	mqttObject->mqttClient.userCtxData = (void *) mqttObject;

	for (nRetry=0; nRetry<nMaxRetry; nRetry++)
	{
		fprintf(stdout, "mqtt_StartSession... connecting...");
		mqttObject->network.connect(&mqttObject->network, mqttObject->mqttConfig.serverUrl, mqttObject->mqttConfig.serverPort);

		MQTTClient(&mqttObject->mqttClient, &mqttObject->network, TIMEOUT_MS, mqttObject->mqttBuffer, sizeof(mqttObject->mqttBuffer), mqttObject->mqttReadBuffer, sizeof(mqttObject->mqttReadBuffer));
	 
		mqttObject->data.willFlag = 0;
		mqttObject->data.MQTTVersion = MQTT_VERSION;
		mqttObject->data.clientID.cstring = mqttObject->mqttConfig.deviceId;
		mqttObject->data.username.cstring = mqttObject->mqttConfig.username;
		mqttObject->data.password.cstring = mqttObject->mqttConfig.secret;

		mqttObject->data.keepAliveInterval = mqttObject->mqttConfig.keepAlive;
		mqttObject->data.cleansession = 1;

		fprintf(stdout, "  clientId : %s\n", mqttObject->mqttConfig.deviceId);
		fprintf(stdout, "  username : %s\n", mqttObject->mqttConfig.username);

		fprintf(stdout, "Attempting (%d/%d) to connect to tcp://%s:%d... ", nRetry+1, nMaxRetry, mqttObject->mqttConfig.serverUrl, mqttObject->mqttConfig.serverPort);

		fflush(stdout);
	
		rc = MQTTConnect(&mqttObject->mqttClient, &mqttObject->data);
		//printf("Connected %d\n", rc);
		fprintf(stdout, "%s\n", rc == SUCCESS ? "OK" : "Failed");
	    fflush(stdout);

		if (rc == SUCCESS) 
		{
			//connected			
			break;
		}
		else
		{
			MQTTDisconnect(&mqttObject->mqttClient);
			mqttObject->network.disconnect(&mqttObject->network);
		}
	}

	if (rc != SUCCESS)
	{
		fprintf(stdout, "Failed to connect to %s\n", mqttObject->mqttConfig.serverUrl);
		fflush(stdout);
	}

	return rc;
}

//-------------------------------------------------------------------------------------------------------
void* mqtt_CreateUserData(mqtt_instance_st * mqttObject)
{
	mqtt_ctxData_t* userCb =  (mqtt_ctxData_t*) mqtt_GetUserData(mqttObject, USER_DATA_INDEX);

	if (!userCb)
	{
		mqtt_ctxData_t * userData = (mqtt_ctxData_t *) malloc(sizeof(mqtt_ctxData_t));
		memset(userData, 0, sizeof(mqtt_ctxData_t));

		mqtt_SetUserData(mqttObject, userData, USER_DATA_INDEX);
	}

	return mqtt_GetUserData(mqttObject, USER_DATA_INDEX);
}

//-------------------------------------------------------------------------------------------------------
void mqtt_SetCommandHandler(mqtt_instance_st * mqttObject, incomingMessageHandler pHandler, void * pUserContext)
{
	
	mqtt_ctxData_t* userCb =  (mqtt_ctxData_t*) mqtt_CreateUserData(mqttObject);
	
	userCb->pfnUserCommandHandler = pHandler;
	userCb->pUserCommandContext = pUserContext;
}

//-------------------------------------------------------------------------------------------------------
void mqtt_SetSoftwareInstallRequestHandler(mqtt_instance_st * mqttObject, softwareInstallRequestHandler pHandler, void * pUserContext)
{
	mqtt_ctxData_t* userCb =  (mqtt_ctxData_t*) mqtt_CreateUserData(mqttObject);
	
	userCb->pfnUserSWInstallHandler = pHandler;
	userCb->pUserSWInstallContext = pUserContext;
}

//-------------------------------------------------------------------------------------------------------
void mqtt_OnIncomingMessage(MessageData* md)
{
	/*
		This is a callback function (handler), invoked by MQTT client whenever there is an incoming message
		It performs the following actions :
		  - deserialize the incoming MQTT JSON-formatted message
		  - call convertDataToCSV()
	*/

	MQTTMessage* message = md->message;
	MQTTString*  topicName = md->topicName;
	Client*      client = md->client;

	mqtt_instance_st * mqttObject = (mqtt_instance_st *) client->userCtxData;

	int payloadLen = (int)message->payloadlen;

	char* topic = malloc(topicName->lenstring.len + 1);
	memcpy(topic, topicName->lenstring.data, topicName->lenstring.len);
	topic[topicName->lenstring.len] = 0;

	fprintf(stdout, "\nIncoming data from topic %s :\n", topic);
	fprintf(stdout, "%.*s\n", payloadLen, (char*)message->payload);

	char* szPayload = (char *) malloc(payloadLen + 1);

	memcpy(szPayload, (char*)message->payload, payloadLen);
	szPayload[payloadLen] = 0;

	mqtt_ctxData_t* userCb =  (mqtt_ctxData_t*) mqtt_GetUserData(mqttObject, USER_DATA_INDEX);

	if (userCb && userCb->pfnUserCommandHandler)
	{
		userCb->pfnUserCommandHandler(topic, "", szPayload, "", userCb->pUserCommandContext);
	}

	if (topic)
	{
		free(topic);
	}
	if (szPayload)
	{
		free(szPayload);
	}
}

//-------------------------------------------------------------------------------------------------------
int mqtt_SubscribeTopic(mqtt_instance_st * mqttObject, const char* topicName)
{
	fprintf(stdout, "Subscribing to topic %s... ", topicName);
	int rc = MQTTSubscribe(&mqttObject->mqttClient, topicName, mqttObject->mqttConfig.qoS, mqtt_OnIncomingMessage);
	fprintf(stdout, "%s\n", rc == 0 ? "OK" : "Failed");
	//fprintf(stdout, "Subscribed %d\n", rc);
	fflush(stdout);

	return rc;
}


//-------------------------------------------------------------------------------------------------------
int mqtt_UnsubscribeTopic(mqtt_instance_st * mqttObject, const char* topicName)
{
	fprintf(stdout, "Unsubscribing to %s\n", topicName);
	int rc = MQTTUnsubscribe(&mqttObject->mqttClient, topicName);
	fprintf(stdout, "Unsubscribed %d\n", rc);
	fflush(stdout);

	return rc;
}

//-------------------------------------------------------------------------------------------------------
int mqtt_StopSession(mqtt_instance_st * mqttObject)
{
	//fprintf(stdout, "Disconnecting MQTT session");
	//fflush(stdout);
	int rc = MQTTDisconnect(&mqttObject->mqttClient);

	//fprintf(stdout, "Disconnecting Network : %p", &mqttObject->network);
	//fflush(stdout);
	if (mqttObject->network.disconnect)
	{
		mqttObject->network.disconnect(&mqttObject->network);
	}

	return rc;
}

//-------------------------------------------------------------------------------------------------------
int mqtt_IsConnected(mqtt_instance_st * mqttObject)
{
	if (mqttObject)
	{
		return mqttObject->mqttClient.isconnected;
	}
	
	return 0;
}
