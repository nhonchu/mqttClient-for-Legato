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

/* data Connection Services (Client) */
#include "le_data_interface.h"

#include "le_info_interface.h"

#include "MQTTClient.h"
#include "json/swir_json.h"

#define     DEFAULT_SIZE                32
#define     MAX_URL_LENGTH              256
#define     MAX_PAYLOAD_SIZE            2048    //Default payload buffer size
#define     MQTT_VERSION                3
#define     TIMEOUT_MS                  10000    //1 second time-out, MQTT client init
#define     TOPIC_NAME_PUBLISH          "/messages/json"
#define     TOPIC_NAME_SUBSCRIBE        "/tasks/json"
#define     TOPIC_NAME_ACK              "/acks/json"
#define     URL_AIRVANTAGE_SERVER       "eu.airvantage.net"
#define     PORT_AIRVANTAGE_SERVER      1883
//#define     PORT_AIRVANTAGE_SERVER      8883
#define     DEFAULT_KEEP_ALIVE          30      //seconds
#define     YIELD_INTERVAL_SECOND       15
#define     DEFAULT_QOS                 1       //[0, 2]
#define     AV_JSON_KEY_MAX_COUNT       10
#define     AV_JSON_KEY_MAX_LENGTH      32

#define     MAX_ARGS                    10
#define     MAX_ARG_LENGTH              32
#define     MAX_INMESSAGE_IN_LIST       20

typedef struct
{
    Client          oMqttClient;
    Network         oNetwork;
    char            szBrokerUrl[MAX_URL_LENGTH];
    int32_t         u32PortNumber;
    int32_t         u32KeepAlive;
    int32_t         u32QoS;
    char            szKey[DEFAULT_SIZE];
    char            szValue[DEFAULT_SIZE];
    char            szDeviceId[DEFAULT_SIZE];
    char            szSecret[DEFAULT_SIZE];
    char            szSubscribeTopic[2*DEFAULT_SIZE];
} ST_PROGRAM_CONTEXT;

typedef enum
{
    IDLE,
    CONNECTING,
    DISCONNECTING,
    DATA_CONNECTED,
    DATA_DISCONNECTED,
    MQTT_CONNECTED,
    MQTT_DISCONNECTED
} E_MQTTCLIENT_STATE;

typedef struct
{
    le_sls_Link_t   link;
    char            message[256];
} ST_MQTT_INCOMING_MSG_T;

static le_sls_List_t        g_inMessageList = LE_SLS_LIST_INIT;
static le_mem_PoolRef_t     g_inMessagePool = NULL;

static ST_PROGRAM_CONTEXT   g_stContext;
static E_MQTTCLIENT_STATE   g_eState = IDLE;

static le_data_ConnectionStateHandlerRef_t  g_hDataConnectionState = NULL;

static unsigned char        g_buf[MAX_PAYLOAD_SIZE];
static unsigned char        g_readbuf[MAX_PAYLOAD_SIZE];
//--------------------------------------------------------------------------------------------------
/**
 *  The Data Connection reference
 */
