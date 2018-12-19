/**
 * This module implements MQTT Client interacting with a MQTT Broker
 * The default broker is Sierra Wireless AirVantage server
 *
 * This component exposes an API, accessible by other Legato apps over IPC, refer to mqtt.api
 * 
 * Nhon Chu
 *
 *
 */

/* Legato Framework */
#include "legato.h"
#include "interfaces.h"


#include "le_info_interface.h"

#include "mqttAirVantage.h"


#define MAX_INSTANCE                    5

typedef struct 
{
    mqtt_config_t                       mqttConfig;
    mqtt_instance_st*                   mqttObject;
} ST_MQTT_CLIENT;



//--------------------------------------------------------------------------------------------------
/**
 *  This mqtt client manager can handle up to MAX_INSTANCE instances of mqtt client
 *  client instace #0 is reserved for CLI
 */
//--------------------------------------------------------------------------------------------------

// Pool from which ST_MQTT_CLIENT objects are allocated.
le_mem_PoolRef_t            g_MqttClientPool;
 
// Safe Reference Map for ST_MQTT_CLIENT objects.
le_ref_MapRef_t             g_MqttClientRefMap;


#define GET_MQTT_OBJECT(mqttClientRef) ST_MQTT_CLIENT* mqttClientPtr = le_ref_Lookup(g_MqttClientRefMap, mqttClientRef)

////////////////////////////////////////////////////////////////////////////////////////////////////


//--------------------------------------------------------------------------------------------------
/**
 * This function adds a handler ...
 */
//--------------------------------------------------------------------------------------------------
mqttClient_IncomingMessageHandlerRef_t mqttClient_AddIncomingMessageHandler
(
    mqttClient_InstanceRef_t                    mqttClientRef,
    mqttClient_IncomingMessageHandlerFunc_t     handlerPtr,
    void*                                       contextPtr
)
{
    GET_MQTT_OBJECT(mqttClientRef);

    if (mqttClientPtr != NULL && mqttClientPtr->mqttObject != NULL)
    {
        if (mqtt_avIsAirVantageBroker(mqttClientPtr->mqttObject))
        {
            //Set handler for AirVantage incoming message/command/SW-install
            mqtt_avSetCommandHandler(mqttClientPtr->mqttObject, (incomingMessageHandler) handlerPtr, contextPtr);
        }
        //else
        {
            mqtt_SetCommandHandler(mqttClientPtr->mqttObject, (incomingMessageHandler) handlerPtr, contextPtr);
        }

        return (mqttClient_IncomingMessageHandlerRef_t) mqttClientRef;
    }

    return NULL;
}


//--------------------------------------------------------------------------------------------------
/**
 * This function removes a handler ...
 */
