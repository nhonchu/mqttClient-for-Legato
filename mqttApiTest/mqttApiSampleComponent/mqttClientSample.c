//--------------------------------------------------------------------------------------------------
/**
 * @file mqttClientSample.c
 *
 * This sample makes use of mqttClient API over IPC, to start/stop mqttClient and to send mqtt messages to AirVantage.
 *
 * Nhon Chu
 *
 *
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"


#define     YIELD_INTERVAL_SECOND           15

mqttClient_InstanceRef_t             _cliMqttRef = NULL;

le_timer_Ref_t                       _timerRef = NULL;

le_timer_Ref_t                       _pubTimerRef = NULL;

le_data_RequestObjRef_t              _DataRequestRef = NULL;
le_data_ConnectionStateHandlerRef_t  _DataConnectionStateHandlerRef = NULL;

char                                 _broker[128] = {0};
int32_t                              _portNumber = 8883;
int32_t                              _useTLS = 1;
char                                 _deviceId[] = "legatoMqttClient1234";
char                                 _secret[] = "mySecret";
int32_t                              _keepAlive;
int32_t                              _qoS;

int                                  _count = 0;


//--------------------------------------------------------------------------------------------------
/**
 *  Helper to display log
 */
//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
/**
 *  The opposite of Connect, this function will tear down the data connection.
 */
