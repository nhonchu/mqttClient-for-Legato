//--------------------------------------------------------------------------------------------------
/**
 * @file mqttTest.c
 *
 * This sample makes use of mqttClient over IPC, to start/stop mqttClient and to send mqtt messages to AirVantage.
 *
 * Nhon Chu
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 *
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"

static mqtt_SessionStateHandlerRef_t         HdlrRef;
static mqtt_IncomingMessageHandlerRef_t     hInMsgRef;

//---------------------------------------------------------------------------------------------------
static void SessionStateHandler
(
    bool                bIsConnected,
    int32_t             nConnectErrorCode,
    int32_t             nSubErrorCode,
    void*               contextPtr
)
{
    LE_INFO("CALLBACK, SessionStateHandler: %i, %d, %d", bIsConnected, nConnectErrorCode, nSubErrorCode);

    if (bIsConnected)
    {
        int32_t     error;
        LE_INFO("Sending request");
        mqtt_Send("TestApp", "testing", &error);    //MQTT session has been established, let's publish a message
        LE_INFO("mqtt_Send returns: %d", error);
    }
    else
    {
        exit(EXIT_SUCCESS);     //failed to connect to Broker, just quit
    }
}

//---------------------------------------------------------------------------------------------------
static void IncomingMessageHandler
(
    const char* szTopicName,
    const char* szKeyName,
    const char* szValue,
    const char* szTimestamps,
    void*       contextPtr
)
{
    LE_INFO("CALLBACK, IncomingMessageHandler: %s-> %s:%s@%s", szTopicName, szKeyName, szValue, szTimestamps);

    //in this sample code, when we received a message from mqtt Broker, we just quit the app, done with demo

    LE_INFO("Close connection");
    mqtt_Disconnect();


    exit(EXIT_SUCCESS);
}


//--------------------------------------------------------------------------------------------------
/**
 * App init.
 *
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    //Register a handler for MQTT connectivity state change
    HdlrRef = mqtt_AddSessionStateHandler(SessionStateHandler, NULL);
    
    if (!HdlrRef)
    {
        LE_ERROR("mqtt_AddSessionStateHandler has failed!");
    }
    else
    {
        LE_INFO("mqtt_AddSessionStateHandler succedded!");
    }

    //Register a handler to be notified on incoming messages sent from the MQTT Broker
    hInMsgRef = mqtt_AddIncomingMessageHandler(IncomingMessageHandler, NULL);
    if (!hInMsgRef)
    {
        LE_ERROR("mqtt_AddIncomingMessageHandler has failed!");
    }
    else
    {
        LE_INFO("mqtt_AddIncomingMessageHandler succedded!");
    }    

    //mqtt_Config("eu.airvantage.net", 1883, 30, 1, "userId", "password");  //not called, using default values defined in mqttClient

    LE_INFO("Calling mqttsClient to start MQTT connection");
    mqtt_Connect();

    LE_INFO("To quit the app, send a message from AirVantage to this device");
}