//--------------------------------------------------------------------------------------------------
static le_data_RequestObjRef_t  g_RequestRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Event for sending connection to applications
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t MqttConnStateEvent;
static le_event_Id_t MqttInMsgEvent;
//--------------------------------------------------------------------------------------------------
/**
 * Data associated with the above MqttConnStateEvent.
 *
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    bool    bIsConnected;
    int     nConnectErrorCode;
    int     nSubErrorCode;
}
MqttConnStateData_t;

//--------------------------------------------------------------------------------------------------
/**
 * Data associated with the above ConnStateEvent.
 *
 * interfaceName is only valid if isConnected is true.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    char szTopicName[128+1];
    char szKeyName[128+1];
    char szValue[128+1];
    char szTimestamp[16+1];
}
MqttInMsgData_t;

////////////////////////////////////////////////////////////////////////////////////////////////////
static void SendMqttConnStateEvent
(
    bool        bIsConnected,
    int32_t     nConnectErrorCode,
    int32_t     nSubErrorCode
);

static void DcsStateHandler
(
    const char* intfName,
    bool        isConnected,
    void*       contextPtr
);
////////////////////////////////////////////////////////////////////////////////////////////////////

void PrintMessage(char* szTrace, ...)
{
    bool    bIsSandboxed = (getuid() != 0);

    char    szMessage[256];
    va_list args;

    va_start(args, szTrace);
    vsnprintf(szMessage, sizeof(szMessage), szTrace, args);
    va_end(args);

    if (bIsSandboxed)
    {
        LE_INFO("    %s", szMessage);
    }
    else
    {
        puts(szMessage);
        puts("\n");
    }
}

le_result_t       QueueIncomingMessage(const char* message)
{
    ST_MQTT_INCOMING_MSG_T*    newNodePtr = NULL;

    if (g_inMessagePool == NULL)
    {
        g_inMessagePool = le_mem_CreatePool("mqtt-incoming-message", sizeof(ST_MQTT_INCOMING_MSG_T));
        le_mem_ExpandPool(g_inMessagePool, 20);
    }

    newNodePtr = le_mem_ForceAlloc(g_inMessagePool);
    if (newNodePtr)
    {
        strcpy(newNodePtr->message, message);

        // Initialize the link.
        newNodePtr->link = LE_SLS_LINK_INIT;

        // Insert the new node to the tail.
        le_sls_Queue(&g_inMessageList, &(newNodePtr->link));

        if (le_sls_NumLinks(&g_inMessageList) > MAX_INMESSAGE_IN_LIST)
        {
            //discard the top one in the list (oldest)
            le_sls_Link_t*  linkPtr = le_sls_Pop(&g_inMessageList);
            ST_MQTT_INCOMING_MSG_T* topNodePtr = CONTAINER_OF(linkPtr, ST_MQTT_INCOMING_MSG_T, link);
            le_mem_Release(topNodePtr);
        }

        PrintMessage("Incoming messages queued : %d", le_sls_NumLinks(&g_inMessageList));
        return LE_OK;
    }

    return LE_FAULT;
}

//----------------------------------------------------------------------------------
char* prv_end_of_space(char* buffer)
{
    while (isspace(buffer[0]&0xff))
    {
        buffer++;
    }
    return buffer;
}

char* get_end_of_arg(char* buffer)
{
    while (buffer[0] != 0 && !isspace(buffer[0]&0xFF))
    {
        buffer++;
    }
    return buffer;
}

char * get_next_arg(char * buffer, char** end)
{
    // skip arg
    buffer = get_end_of_arg(buffer);
    // skip space
    buffer = prv_end_of_space(buffer);
    if (NULL != end)
    {
        *end = (void *) get_end_of_arg(buffer);
    }

    return buffer;
}

int decodeArgument(const char* command, char argument[MAX_ARGS][MAX_ARG_LENGTH])
{
    char    args[256] = {0};
    char *  end = NULL;
    char *  value = args;

    strcpy(args, command);

    int     argLen, index=0;

    int argCount = 0;

    LE_INFO("decodeArg - %s", args);

    memset(argument, 0, MAX_ARGS*MAX_ARG_LENGTH);

    //              value    end
    //              |        |
    //              V        V
    //get 1st arg : argument1 argument2 argument3
    end = get_end_of_arg(value);
    argLen = end-value;

    if (argLen >= 1)
    {
        memcpy(argument[index], value, argLen);

        LE_INFO("decodeArg(%d) = %s", index, argument[index]);

        index++;
        argCount++;
    }
    else
    {
        return argCount;
    }

    LE_INFO("decodeArg(%d) = %s", index-1, argument[index-1]);

    while (1)
    {
        value = get_next_arg(end, &end);
        argLen = end-value;

        if (argLen >= 1)
        {
            memcpy(argument[index], value, argLen);

            LE_INFO("decodeArg(%d) = %s", index, argument[index]);

            index++;
            argCount++;
        }
        else
        {
            return argCount;
        }
    }

    return argCount;
}
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 *  This function will request the data connection
 */
//--------------------------------------------------------------------------------------------------
static void ConnectData
(
    void
)
{
    if (g_RequestRef)
    {
        LE_ERROR("A data connection request already exist.");
        return;
    }

    g_eState = CONNECTING;

    // register handler for data connection state change
    if (!g_hDataConnectionState)
    {
        g_hDataConnectionState = le_data_AddConnectionStateHandler(DcsStateHandler, NULL);
    }
    g_RequestRef = le_data_Request();
    LE_INFO("Requesting the data connection: %p.", g_RequestRef);
}

//--------------------------------------------------------------------------------------------------
/**
 *  The opposite of ConnectData, this function will tear down the data connection.
 */
//--------------------------------------------------------------------------------------------------
static void DisconnectData
(
    void
)
{
    if (!g_RequestRef)
    {
        LE_ERROR("Not existing data connection reference.");
        return;
    }
    
    if (g_stContext.oMqttClient.isconnected)
    {
        LE_INFO("Dispose MQTT resources.");
        MQTTDisconnect(&g_stContext.oMqttClient);
        g_eState = MQTT_DISCONNECTED;
    }

    if (g_eState >= DATA_CONNECTED)
    {
        g_stContext.oNetwork.disconnect(&g_stContext.oNetwork);
    }

    LE_INFO("Releasing the data connection.");
    if (g_hDataConnectionState)
    {
        le_data_RemoveConnectionStateHandler(g_hDataConnectionState);
        g_hDataConnectionState = NULL;
    }
    le_data_Release(g_RequestRef);

    g_RequestRef = NULL;

    g_eState = IDLE;

    SendMqttConnStateEvent(false, 0, 0);

    PrintMessage("MQTT Disconnected");
}


