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

#include <stdio.h>
#include "MQTTClient.h"

#include "mqttAirVantage.h"
#include "swir_json.h"

#include <stdio.h>
#include <signal.h>
#include <memory.h>

#include <sys/time.h>
#include <pthread.h>
#include <dirent.h>



/*---------- Parameters for AirVantage ---------------------------------*/
#define 	URL_AIRVANTAGE_SERVER			"eu.airvantage.net"
#define		PORT_AIRVANTAGE_SECURE			8883
#define		PORT_AIRVANTAGE_PLAIN			1883
#define 	TOPIC_NAME_PUBLISH				"/messages/json"
#define 	TOPIC_NAME_SUBSCRIBE			"/tasks/json"
#define 	TOPIC_NAME_ACK					"/acks/json"

#define		AV_MQTT_KEEP_ALIVE				30
#define		AV_MQTT_QOS						QOS0

#define		AV_USER_DATA_INDEX				1

//-------------------------------------------------------------------------------------------------------
char* getDeviceId(mqtt_instance_st * mqttObject)
{
	if (mqttObject)
	{
		return mqttObject->mqttConfig.deviceId;
	}

	return "";
}

//-------------------------------------------------------------------------------------------------------
void mqtt_avGetDefaultConfig(mqtt_config_t* mqttConfig)
{
	strcpy(mqttConfig->serverUrl, URL_AIRVANTAGE_SERVER);
    mqttConfig->serverPort = PORT_AIRVANTAGE_SECURE;
    mqttConfig->useTLS = 1;
    strcpy(mqttConfig->deviceId, "");
    strcpy(mqttConfig->username, "");
    strcpy(mqttConfig->secret, "sierra");
    mqttConfig->keepAlive = AV_MQTT_KEEP_ALIVE;
    mqttConfig->qoS = AV_MQTT_QOS;
}

//-------------------------------------------------------------------------------------------------------
int mqtt_avIsAirVantageUrl(const char * url)
{
	if (strcmp(URL_AIRVANTAGE_SERVER, url) == 0)
	{
		return 1;
	}

	return 0;
}

//-------------------------------------------------------------------------------------------------------
int mqtt_avIsAirVantageBroker(mqtt_instance_st * mqttObject)
{
	if (strcmp(URL_AIRVANTAGE_SERVER, mqttObject->mqttConfig.serverUrl) == 0)
	{
		return 1;
	}

	return 0;
}

//-------------------------------------------------------------------------------------------------------
int  mqtt_avPublishData(mqtt_instance_st * mqttObject, const char* szKey, const char* szValue)
{
	char* 	pTopic = (char *) malloc(strlen(getDeviceId(mqttObject)) + strlen(TOPIC_NAME_PUBLISH) + 1);
	sprintf(pTopic, "%s%s", getDeviceId(mqttObject), TOPIC_NAME_PUBLISH);

	int rc = mqtt_PublishKeyValue(mqttObject, szKey, szValue, pTopic);

	free(pTopic);

	return rc;
}


//-------------------------------------------------------------------------------------------------------
int mqtt_avPublishAck(mqtt_instance_st * mqttObject, const char* szUid, int nAck, const char* szMessage)
{
	char* szPayload = (char*) malloc(strlen(szUid)+strlen(szMessage)+48);

	if (nAck == 0)
	{
		sprintf(szPayload, "[{\"uid\": \"%s\", \"status\" : \"OK\"", szUid);
	}
	else
	{
		sprintf(szPayload, "[{\"uid\": \"%s\", \"status\" : \"KO\"", szUid);
	}

	if (strlen(szMessage) > 0)
	{
		sprintf(szPayload, "%s, \"message\" : \"%s\"}]", szPayload, szMessage);
	}
	else
	{
		sprintf(szPayload, "%s}]", szPayload);
	}

	printf("Sending ACK: %s\n", szPayload);

	char* 	pTopic = (char *) malloc(strlen(getDeviceId(mqttObject)) + strlen(TOPIC_NAME_ACK) + 1);
	sprintf(pTopic, "%s%s", getDeviceId(mqttObject), TOPIC_NAME_ACK);

	int rc =  mqtt_PublishData(mqttObject, szPayload, strlen(szPayload), pTopic);

	free(pTopic);
	free(szPayload);

	return rc;
}