//--------------------------------------------------------------------------------------------------
static void Disconnect(bool quitApp)
{
    if (_cliMqttRef)
    {
        LE_INFO("Delete MQTT instance");
        mqttClient_Delete(_cliMqttRef);
        _cliMqttRef = NULL;
    }

    
    if (_DataConnectionStateHandlerRef)
    {
        le_data_RemoveConnectionStateHandler(_DataConnectionStateHandlerRef);
        _DataConnectionStateHandlerRef = NULL;
    }
    
    if (_DataRequestRef)
    {
        LE_INFO("Releasing the data connection.");
        le_data_Release(_DataRequestRef);

        _DataRequestRef = NULL;
    }

    if (quitApp)
    {
        exit(EXIT_SUCCESS);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  Event callback for MQTT-server originated Commands
 */
//--------------------------------------------------------------------------------------------------
static void OnIncomingMessage(
                const char* topicName,
                const char* key,    //could be empty
                const char* value,  //aka payload
                const char* timestamp,  //could be empty
                void*       pUserContext)
{
    PrintMessage("Received message from topic %s:", topicName);
    PrintMessage("   Message timestamp epoch: %s", timestamp);
    PrintMessage("   Parameter Name: %s", key);
    PrintMessage("   Parameter Value: %s", value);


    char szPayload[256] = {0};
    sprintf(szPayload, "%s = %s @ %s", key, value, timestamp);

    PrintMessage("Received message from MQTT broker, now terminate the sample app");
    Disconnect(true);
}


//--------------------------------------------------------------------------------------------------
/**
 *  Timer Handler
 */
//--------------------------------------------------------------------------------------------------
static void pubTimerHandler
(
    le_timer_Ref_t  timerRef
)
{
    if (mqttClient_IsConnected(_cliMqttRef))
    {
        LE_INFO("Publish data");
        char data[8] = {0};
        sprintf(data, "%d", _count++);
        mqttClient_PublishKeyValue(_cliMqttRef, "count", data, "");
        le_timer_Start(_pubTimerRef);
    }    
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
        PrintMessage("DSC connected... starting MQTT connection");

        LE_INFO("%s connected! Starting MQTT session", intfName);
        
        if (LE_OK == mqttClient_StartSession(_cliMqttRef))
        {
            PrintMessage("Let's wait for incoming command to quit the app");

            mqttClient_AddIncomingMessageHandler(_cliMqttRef, OnIncomingMessage, NULL);

            mqttClient_Subscribe(_cliMqttRef, "legatoApp");

            le_timer_Start(_timerRef);
            pubTimerHandler(_pubTimerRef);
        }
        else
        {
            PrintMessage("Failed to start MQTT session with AirVantage, let's try iot.eclipse.org");

            mqttClient_Delete(_cliMqttRef);

            _cliMqttRef = mqttClient_Create("iot.eclipse.org", 8883, 1, _deviceId, _secret, 30, 0);

            if (LE_OK == mqttClient_StartSession(_cliMqttRef))
            {
                PrintMessage("Let's wait for incoming command to quit the app");

                mqttClient_AddIncomingMessageHandler(_cliMqttRef, OnIncomingMessage, NULL);

                mqttClient_Subscribe(_cliMqttRef, "legatoApp");

                le_timer_Start(_timerRef);
                pubTimerHandler(_pubTimerRef);
            }
            else
            {
                PrintMessage("Failed to start MQTT session iot.eclipse.org, now exit app");
                Disconnect(true);
            }
        }
    }
}



//--------------------------------------------------------------------------------------------------
/**
 *  This function will request the data connection
 */
//--------------------------------------------------------------------------------------------------
static void Connect
(
    void
)
{
    if (_cliMqttRef == NULL)
    {
        //dat aconnection might be already exists, so it is important to create the MQTT instance first
        //Get default config, default is AirVantage server
        mqttClient_GetConfig(NULL, _broker, sizeof(_broker), &_portNumber, &_useTLS, _deviceId, sizeof(_deviceId), _secret, sizeof(_secret), &_keepAlive, &_qoS);

        LE_INFO("Create MQTT instance");
        _cliMqttRef = mqttClient_Create(_broker, _portNumber, _useTLS, _deviceId, _secret, _keepAlive, _qoS);
    }

    // register handler for data connection state change
    if (!_DataConnectionStateHandlerRef)
    {
        _DataConnectionStateHandlerRef = le_data_AddConnectionStateHandler(DcsStateHandler, NULL);
        LE_INFO("Registered for data connection state : %p.", _DataConnectionStateHandlerRef);
    }

    if (!_DataRequestRef)
    {
        _DataRequestRef = le_data_Request();
        LE_INFO("Requesting the data connection: %p.", _DataRequestRef);
    }
}



//--------------------------------------------------------------------------------------------------
/**
 *  Timer Handler
 */
//--------------------------------------------------------------------------------------------------
static void timerHandler
(
    le_timer_Ref_t  timerRef
)
{
    if (mqttClient_IsConnected(_cliMqttRef))
    {
        LE_INFO("MQTT yield");
        if (LE_FAULT == mqttClient_ProcessEvent(_cliMqttRef))
        {
            //MQTT connection might be lost
            //Start over again
            LE_INFO("MQTT connection Lost - Reconnecting");
            Disconnect(false);
            Connect();
        }
        else
        {
            le_timer_Start(_timerRef);
        }
    }
    else
    {
        LE_INFO("No active MQTT session, now stop Timer");
        le_timer_Stop(_timerRef);
    }
    
}



//--------------------------------------------------------------------------------------------------
/**
 * App init.
 *
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    //Create timer to call mqttClient_ProcessEvent
    _timerRef = le_timer_Create("mqttClientSampleTimer");
    LE_FATAL_IF(_timerRef == NULL, "timerApp timer ref is NULL");

    le_clk_Time_t   interval = { YIELD_INTERVAL_SECOND, 0 };
    le_result_t     res;

    res = le_timer_SetInterval(_timerRef, interval);
    LE_FATAL_IF(res != LE_OK, "set interval to %lu seconds: %d", interval.sec, res);

    res = le_timer_SetRepeat(_timerRef, 1);
    LE_FATAL_IF(res != LE_OK, "set repeat to once: %d", res);

    le_timer_SetHandler(_timerRef, timerHandler);


    //Create timer to publish data
    _pubTimerRef = le_timer_Create("mqttSamplePubTimer");
    LE_FATAL_IF(_pubTimerRef == NULL, "timerApp timer ref is NULL");

    le_clk_Time_t   interval2 = { 30, 0 };

    res = le_timer_SetInterval(_pubTimerRef, interval2);
    LE_FATAL_IF(res != LE_OK, "set interval to %lu seconds: %d", interval.sec, res);

    res = le_timer_SetRepeat(_pubTimerRef, 1);
    LE_FATAL_IF(res != LE_OK, "set repeat to once: %d", res);

    le_timer_SetHandler(_pubTimerRef, pubTimerHandler);

    LE_INFO("mqttClientSample app started");
    LE_INFO("To quit the app, send a message from AirVantage to this device");

    Connect();
}