static void StartTimer
(
    le_timer_Ref_t  timerRef
)
{
    le_result_t     res;

    //start timer
    res = le_timer_Start(timerRef);
    LE_FATAL_IF(res != LE_OK, "Unable to start timer: %d", res);
}

static void timerHandler
(
    le_timer_Ref_t  timerRef
)
{
    if (g_stContext.oMqttClient.isconnected)
    {
        LE_INFO("MQTT yield");
        MQTTYield(&g_stContext.oMqttClient, 3000);
        StartTimer(timerRef);
    }
    else
    {
        LE_INFO("No active MQTT session, now stop Timer");
        le_timer_Stop(timerRef);
    }
    
}

static int publishAckCmd(const char* szUid, int nAck, char* szMessage)
{
    char* szPayload = (char*) malloc(strlen(szUid)+strlen(szMessage)+48);

    if (nAck == 0)
    {
        sprintf(szPayload, "[{\"uid\": \"%s\", \"status\" : \"OK\"", szUid);
    }
    else
    {
        sprintf(szPayload, "[{\"uid\": \"%s\", \"status\" : \"ERROR\"", szUid);
    }

    if (strlen(szMessage) > 0)
    {
        sprintf(szPayload, "%s, \"message\" : \"%s\"}]", szPayload, szMessage);
    }
    else
    {
        sprintf(szPayload, "%s}]", szPayload);
    }

    //LE_INFO("[ACK Message] %s\n", szPayload);

    MQTTMessage     msg;
    msg.qos = g_stContext.u32QoS; //QOS0;
    msg.retained = 0;
    msg.dup = 0;
    msg.id = 0;
    msg.payload = szPayload;
    msg.payloadlen = strlen(szPayload);

    char* pTopic = malloc(strlen(TOPIC_NAME_ACK) + strlen(g_stContext.szDeviceId) + 1);
    sprintf(pTopic, "%s%s", g_stContext.szDeviceId, TOPIC_NAME_ACK);
    LE_INFO("Publish on %s\n", pTopic);
    LE_INFO("[ACK Message] %s\n", szPayload);

    PrintMessage("Published ACK - %s\n", pTopic);

    int rc = MQTTPublish(&g_stContext.oMqttClient, pTopic, &msg);
    if (rc == 0)
    {
        LE_INFO("publish OK: %d\n", rc);
    }
    else
    {
        LE_INFO("publish error: %d\n", rc);
    }

    if (pTopic)
    {
        free(pTopic);
    }
    if (szPayload)
    {
        free(szPayload);
    }

    return rc;
}

//--------------------------------------------------------------------------------------------------
/**
 * Send Mqtt incoming message event
 */
//--------------------------------------------------------------------------------------------------
static void SendMqttIncomingMessageEvent
(
    char*       szTopicName,
    char*       szKeyName,
    char*       szValue,
    char*       szTimestamp
)
{
    // Init the event data
    MqttInMsgData_t eventData;
    memset(&eventData, 0, sizeof(eventData));
    strcpy(eventData.szTopicName, szTopicName);
    strcpy(eventData.szKeyName, szKeyName);
    strcpy(eventData.szValue, szValue);
    strcpy(eventData.szTimestamp, szTimestamp);

    LE_DEBUG("Reporting MQTT incoming message: %s, (%s:%s@%s)", eventData.szTopicName, eventData.szKeyName, eventData.szValue, eventData.szTimestamp);

    // Send the event to interested applications
    le_event_Report(MqttInMsgEvent, &eventData, sizeof(eventData));
}