//--------------------------------------------------------------------------------------------------
void mqttClient_RemoveIncomingMessageHandler
(
    mqttClient_IncomingMessageHandlerRef_t incomingMsgHandlerRef
)
{
    GET_MQTT_OBJECT(incomingMsgHandlerRef);

    if (mqttClientPtr != NULL && mqttClientPtr->mqttObject != NULL)
    {
        if (mqtt_avIsAirVantageBroker(mqttClientPtr->mqttObject))
        {
            //Set handler for AirVantage incoming message/command/SW-install
            mqtt_avSetCommandHandler(mqttClientPtr->mqttObject, NULL, NULL);
        }
        //else
        {
            mqtt_SetCommandHandler(mqttClientPtr->mqttObject, NULL, NULL);
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * This function adds a handler ...
 */
//--------------------------------------------------------------------------------------------------
mqttClient_AvSoftwareInstallHandlerRef_t mqttClient_AddAvSoftwareInstallHandler
(
    mqttClient_InstanceRef_t                    mqttClientRef,
    mqttClient_AvSoftwareInstallHandlerFunc_t   handlerPtr,
    void*                                       contextPtr
)
{
    GET_MQTT_OBJECT(mqttClientRef);

    if (mqttClientPtr != NULL && mqttClientPtr->mqttObject != NULL)
    {
        if (mqtt_avIsAirVantageBroker(mqttClientPtr->mqttObject))
        {
            //Set handler for AirVantage incoming message/command/SW-install
            mqtt_avSetSoftwareInstallRequestHandler(mqttClientPtr->mqttObject, (softwareInstallRequestHandler) handlerPtr, contextPtr);
        }

        return (mqttClient_AvSoftwareInstallHandlerRef_t) mqttClientRef;
    }

    return NULL;
}


//--------------------------------------------------------------------------------------------------
/**
 * This function removes a handler ...
 */
//--------------------------------------------------------------------------------------------------
void mqttClient_RemoveAvSoftwareInstallHandler
(
    mqttClient_AvSoftwareInstallHandlerRef_t swInstallHandlerRef
)
{
    GET_MQTT_OBJECT(swInstallHandlerRef);

    if (mqttClientPtr != NULL && mqttClientPtr->mqttObject != NULL)
    {
        if (mqtt_avIsAirVantageBroker(mqttClientPtr->mqttObject))
        {
            //Set handler for AirVantage incoming message/command/SW-install
            mqtt_avSetSoftwareInstallRequestHandler(mqttClientPtr->mqttObject, NULL, NULL);
        }
    }
}


//-------------------------------------------------------------------------
le_result_t mqttClient_ProcessEvent
(
    mqttClient_InstanceRef_t    mqttClientRef
)
{
    GET_MQTT_OBJECT(mqttClientRef);

    if (mqttClientPtr != NULL && mqttClientPtr->mqttObject != NULL)
    {
        int ret = mqtt_ProcessEvent(mqttClientPtr->mqttObject, 1000);

        if (ret >= -1)  //paho : FAILURE, SUCCESS, or packet_type
        {
            return LE_OK;
        }
    }

    return LE_FAULT;
}

//-------------------------------------------------------------------------
le_result_t mqttClient_Subscribe
(
    mqttClient_InstanceRef_t    mqttClientRef,
    const char*                 topicName
)
{
    GET_MQTT_OBJECT(mqttClientRef);

    if (mqttClientPtr != NULL && mqttClientPtr->mqttObject != NULL)
    {
        int ret = mqtt_SubscribeTopic(mqttClientPtr->mqttObject, topicName);

        if (0 == ret)
        {
            return LE_OK;
        }
    }

    return LE_FAULT;
}

//-------------------------------------------------------------------------
le_result_t mqttClient_Unsubscribe
(
    mqttClient_InstanceRef_t    mqttClientRef,
    const char*                 topicName
)
{
    GET_MQTT_OBJECT(mqttClientRef);

    if (mqttClientPtr != NULL && mqttClientPtr->mqttObject != NULL)
    {
        int ret = mqtt_UnsubscribeTopic(mqttClientPtr->mqttObject, topicName);

        if (0 == ret)
        {
            return LE_OK;
        }
    }

    return LE_FAULT;
}

//-------------------------------------------------------------------------
le_result_t mqttClient_AvPublish
(
    mqttClient_InstanceRef_t        mqttClientRef,
    const char *                    key,
    const char *                    value
)
{
    GET_MQTT_OBJECT(mqttClientRef);

    if (mqttClientPtr != NULL && mqttClientPtr->mqttObject != NULL)
    {
        int ret = mqtt_avPublishData(mqttClientPtr->mqttObject, key, value);

        if (0 == ret)
        {
            return LE_OK;
        }
    }

    return LE_FAULT;
}

//-------------------------------------------------------------------------
le_result_t mqttClient_AvAck
(
    mqttClient_InstanceRef_t        mqttClientRef,
    const char *                    uid,
    int32_t                         errorCode,
    const char *                    message
)
{
    GET_MQTT_OBJECT(mqttClientRef);

    if (mqttClientPtr != NULL && mqttClientPtr->mqttObject != NULL)
    {
        int ret = mqtt_avPublishAck(mqttClientPtr->mqttObject, uid, errorCode, message);

        if (0 == ret)
        {
            return LE_OK;
        }
    }

    return LE_FAULT;
}

//-------------------------------------------------------------------------
le_result_t mqttClient_Publish
(
    mqttClient_InstanceRef_t    mqttClientRef,
    const uint8_t *             data,
    size_t                      dataSize,
    const char *                topicName
)
{
    GET_MQTT_OBJECT(mqttClientRef);

    if (mqttClientPtr != NULL && mqttClientPtr->mqttObject != NULL)
    {
        int ret = mqtt_PublishData(mqttClientPtr->mqttObject, (const char *) data, dataSize, topicName);

        if (0 == ret)
        {
            return LE_OK;
        }
    }

    return LE_FAULT;
}

//-------------------------------------------------------------------------
le_result_t mqttClient_PublishKeyValue
(
    mqttClient_InstanceRef_t        mqttClientRef,
    const char *                    key,
    const char *                    value,
    const char *                    topicName
)
{
    GET_MQTT_OBJECT(mqttClientRef);

    if (mqttClientPtr != NULL && mqttClientPtr->mqttObject != NULL)
    {
        int ret = 1;

        bool noTopic = false;
        if (topicName != NULL && strlen(topicName) == 0)
        {
            noTopic = true;
        }
        else if (topicName == NULL)
        {
            noTopic = true;
        }

        if (noTopic && mqtt_avIsAirVantageBroker(mqttClientPtr->mqttObject))
        {
            ret = mqtt_avPublishData(mqttClientPtr->mqttObject, key, value);
        }
        else
        {
            if (noTopic)
            {
                char    defaultTopic[] = "LegatoMqttClient";

                ret = mqtt_PublishKeyValue(mqttClientPtr->mqttObject, key, value, defaultTopic);   
            }
            else
            {
                ret = mqtt_PublishKeyValue(mqttClientPtr->mqttObject, key, value, topicName);
            }
        }

        if (0 == ret)
        {
            return LE_OK;
        }
    }

    return LE_FAULT;
}

//-------------------------------------------------------------------------
uint8_t * readFile(const char * filename, long * lDataSize)
{
    FILE*           file = NULL;
    uint8_t*        pFileBuffer = NULL;

    *lDataSize = 0;

    LE_INFO("Loading binary file %s...", filename);
    file = fopen(filename, "r");
    if (NULL == file)
    {
        LE_INFO("Cannot open file %s...", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    *lDataSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    LE_INFO("allocating %ld bytes...", *lDataSize);
    pFileBuffer = (uint8_t *) malloc(*lDataSize);

    if (pFileBuffer)
    {
        memset(pFileBuffer, 0, *lDataSize);
        fread(pFileBuffer, 1, *lDataSize, file);
    }

    fclose(file);
    return pFileBuffer;
}


//-------------------------------------------------------------------------
le_result_t mqttClient_PublishFileContent
(
    mqttClient_InstanceRef_t        mqttClientRef,
    const char *                    filename,
    const char *                    topicName
)
{
    le_result_t ret = LE_FAULT;

    GET_MQTT_OBJECT(mqttClientRef);

    if (mqttClientPtr != NULL && mqttClientPtr->mqttObject != NULL)
    {
        bool noTopic = false;
        if (topicName != NULL && strlen(topicName) == 0)
        {
            noTopic = true;
        }
        else if (topicName == NULL)
        {
            noTopic = true;
        }

        if (noTopic)
        {
            return ret;
        }

        
        long     lDataSize = 0;
        uint8_t* pData = readFile(filename, &lDataSize);
        if (pData)
        {
            ret = mqttClient_Publish(mqttClientRef, pData, (size_t)lDataSize, topicName);

            free(pData);
        }
    }

    return ret;
}

//-------------------------------------------------------------------------
bool mqttClient_IsConnected
(
    mqttClient_InstanceRef_t       mqttClientRef
)
{
    GET_MQTT_OBJECT(mqttClientRef);

    if (mqttClientPtr != NULL && mqttClientPtr->mqttObject != NULL)
    {
        return mqtt_IsConnected(mqttClientPtr->mqttObject);
    }

    return false;
}

//-------------------------------------------------------------------------
le_result_t mqttClient_StartSession
(
    mqttClient_InstanceRef_t       mqttClientRef
)
{
    GET_MQTT_OBJECT(mqttClientRef);

    LE_INFO("StartSession called");
    if (mqttClientPtr != NULL)
    {
        if (mqttClientPtr->mqttObject != NULL)
        {
            int ret = mqtt_StartSession(mqttClientPtr->mqttObject);
            if (0 == ret)
            {
                if (mqtt_avIsAirVantageBroker(mqttClientPtr->mqttObject))
                {
                    //Set handler for AirVantage incoming message/command/SW-install
                    mqtt_avSubscribeAirVantageTopic(mqttClientPtr->mqttObject);
                }

                return LE_OK;
            }
        }
    }

    return LE_FAULT;
}

//-------------------------------------------------------------------------
le_result_t mqttClient_StopSession
(
    mqttClient_InstanceRef_t       mqttClientRef
)
{
    GET_MQTT_OBJECT(mqttClientRef);

    if (mqttClientPtr != NULL)
    {
        if (mqttClientPtr->mqttObject != NULL)
        {
            int ret = mqtt_StopSession(mqttClientPtr->mqttObject);
            if (0 == ret)
            {
                return LE_OK;
            }
        }
    }

    return LE_FAULT;
}

//-------------------------------------------------------------------------
le_result_t mqttClient_SetTls
(
    mqttClient_InstanceRef_t    mqttClientRef,
    const char*                 rootCAFile,
    const char*                 certificateFile,
    const char*                 privateKeyFile
)
{
    GET_MQTT_OBJECT(mqttClientRef);

    if (mqttClientPtr != NULL && mqttClientPtr->mqttObject != NULL)
    {
        mqtt_SetTls(mqttClientPtr->mqttObject, rootCAFile, certificateFile, privateKeyFile);

        return LE_OK;
    }

    return LE_FAULT;
}

//-------------------------------------------------------------------------
mqttClient_InstanceRef_t mqttClient_Create
(
    const char*   brokerUrl,
    int32_t       portNumber,
    int32_t       useTLS,
    const char*   deviceId,
    const char*   username,
    const char*   secret,
    int32_t       keepAlive,
    int32_t       qoS
)
{
    ST_MQTT_CLIENT* mqttClientPtr = le_mem_ForceAlloc(g_MqttClientPool);
 
    memset(mqttClientPtr, 0, sizeof(ST_MQTT_CLIENT));
 
    strcpy(mqttClientPtr->mqttConfig.serverUrl, brokerUrl);
    mqttClientPtr->mqttConfig.serverPort = portNumber;
    mqttClientPtr->mqttConfig.useTLS = useTLS;
    strcpy(mqttClientPtr->mqttConfig.deviceId, deviceId);
    strcpy(mqttClientPtr->mqttConfig.username, username);
    strcpy(mqttClientPtr->mqttConfig.secret, secret);
    mqttClientPtr->mqttConfig.keepAlive = keepAlive;
    mqttClientPtr->mqttConfig.qoS = qoS;

    if (mqtt_avIsAirVantageUrl(mqttClientPtr->mqttConfig.serverUrl))
    {
        if (strlen(mqttClientPtr->mqttConfig.username) == 0)
        {
            strcpy(mqttClientPtr->mqttConfig.username, mqttClientPtr->mqttConfig.deviceId);
        }
    }


    mqttClientPtr->mqttObject = mqtt_CreateInstance(&mqttClientPtr->mqttConfig);

    // Create and return a Safe Reference for this new ST_MQTT_CLIENT object.
    mqttClient_InstanceRef_t clientRef = le_ref_CreateRef(g_MqttClientRefMap, mqttClientPtr);
    LE_INFO("Created mqttClientRef : %p", clientRef);

    return clientRef;
}

//-------------------------------------------------------------------------
le_result_t mqttClient_Delete
(
    mqttClient_InstanceRef_t   mqttClientRef
)
{
    GET_MQTT_OBJECT(mqttClientRef);
 
    if (mqttClientPtr != NULL && mqttClientPtr->mqttObject != NULL)
    {
        LE_INFO("Deleting MQTT instance mqttClientRef : %p", mqttClientRef);
        mqttClientPtr->mqttObject = mqtt_DeleteInstance(mqttClientPtr->mqttObject);

        LE_INFO("Deleting mqttClientRef : %p", mqttClientRef);
        le_ref_DeleteRef(g_MqttClientRefMap, mqttClientRef);
 
        LE_INFO("Releasing memory : %p", mqttClientPtr);
        le_mem_Release(mqttClientPtr);

        return LE_OK;
    }

    return LE_FAULT;
}

//------------------------------------------------------------------
le_result_t mqttClient_GetConfig
(
    mqttClient_InstanceRef_t        mqttClientRef,
    char*                           broker,
    size_t                          brokerLen,
    int32_t*                        portNumber,
    int32_t*                        useTLS,
    char*                           deviceId,
    size_t                          deviceIdLen,
    char*                           username,
    size_t                          usernameLen,
    char*                           secret,
    size_t                          secretLen,
    int32_t*                        keepAlive,
    int32_t*                        qoS
)
{

    GET_MQTT_OBJECT(mqttClientRef);
 
    if (mqttClientPtr == NULL)
    {
        mqtt_config_t   mqttConfig;

        mqtt_avGetDefaultConfig(&mqttConfig);

        //le_info_GetImei(mqttConfig.deviceId, sizeof(mqttConfig.deviceId));
        le_info_GetPlatformSerialNumber(mqttConfig.deviceId, sizeof(mqttConfig.deviceId));
        LE_INFO("Serial Number = %s", mqttConfig.deviceId);

        strcpy(broker, mqttConfig.serverUrl);
        *portNumber = mqttConfig.serverPort;
        *useTLS = mqttConfig.useTLS;
        strcpy(deviceId, mqttConfig.deviceId);
        strcpy(username, mqttConfig.username);
        strcpy(secret, mqttConfig.secret);
        *keepAlive = mqttConfig.keepAlive;
        *qoS = mqttConfig.qoS;
    }
    else
    {
        strcpy(broker, mqttClientPtr->mqttConfig.serverUrl);
        *portNumber = mqttClientPtr->mqttConfig.serverPort;
        *useTLS = mqttClientPtr->mqttConfig.useTLS;
        strcpy(deviceId, mqttClientPtr->mqttConfig.deviceId);
        strcpy(username, mqttClientPtr->mqttConfig.username);
        strcpy(secret, mqttClientPtr->mqttConfig.secret);
        *keepAlive = mqttClientPtr->mqttConfig.keepAlive;
        *qoS = mqttClientPtr->mqttConfig.qoS;
    }

    return LE_OK;
}
//--------------------------------------------------------------------------------------------------
/**
 *  Main function.
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    LE_INFO("---------- Launching mqttClient Manager----------");

    // Create the ST_MQTT_CLIENT object pool.
    g_MqttClientPool = le_mem_CreatePool("stMqttClient", sizeof(ST_MQTT_CLIENT));
    le_mem_ExpandPool(g_MqttClientPool, MAX_INSTANCE);
 
    // Create the Safe Reference Map to use for ST_MQTT_CLIENT object Safe References.
    g_MqttClientRefMap = le_ref_CreateMap("MqttClientMap", MAX_INSTANCE);


    LE_INFO("MQTT Client Service started");

}