//-------------------------------------------------------------------------------------------------------
void mqtt_avOnIncomingMessage(MessageData* md)
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

	char* topic = (char *) malloc(topicName->lenstring.len + 1);
	memcpy(topic, topicName->lenstring.data, topicName->lenstring.len);
	topic[topicName->lenstring.len] = 0;

	printf("\nIncoming data from topic %s(%d) : Length(%d)\n", topic, topicName->lenstring.len, payloadLen);
	printf("%.*s\n", payloadLen, (char*)message->payload);

	char* szPayload = (char *) malloc(payloadLen + 1);

	memcpy(szPayload, (char*)message->payload, payloadLen);
	szPayload[payloadLen] = 0;

	//decode JSON payload

	char* pszCommand = swirjson_getValue(szPayload, -1, (char *) "command");
	if (pszCommand)
	{
		char*	pszUid = swirjson_getValue(szPayload, -1, (char *) "uid");
		char*	pszTimestamp = swirjson_getValue(szPayload, -1, (char *) "timestamp");
		char*	pszId = swirjson_getValue(pszCommand, -1, (char *) "id");
		char*	pszParam = swirjson_getValue(pszCommand, -1, (char *) "params");

		int     i;
		int		rc = 0;

		#define     AV_JSON_KEY_MAX_COUNT       	10
		#define     AV_JSON_KEY_MAX_LENGTH      	32

		for (i=0; i<AV_JSON_KEY_MAX_COUNT; i++)
		{
			char szKey[AV_JSON_KEY_MAX_LENGTH];

			char * pszValue = swirjson_getValue(pszParam, i, szKey);

			if (pszValue)
			{
				char key[128] = {0};
				sprintf(key, "%s.%s", pszId, szKey);

				if (mqttObject)
				{
					mqtt_ctxData_t* userCb =  (mqtt_ctxData_t*) mqtt_GetUserData(mqttObject, AV_USER_DATA_INDEX);

					if (userCb && userCb->pfnUserCommandHandler)
					{
						printf("calling user handler");
						userCb->pfnUserCommandHandler(topic, key, pszValue, pszTimestamp, userCb->pUserCommandContext);
					}
				}
				else
				{
					fprintf(stdout, "Command[%d] : %s, %s, %s, %s\n", i, topic, key, pszValue, pszTimestamp);	
				}
				free(pszValue);
			}
			else
			{
				break;
			}
		}

		mqtt_avPublishAck(mqttObject, pszUid, rc, (char *) "");

		free(pszCommand);

		if (pszId)
		{
			free(pszId);
		}
		if (pszParam)
		{
			free(pszParam);
		}
		if (pszTimestamp)
		{
			free(pszTimestamp);
		}
		if (pszUid)
		{
			free(pszUid);
		}
	}
	else
	{
		pszCommand = swirjson_getValue(szPayload, -1, (char *) "swinstall");
		if (pszCommand)
		{
			char*	uid = swirjson_getValue(szPayload, -1, (char *) "uid");
			char*	pszTimestamp = swirjson_getValue(szPayload, -1, (char *) "timestamp");
			char*	type = swirjson_getValue(pszCommand, -1, (char *) "type");
			char*	revision = swirjson_getValue(pszCommand, -1, (char *) "revision");
			char*	url = swirjson_getValue(pszCommand, -1, (char *) "url");

			if (mqttObject)
			{
				mqtt_ctxData_t* userCb =  (mqtt_ctxData_t*) mqtt_GetUserData(mqttObject, AV_USER_DATA_INDEX);

				if (userCb && userCb->pfnUserSWInstallHandler)
				{
					userCb->pfnUserSWInstallHandler(uid, type, revision, url, pszTimestamp, userCb->pUserSWInstallContext);
				}
			}
			else
			{
				fprintf(stdout, "SW install Request : %s, %s, %s, %s, %s\n", uid, type, revision, url, pszTimestamp);	
			}

			free(pszCommand);

			if (uid)
			{
				free(uid);
			}
			if (type)
			{
				free(type);
			}
			if (pszTimestamp)
			{
				free(pszTimestamp);
			}
			if (revision)
			{
				free(revision);
			}
			if (url)
			{
				free(url);
			}
		}
	}

	if (topic)
	{
		free(topic);
	}
	if (szPayload)
	{
		free(szPayload);
	}

	fflush(stdout);
}

//-------------------------------------------------------------------------------------------------------
void mqtt_avSubscribeAirVantageTopic(mqtt_instance_st* mqttObject)
{
	char* 	pTopic = (char *) malloc(strlen(getDeviceId(mqttObject)) + strlen(TOPIC_NAME_SUBSCRIBE) + 1);
	sprintf(pTopic, "%s%s", getDeviceId(mqttObject), TOPIC_NAME_SUBSCRIBE);

	printf("Subscribing to topic %s... ", pTopic);
	int rc = MQTTSubscribe(&mqttObject->mqttClient, pTopic, mqttObject->mqttConfig.qoS, mqtt_avOnIncomingMessage);
	printf("%s\n", rc == 0 ? "OK" : "Failed");
	//printf("Subscribed %d\n", rc);
	fflush(stdout);

	free(pTopic);
}


//-------------------------------------------------------------------------------------------------------
void* mqtt_avCreateUserData(mqtt_instance_st * mqttObject)
{
	mqtt_ctxData_t* userCb =  (mqtt_ctxData_t*) mqtt_GetUserData(mqttObject, AV_USER_DATA_INDEX);

	if (!userCb)
	{
		mqtt_ctxData_t * userData = (mqtt_ctxData_t *) malloc(sizeof(mqtt_ctxData_t));
		memset(userData, 0, sizeof(mqtt_ctxData_t));

		mqtt_SetUserData(mqttObject, userData, AV_USER_DATA_INDEX);

		mqtt_avSubscribeAirVantageTopic(mqttObject);
	}

	return mqtt_GetUserData(mqttObject, AV_USER_DATA_INDEX);
}

//-------------------------------------------------------------------------------------------------------
void mqtt_avSetCommandHandler(mqtt_instance_st * mqttObject, incomingMessageHandler pHandler, void * pUserContext)
{
	
	mqtt_ctxData_t* userCb =  (mqtt_ctxData_t*) mqtt_avCreateUserData(mqttObject);
	
	userCb->pfnUserCommandHandler = pHandler;
	userCb->pUserCommandContext = pUserContext;
}

//-------------------------------------------------------------------------------------------------------
void mqtt_avSetSoftwareInstallRequestHandler(mqtt_instance_st * mqttObject, softwareInstallRequestHandler pHandler, void * pUserContext)
{
	mqtt_ctxData_t* userCb =  (mqtt_ctxData_t*) mqtt_avCreateUserData(mqttObject);
	
	userCb->pfnUserSWInstallHandler = pHandler;
	userCb->pUserSWInstallContext = pUserContext;
}