static void onIncomingMessage(MessageData* md)
{
    /*
        This is a callback function (handler), invoked by MQTT client whenever there is an incoming message
        It performs the following actions :
          - deserialize the incoming MQTT JSON-formatted message
          - call convertDataToCSV()
    */

    MQTTMessage* message = md->message;
    char        szTopicName[128] = {0};

    memcpy(szTopicName, md->topicName->lenstring.data, md->topicName->lenstring.len);
    szTopicName[md->topicName->lenstring.len] = 0;
    LE_INFO("\nReceived message from topic: %s", szTopicName);

    LE_INFO("\nIncoming data:\n%.*s%s\n", (int)message->payloadlen, (char*)message->payload, " ");

    char* szPayload = (char *) malloc(message->payloadlen+1);

    memcpy(szPayload, (char*)message->payload, message->payloadlen);
    szPayload[message->payloadlen] = 0;

    //decode JSON payload

    char* pszCommand = swirjson_getValue(szPayload, -1, "command");
    if (pszCommand)
    {
        char*   pszUid = swirjson_getValue(szPayload, -1, "uid");
        char*   pszTimestamp = swirjson_getValue(szPayload, -1, "timestamp");
        char*   pszId = swirjson_getValue(pszCommand, -1, "id");
        char*   pszParam = swirjson_getValue(pszCommand, -1, "params");

        int     i;

        for (i=0; i<AV_JSON_KEY_MAX_COUNT; i++)
        {
            char szKey[AV_JSON_KEY_MAX_LENGTH];

            char * pszValue = swirjson_getValue(pszParam, i, szKey);

            if (pszValue)
            {
                LE_INFO("--> Incoming message from AirVantage: %s.%s = %s @ %s", pszId, szKey, pszValue, pszTimestamp);

                PrintMessage("--> Incoming message from AirVantage: %s.%s = %s @ %s", pszId, szKey, pszValue, pszTimestamp);

                char szFullKey[128];

                sprintf(szFullKey, "%s.%s", pszId, szKey);
                SendMqttIncomingMessageEvent(szTopicName, szFullKey, pszValue, pszTimestamp);

                sprintf(szPayload, "%s.%s = %s @ %s", pszId, szKey, pszValue, pszTimestamp);
                QueueIncomingMessage(szPayload);

                free(pszValue);
            }
            else
            {
                break;
            }
        }

        publishAckCmd(pszUid, 0, "");

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

    if (szPayload)
    {
        free(szPayload);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Send Mqtt connection state event
 */
//--------------------------------------------------------------------------------------------------
static void SendMqttConnStateEvent
(
    bool        bIsConnected,
    int32_t     nConnectErrorCode,
    int32_t     nSubErrorCode
)
{
    // Init the event data
    MqttConnStateData_t eventData;
    eventData.bIsConnected = bIsConnected;
    eventData.nConnectErrorCode = nConnectErrorCode;
    eventData.nSubErrorCode = nSubErrorCode;

    LE_DEBUG("Reporting MQTT state[%i], %d, %d", eventData.bIsConnected, eventData.nConnectErrorCode, eventData.nSubErrorCode);

    // Send the event to interested applications
    le_event_Report(MqttConnStateEvent, &eventData, sizeof(eventData));
}

//--------------------------------------------------------------------------------------------------
/**
 *  Handle MQTT message
 */
//--------------------------------------------------------------------------------------------------
static int InitMqtt
(
    void
)
{
    int     rc = 0;

    NewNetwork(&g_stContext.oNetwork);
    ConnectNetwork(&g_stContext.oNetwork, g_stContext.szBrokerUrl, g_stContext.u32PortNumber);
    MQTTClient(&g_stContext.oMqttClient, &g_stContext.oNetwork, TIMEOUT_MS, g_buf, sizeof(g_buf), g_readbuf, sizeof(g_readbuf));
 
    LE_INFO("Connecting to connect to tcp://%s:%d\n", g_stContext.szBrokerUrl, g_stContext.u32PortNumber);
    PrintMessage("Connecting to connect to tcp://%s:%d\n", g_stContext.szBrokerUrl, g_stContext.u32PortNumber);

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;       
    data.willFlag = 0;
    data.MQTTVersion = MQTT_VERSION;

    if (strlen(g_stContext.szDeviceId) > 0)
    {
        data.clientID.cstring = g_stContext.szDeviceId;
        data.username.cstring = g_stContext.szDeviceId;
        data.password.cstring = g_stContext.szSecret;
        LE_INFO("   Using deviceID= %s, pwd= %s", g_stContext.szDeviceId, g_stContext.szSecret);
    }

    data.keepAliveInterval = g_stContext.u32KeepAlive;
    data.cleansession = 1;
    
    rc = MQTTConnect(&g_stContext.oMqttClient, &data);

    LE_INFO("MQTT connection status= %d\n", rc);
    if (g_stContext.oMqttClient.isconnected)
    {
        LE_INFO("MQTT connected");
    }

    if (rc == SUCCESS) 
    {
        //connected
        LE_INFO("MQTT connected...");

        
        LE_INFO("Subscribing to %s\n", g_stContext.szSubscribeTopic);
        rc = MQTTSubscribe(&g_stContext.oMqttClient, g_stContext.szSubscribeTopic, 0, onIncomingMessage);
        LE_INFO("Subscription return code: %d\n", rc);

        if (rc != SUCCESS)
        {
            sleep(2);
            LE_INFO("Reattempt to Subscribe to %s\n", g_stContext.szSubscribeTopic);
            rc = MQTTSubscribe(&g_stContext.oMqttClient, g_stContext.szSubscribeTopic, 0, onIncomingMessage);
            LE_INFO("Subscription return code: %d\n", rc);
        }

        if (rc == SUCCESS)
        {
            g_eState = MQTT_CONNECTED;

            LE_INFO("Starting Timer for Mqtt Yield: %d seconds", YIELD_INTERVAL_SECOND);
            le_timer_Ref_t timerRef = le_timer_Create("timerApp");
            LE_FATAL_IF(timerRef == NULL, "timerApp timer ref is NULL");

            le_clk_Time_t   interval = { YIELD_INTERVAL_SECOND, 0 };
            le_result_t     res;

            res = le_timer_SetInterval(timerRef, interval);
            LE_FATAL_IF(res != LE_OK, "set interval to %lu seconds: %d", interval.sec, res);

            res = le_timer_SetRepeat(timerRef, 1);
            LE_FATAL_IF(res != LE_OK, "set repeat to once: %d", res);

            le_timer_SetHandler(timerRef, timerHandler);

            StartTimer(timerRef);

            SendMqttConnStateEvent(true, 0, rc);

            PrintMessage("MQTT connected");

            return 0;
        }
        else
        {
            LE_INFO("MQTT disconnect");
            MQTTDisconnect(&g_stContext.oMqttClient);

            SendMqttConnStateEvent(false, rc, -1);

            PrintMessage("MQTT Disconnected");

            g_eState = MQTT_DISCONNECTED;
        }
    }
    else
    {
        LE_INFO("MQTT connection Failed");

        SendMqttConnStateEvent(false, rc, -1);

        PrintMessage("MQTT connection Failed");

        g_eState = MQTT_DISCONNECTED;
    }

    return 1;
}


//--------------------------------------------------------------------------------------------------
/**
 *  Event callback for data connection state changes.
 */
//--------------------------------------------------------------------------------------------------
static void DcsStateHandler
(
    const char* intfName,
    bool        isConnected,
    void*       contextPtr
)
{
    if (isConnected)
    {
        if (g_eState == CONNECTING)
        {
            g_eState = DATA_CONNECTED;

            #if 0
            if (g_stContext.oMqttClient.isconnected)
            {
                LE_INFO("MQTT session already established");
                return;
            }
            #endif

            PrintMessage("DSC connected... starting MQTT connection");

            LE_INFO("%s connected! Starting MQTT session", intfName);
            
            if (InitMqtt())
            {
                DisconnectData();
                LE_INFO("Failed to open MQTT session, close data connection");
            }
        }
        else
        {
            LE_INFO("No ongoing MQTT Connection request");

            PrintMessage("DSC connected... no ongoing MQTT request");
        }
    }
    else
    {
        LE_INFO("%s disconnected!", intfName);

        PrintMessage("DSC Disconnected");

        DisconnectData();
    }
}

void mqtt_ViewConfig
(
    void
)
{
    PrintMessage("Current MQTT Setting is:\nbroker : %s\nport : %d\nkalive : %d\nqos : %d\nusername : %s\npassword : %s\n",
                                g_stContext.szBrokerUrl,
                                g_stContext.u32PortNumber,
                                g_stContext.u32KeepAlive,
                                g_stContext.u32QoS,
                                g_stContext.szDeviceId,
                                g_stContext.szSecret);
}

void mqtt_Config
(
    const char*     szBrokerUrl,
    int32_t         n32PortNumber,
    int32_t         n32KeepAlive,
    int32_t         n32QoS,
    const char*     szUserName,
    const char*     szPassword
)
{
    if (strlen(szBrokerUrl) > 0)
    {
        PrintMessage("Previous MQTT Broker URL was: %s", g_stContext.szBrokerUrl);
        strcpy(g_stContext.szBrokerUrl, szBrokerUrl);
        PrintMessage("New MQTT Broker URL is now: %s", g_stContext.szBrokerUrl); 
    }

    if (n32PortNumber != -1)
    {
        PrintMessage("Previous MQTT Broker Port was: %lu", (long unsigned int) g_stContext.u32PortNumber);
        g_stContext.u32PortNumber = n32PortNumber;
        PrintMessage("New MQTT Broker is now: %lu", (long unsigned int) g_stContext.u32PortNumber);
    }

    if (n32KeepAlive != -1)
    {
        PrintMessage("Previous Keep Alive was: %lu seconds", (long unsigned int) g_stContext.u32KeepAlive);
        g_stContext.u32KeepAlive = n32KeepAlive;
        PrintMessage("New Keep Alive is now: %lu seconds", (long unsigned int) g_stContext.u32KeepAlive);
    } 

    if (n32QoS != -1)
    {
        PrintMessage("Previous QoS was: %lu", (long unsigned int) g_stContext.u32QoS);
        g_stContext.u32QoS = n32QoS;
        PrintMessage("New QoS is now: %lu", (long unsigned int) g_stContext.u32QoS);
    }

    if (strlen(szUserName) > 0)
    {
        PrintMessage("Previous username was: %s", g_stContext.szDeviceId);
        strcpy(g_stContext.szDeviceId, szUserName);
        PrintMessage("New username is now: %s", g_stContext.szDeviceId);
    }

    if (strlen(szPassword) > 0)
    {
        PrintMessage("Previous password was: %s", g_stContext.szSecret);
        strcpy(g_stContext.szSecret, szPassword);
        PrintMessage("New password is now: %s", g_stContext.szSecret);
    }

    PrintMessage("New settings will be applied at the next connection");
}

void mqtt_Connect
(
)
{
    if (g_eState == MQTT_CONNECTED)
    {
        LE_INFO("\n*** MQTT session already active ***\n");
        SendMqttConnStateEvent(true, 0, 0);

        PrintMessage("MQTT session already active");
        return;
    }

    if (g_eState == IDLE)
    {
        LE_INFO("Idle, call Connect()");

        LE_INFO("Initiated Data Connection");
        ConnectData();
    }
    else
    {
        
        LE_INFO("Already Connecting, try later");
        SendMqttConnStateEvent(false, 1, -1);

        PrintMessage("MQTT session is ongoing, try later");
    }
}

void mqtt_Disconnect
(
    void
)
{
    DisconnectData();
}

//--------------------------------------------------------------------------------------------------
/**
 *  Send MQTT message.
 */
//--------------------------------------------------------------------------------------------------
void mqtt_Send
(
    const char*  szKey, ///< [IN] Key
    const char*  szValue, ///< [IN] Value
    int32_t*     pi32ReturnCode ///< [OUT] errCode
)
{
    //if (!g_stContext.oMqttClient.isconnected)
    if (g_eState != MQTT_CONNECTED)
    {
        LE_INFO("There is no active MQTT session, please open a session using mqttClient connect");
        return;
    }

    char* szPayload = swirjson_szSerialize(szKey, szValue, 0);

    MQTTMessage     msg;

    msg.qos = g_stContext.u32QoS; //QOS0;
    msg.retained = 0;
    msg.dup = 0;
    msg.id = 0;
    msg.payload = szPayload;
    msg.payloadlen = strlen(szPayload);

    char* pTopic = malloc(strlen(TOPIC_NAME_PUBLISH) + strlen(g_stContext.szDeviceId) + 1);
    sprintf(pTopic, "%s%s", g_stContext.szDeviceId, TOPIC_NAME_PUBLISH);
    LE_INFO("Publish on %s\n", pTopic);
    LE_INFO("MQTT message %s\n", szPayload);

    int rc = MQTTPublish(&g_stContext.oMqttClient, pTopic, &msg);
    if (rc == 0)
    {
        LE_INFO("publish OK: %d\n", rc);
    }
    else
    {
        LE_INFO("publish error: %d\n", rc);
    }

    if (pTopic)
    {
        free(pTopic);
    }

    if (szPayload)
    {
        free(szPayload);
    }
    
    *pi32ReturnCode = rc;
}


//--------------------------------------------------------------------------------------------------
/**
 * The first-layer Session State Handler
 *
 */
//--------------------------------------------------------------------------------------------------
static void FirstLayerSessionStateHandler
(
    void* reportPtr,
    void* secondLayerHandlerFunc
)
{
    MqttConnStateData_t* eventDataPtr = reportPtr;
    mqtt_SessionStateHandlerFunc_t clientHandlerFunc = secondLayerHandlerFunc;

    clientHandlerFunc(        
                      eventDataPtr->bIsConnected,
                      eventDataPtr->nConnectErrorCode,
                      eventDataPtr->nSubErrorCode,
                      le_event_GetContextPtr());
}

//--------------------------------------------------------------------------------------------------
/**
 * This function adds a handler ...
 */
//--------------------------------------------------------------------------------------------------
mqtt_SessionStateHandlerRef_t mqtt_AddSessionStateHandler
(
    mqtt_SessionStateHandlerFunc_t   handlerPtr,
    void*                               contextPtr
)
{
    LE_DEBUG("%p", handlerPtr);
    LE_DEBUG("%p", contextPtr);

    le_event_HandlerRef_t handlerRef = le_event_AddLayeredHandler(
                                                    "MqttConnState",
                                                    MqttConnStateEvent,
                                                    FirstLayerSessionStateHandler,
                                                    (le_event_HandlerFunc_t)handlerPtr);

    le_event_SetContextPtr(handlerRef, contextPtr);

    return (mqtt_SessionStateHandlerRef_t)(handlerRef);
}


//--------------------------------------------------------------------------------------------------
/**
 * This function removes a handler ...
 */
//--------------------------------------------------------------------------------------------------
void mqtt_RemoveSessionStateHandler
(
    mqtt_SessionStateHandlerRef_t addHandlerRef
)
{
    LE_DEBUG("%p", addHandlerRef);

    le_event_RemoveHandler((le_event_HandlerRef_t)addHandlerRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * The first-layer Session State Handler
 *
 */
//--------------------------------------------------------------------------------------------------
static void FirstLayerIncomingMessageHandler
(
    void* reportPtr,
    void* secondLayerHandlerFunc
)
{
    MqttInMsgData_t*                        eventDataPtr = reportPtr;
    mqtt_IncomingMessageHandlerFunc_t    clientHandlerFunc = secondLayerHandlerFunc;

    clientHandlerFunc(        
                      eventDataPtr->szTopicName,
                      eventDataPtr->szKeyName,
                      eventDataPtr->szValue,
                      eventDataPtr->szTimestamp,
                      le_event_GetContextPtr());
}

//--------------------------------------------------------------------------------------------------
/**
 * This function adds a handler ...
 */
//--------------------------------------------------------------------------------------------------
mqtt_IncomingMessageHandlerRef_t mqtt_AddIncomingMessageHandler
(
    mqtt_IncomingMessageHandlerFunc_t   handlerPtr,
    void*                                  contextPtr
)
{
    LE_DEBUG("%p", handlerPtr);
    LE_DEBUG("%p", contextPtr);

    le_event_HandlerRef_t handlerRef = le_event_AddLayeredHandler(
                                                    "MqttIncomingMessage",
                                                    MqttInMsgEvent,
                                                    FirstLayerIncomingMessageHandler,
                                                    (le_event_HandlerFunc_t)handlerPtr);

    le_event_SetContextPtr(handlerRef, contextPtr);

    return (mqtt_IncomingMessageHandlerRef_t)(handlerRef);
}


//--------------------------------------------------------------------------------------------------
/**
 * This function removes a handler ...
 */
//--------------------------------------------------------------------------------------------------
void mqtt_RemoveIncomingMessageHandler
(
    mqtt_IncomingMessageHandlerRef_t addHandlerRef
)
{
    LE_DEBUG("%p", addHandlerRef);

    le_event_RemoveHandler((le_event_HandlerRef_t)addHandlerRef);
}

//--------------------------------------------------------------------------------------------------
le_result_t HandleReceivedCommand(char * response, size_t responseSize)
{
    le_result_t   ret = LE_OK;

    ST_MQTT_INCOMING_MSG_T*     topNodePtr = NULL;
    le_sls_Link_t*              linkPtr = NULL;
    size_t                      byteCount = 0;

    linkPtr = le_sls_Pop(&g_inMessageList);

    response[0] = 0;

    while (linkPtr)
    {
        topNodePtr = CONTAINER_OF(linkPtr, ST_MQTT_INCOMING_MSG_T, link);

        strcat(response, topNodePtr->message);
        strcat(response, "\n");
        byteCount += strlen(topNodePtr->message) + 1;

        le_mem_Release(topNodePtr);

        if (byteCount >= responseSize)
        {
            break;
        }

        linkPtr = le_sls_Pop(&g_inMessageList);
    };

    return ret;
}

//--------------------------------------------------------------------------------------------

le_result_t HandleConfigCommand(int argIndex, char argument[MAX_ARGS][MAX_ARG_LENGTH], char* response, size_t responseSize)
{
    le_result_t  ret = LE_BAD_PARAMETER;

    if (strcasecmp("get", argument[argIndex])==0)
    {
        //command format: config get

        sprintf(response, "broker : %s\nport : %d\nkalive : %d\nqos : %d\nusername : %s\npassword : %s\n",
                                g_stContext.szBrokerUrl,
                                g_stContext.u32PortNumber,
                                g_stContext.u32KeepAlive,
                                g_stContext.u32QoS,
                                g_stContext.szDeviceId,
                                g_stContext.szSecret);

        return LE_OK;
    }
    else if (strcasecmp("set", argument[argIndex])==0)
    {
        if (strcasecmp("broker", argument[argIndex+1])==0)
        {
            strcpy(g_stContext.szBrokerUrl, argument[argIndex+2]);
            ret = LE_OK;
        }
        else if (strcasecmp("port", argument[argIndex+1])==0)
        {
            g_stContext.u32PortNumber = atoi( argument[argIndex+2]);
            ret = LE_OK;
        }
        else if (strcasecmp("kalive", argument[argIndex+1])==0)
        {
            g_stContext.u32KeepAlive = atoi( argument[argIndex+2]);
            ret = LE_OK;
        }
        else if (strcasecmp("qos", argument[argIndex+1])==0)
        {
            g_stContext.u32QoS = atoi( argument[argIndex+2]);
            ret = LE_OK;
        }
        else if (strcasecmp("username", argument[argIndex+1])==0)
        {
            strcpy(g_stContext.szDeviceId, argument[argIndex+2]);
            ret = LE_OK;
        }
        else if (strcasecmp("password", argument[argIndex+1])==0)
        {
            strcpy(g_stContext.szSecret, argument[argIndex+2]);
            ret = LE_OK;
        }
        
        if (LE_OK == ret)
        {
            PrintMessage("config: config changed");
        }
        else
        {
            PrintMessage("config: failed to change config");   
        }
    }

    return ret;
}


le_result_t HandleSessionCommand(int argIndex, char argument[MAX_ARGS][MAX_ARG_LENGTH], char* response, size_t responseSize)
{
    // Command format : session start/stop

    le_result_t     ret = LE_NOT_FOUND;

    if (strcasecmp("start", argument[argIndex])==0)
    {
        mqtt_Connect();
        ret = LE_OK;
    }
    else if (strcasecmp("stop", argument[argIndex])==0)
    {
        mqtt_Disconnect();
        ret = LE_OK;
    }
    else if (strcasecmp("status", argument[argIndex])==0)
    {
        if (g_stContext.oMqttClient.isconnected)
        {
            sprintf(response, "session : started");
        }
        else
        {
            sprintf(response, "session : stopped");
        }

        ret = LE_OK;
        PrintMessage(response);
    }
    else
    {
        PrintMessage("session : invalid command");
    }

    return ret;
}

le_result_t HandleSendCommand(int argIndex, char argument[MAX_ARGS][MAX_ARG_LENGTH], char * response, size_t responseSize)
{
    le_result_t     ret = LE_BAD_PARAMETER;

    //command format: send <path> [<value>]
    if (strlen(argument[argIndex+1]) > 0)
    {
        int rc;

        mqtt_Send(argument[argIndex], argument[argIndex+1], &rc);

        if (rc)
        {
            PrintMessage("send: failed to send data");
            ret = LE_FAULT;
        }
        else
        {
            
            PrintMessage("send : sent OK");
            ret = LE_OK;
        }
    }
    else
    {
        PrintMessage("send : bad_parameter");
    }

    return ret;
}

void mqtt_ExecuteCommand
(
    const char* userCommand,
    int32_t*    returnCodePtr,
    char*       response,            ///< [OUT] Retrieved string
    size_t      responseSize         ///< [IN] String buffer size in bytes
)
{
    *returnCodePtr = LE_NOT_FOUND;

    LE_INFO("MQTT_ExecuteCommand");

    char  argument[MAX_ARGS][MAX_ARG_LENGTH];
    int   argCount = 0;

    argCount = decodeArgument(userCommand, argument);

    response[0] = 0;
            
    if (argCount > 0)
    {
        if (strcasecmp("quit", argument[0])==0)
        {
            mqtt_Disconnect();

            *returnCodePtr = LE_OK;

            exit(EXIT_SUCCESS);
        }
        else if (strcasecmp("config", argument[0])==0)
        {
            *returnCodePtr = HandleConfigCommand(1, argument, response, responseSize);
        }
        else if (strcasecmp("session", argument[0])==0)
        {
            *returnCodePtr = HandleSessionCommand(1, argument, response, responseSize);
        }
        else if (strcasecmp("send", argument[0])==0)
        {
            *returnCodePtr = HandleSendCommand(1, argument, response, responseSize);
        }
        
        else if (strcasecmp("received", argument[0])==0)
        {
            *returnCodePtr = HandleReceivedCommand(response, responseSize);
        }
        else
        {
            puts("invalid command");       
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 *  Main function.
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    LE_INFO("---------- Launching mqttClient ----------");

    MqttConnStateEvent = le_event_CreateId("Mqtt Conn State", sizeof(MqttConnStateData_t));
    MqttInMsgEvent  = le_event_CreateId("Mqtt InMsg", sizeof(MqttInMsgData_t));


    //Set default mqtt broker to be AirVantage
    strcpy(g_stContext.szBrokerUrl, URL_AIRVANTAGE_SERVER);
    g_stContext.u32PortNumber = PORT_AIRVANTAGE_SERVER;

    g_stContext.u32KeepAlive = DEFAULT_KEEP_ALIVE;
    g_stContext.u32QoS       = DEFAULT_QOS;

    strcpy(g_stContext.szSecret, "sierra");
    strcpy(g_stContext.szKey, "");
    strcpy(g_stContext.szValue, "");

    le_info_ConnectService();
    le_info_GetImei(g_stContext.szDeviceId, sizeof(g_stContext.szDeviceId));
    //le_info_GetPlatformSerialNumber(g_stContext.szDeviceId, sizeof(g_stContext.szDeviceId));
    //strcpy(g_stContext.szDeviceId, "00000000B6AF4AFF"); //default

    sprintf(g_stContext.szSubscribeTopic, "%s%s", g_stContext.szDeviceId, TOPIC_NAME_SUBSCRIBE);

    LE_INFO("mqttClient Launched, IMEI= %s", g_stContext.szDeviceId);

    g_eState = IDLE;

    PrintMessage("MQTT Client Service started:");

    int32_t nRet = 0;
    char    response[512] = {0};
    mqtt_ExecuteCommand("config get", &nRet, response, sizeof(response));
    
    if (strlen(response))
    {
        PrintMessage(response);
    }
}

