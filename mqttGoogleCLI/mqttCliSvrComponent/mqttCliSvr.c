/**
 * This module implements MQTT Command Line Interface and is relying on mqttClient API
 * The default broker is Sierra Wireless AirVantage server
 *
 * 
 * Nhon Chu
 *
 *
 */

/* Legato Framework */
#include "legato.h"
#include "interfaces.h"


#define     YIELD_INTERVAL_SECOND       15

#define     MAX_ARGS                    10
#define     MAX_ARG_LENGTH              48
#define     MAX_INMESSAGE_IN_LIST       20

mqttClient_InstanceRef_t    _cliMqttRef = NULL;
le_timer_Ref_t              _timerRef = NULL;

// this is used to store incoming message for commandnd line mode (instance 0)
static le_sls_List_t        _inMessageList = LE_SLS_LIST_INIT;
static le_mem_PoolRef_t     _inMessagePool = NULL;

typedef struct
{
    le_sls_Link_t   link;
    char            message[256];
} ST_MQTT_INCOMING_MSG_T;


//MQTT Client config
char                                 _broker[] = "mqtt.googleapis.com";
int32_t                              _portNumber = 8883;
int32_t                              _useTLS = 1;
char                                 _deviceId[256] = {0};
char                                 _username[128] = {0};
char                                 _secret[512] = {0};
int32_t                              _keepAlive = 30;
int32_t                              _qoS = 0;

//Google Cloud MQTT project Config
char                                 _gProjectId[256] = {0};
char                                 _gLocation[64] = {0};
char                                 _gRegistry[64] = {0};
char                                 _gDeviceId[64] = {0};
char                                 _gRsaPrivateKeyFilename[128] = {0};
char                                 _gPublishTopicName[256] = {0};

//Intermediate files used to generate JWT, for step-by-step and debugging purpose
char                                _jwt_headerFile[] = "header.txt";   //for debugging JWT
char                                _jwt_claimFile[] = "claim.txt";
char                                _jwt_enc_headerFile[] = "enc_header.txt";   //for debugging JWT
char                                _jwt_enc_claimFile[] = "enc_claim.txt";
char                                _jwt_fullClaimFile[] = "tobesigned.txt";
char                                _jwt_binSignatureFile[] = "signature.bin";
char                                _jwt_signatureFile[] = "signature.txt";
char                                _jwt_JwtFile[] = "jwt.txt";

static le_data_RequestObjRef_t  _RequestRef = NULL;
static le_data_ConnectionStateHandlerRef_t  _hDataConnectionState = NULL;

static void DcsStateHandler(const char* intfName, bool        isConnected, void*       contextPtr);
static void OnIncomingMessage(
                const char* topicName,
                const char* key,
                const char* value,
                const char* timestamp,
                void*       pUserContext);



////////////////////////////////////////////////////////////////////////////////////////////////////
size_t WriteFile(const char * filename, const char * buffer, size_t bufferLen)
{
    FILE*           file = NULL;

    file = fopen(filename, "will");
    if (NULL == file)
    {
        return 0;
    }

    size_t written = fwrite(buffer, 1, bufferLen, file);

    fclose(file);

    return written;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
size_t ReadFile(const char* szFilename, char * buffer, size_t   bufferLen)
{
    FILE*           file = NULL;

    file = fopen(szFilename, "r");
    if (NULL == file)
    {
        return 0;
    }

    fseek(file, 0, SEEK_END);
    size_t fsize = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (bufferLen > fsize)
    {
        memset(buffer, 0, bufferLen);
        fread(buffer, 1, fsize, file);
        buffer[fsize] = 0;  //zero terminate the string
    }
    
    fclose(file);

    return fsize;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
size_t jwt_Create(const char * rsaPrivateKeyPemFile, const char * clientId, char* jwt, size_t jwtLen)
{
    //quick & dirty sample using openssl command line to show the different JWT steps. Your final solution should be using openssl EVP_* API functions

    char header[] = "{\"alg\":\"RS256\",\"typ\":\"JWT\"}";
    char claim[128] = {0};

    unsigned long epoch = time(NULL);
    sprintf(claim, "{\"aud\":\"%s\",\"iat\":%lu,\"exp\":%lu}", clientId, epoch, epoch + 86400); //valid for 24h

    remove(_jwt_headerFile);
    remove(_jwt_claimFile);
    remove(_jwt_enc_headerFile);
    remove(_jwt_enc_claimFile);
    remove(_jwt_fullClaimFile);
    remove(_jwt_binSignatureFile);
    remove(_jwt_signatureFile);
    remove(_jwt_JwtFile);

    WriteFile(_jwt_headerFile, header, strlen(header));

    WriteFile(_jwt_claimFile, claim, strlen(claim));

    char command[128];

    sprintf(command, "openssl enc -base64 -in %s | tr -d '\n' >> %s", _jwt_headerFile, _jwt_enc_headerFile);
    system(command);

    sprintf(command, "openssl enc -base64 -in %s | tr -d '\n' >> %s", _jwt_claimFile, _jwt_enc_claimFile);
    system(command);
    
    sprintf(command, "(cat %s ;echo -n '.';cat %s) >> %s", _jwt_enc_headerFile, _jwt_enc_claimFile, _jwt_fullClaimFile);
    system(command);

    sprintf(command, "openssl dgst -sha256 -sign %s -out %s %s", rsaPrivateKeyPemFile, _jwt_binSignatureFile, _jwt_fullClaimFile);
    system(command);

    sprintf(command, "openssl enc -base64 -in %s | tr -d '\n' >> %s", _jwt_binSignatureFile, _jwt_signatureFile);
    system(command);

    sprintf(command, "(cat %s ;echo -n '.';cat %s) >> %s", _jwt_fullClaimFile, _jwt_signatureFile, _jwt_JwtFile);
    system(command);

    return ReadFile(_jwt_JwtFile, jwt, jwtLen);
}

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

////////////////////////////////////////////////////////////////////////////////////////////////////
void GetConfig(char* config, size_t configLen)
{
    /*
    sprintf(config, "Current MQTT Client setting is :\n   broker : %s\n   port : %d\n   useTLS: %d\n   kalive : %d\n   qos : %d\n   clientId : %s\n   password : %s\n",
                                _broker,
                                _portNumber,
                                _useTLS,
                                _keepAlive,
                                _qoS,
                                _deviceId,
                                _secret);
    */
    sprintf(config, "Current Google MQTT setting is :\n   projectId : %s\n   location : %s\n   registry: %s\n   deviceId : %s\n   rsaPrivateKey : %s\n",
                                _gProjectId,
                                _gLocation,
                                _gRegistry,
                                _gDeviceId,
                                _gRsaPrivateKeyFilename);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
le_result_t       QueueIncomingMessage(const char* message)
{
    ST_MQTT_INCOMING_MSG_T*    newNodePtr = NULL;

    if (_inMessagePool == NULL)
    {
        _inMessagePool = le_mem_CreatePool("mqtt-incoming-message", sizeof(ST_MQTT_INCOMING_MSG_T));
        le_mem_ExpandPool(_inMessagePool, 20);
    }

    newNodePtr = le_mem_ForceAlloc(_inMessagePool);
    if (newNodePtr)
    {
        strcpy(newNodePtr->message, message);

        // Initialize the link.
        newNodePtr->link = LE_SLS_LINK_INIT;

        // Insert the new node to the tail.
        le_sls_Queue(&_inMessageList, &(newNodePtr->link));

        if (le_sls_NumLinks(&_inMessageList) > MAX_INMESSAGE_IN_LIST)
        {
            //discard the top one in the list (oldest)
            le_sls_Link_t*  linkPtr = le_sls_Pop(&_inMessageList);
            ST_MQTT_INCOMING_MSG_T* topNodePtr = CONTAINER_OF(linkPtr, ST_MQTT_INCOMING_MSG_T, link);
            le_mem_Release(topNodePtr);
        }

        PrintMessage("Incoming messages queued : %d", le_sls_NumLinks(&_inMessageList));
        return LE_OK;
    }

    return LE_FAULT;
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
    {   //create MQTT instance first, data connection might already exists
        
        
        sprintf(_deviceId, "projects/%s/locations/%s/registries/%s/devices/%s", _gProjectId, _gLocation, _gRegistry, _gDeviceId);

        jwt_Create(_gRsaPrivateKeyFilename, _gProjectId, _secret, sizeof(_secret));

        if (strlen(_secret) > 0)
        {
            LE_INFO("Create new MQTT instance");
            _cliMqttRef = mqttClient_Create(_broker, _portNumber, _useTLS, _deviceId, _username, _secret, _keepAlive, _qoS);

            mqttClient_AddIncomingMessageHandler(_cliMqttRef, OnIncomingMessage, NULL);
        }       
    }

    if (_cliMqttRef)
    {
        // register handler for data connection state change
        if (!_hDataConnectionState)
        {
            LE_INFO("Add handler for data connection status");
            _hDataConnectionState = le_data_AddConnectionStateHandler(DcsStateHandler, NULL);
        }

        if (!_RequestRef)
        {
            _RequestRef = le_data_Request();
            LE_INFO("Requesting the data connection: %p.", _RequestRef);
        }
    }
    else
    {
        PrintMessage("Failed to Create MQTT instance, please check Google project settings");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  The opposite of ConnectData, this function will tear down the data connection.
 */
//--------------------------------------------------------------------------------------------------
static void Disconnect
(
    void
)
{
    if (_cliMqttRef)
    {
        LE_INFO("Delete MQTT instance");
        mqttClient_Delete(_cliMqttRef);
        _cliMqttRef = NULL;
    }

    
    if (_hDataConnectionState)
    {
        le_data_RemoveConnectionStateHandler(_hDataConnectionState);
        _hDataConnectionState = NULL;
    }
    
    if (_RequestRef)
    {
        LE_INFO("Releasing the data connection.");
        le_data_Release(_RequestRef);

        _RequestRef = NULL;
    }
}


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
            Disconnect();
            Connect();
            return;
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
    sprintf(szPayload, "%s - ", topicName);
    if (strlen(key) > 0)
    {
        sprintf(szPayload, "%s%s : ", szPayload, key);
    }
    sprintf(szPayload, "%s%s", szPayload, value);
    if (strlen(timestamp) > 0)
    {
        sprintf(szPayload, "%s @ %s", szPayload, timestamp);
    }
    QueueIncomingMessage(szPayload);

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
            PrintMessage("MQTT session started");

            char    topicName[128] = {0};

            sprintf(topicName, "/devices/%s/config", _gDeviceId);

            int rc = mqttClient_Subscribe(_cliMqttRef, topicName);

            if (rc)
            {
                PrintMessage("Failed to subscribe to topic : %s", topicName);
            }
            else
            {
                
                PrintMessage("Subscribed successfully to topic : %s", topicName);
            }


            le_timer_Start(_timerRef);
        }
        else
        {
            PrintMessage("Failed to start MQTT session");
        }
    }
    else
    {
        LE_INFO("Data connection is closed, deleting MQTT instance");
        //restart the session if network issue
        mqttClient_StopSession(_cliMqttRef);
    }
}



//--------------------------------------------------------------------------------------------------
le_result_t HandleQueuedCommand(char * response, size_t responseSize)
{
    le_result_t   ret = LE_OK;

    ST_MQTT_INCOMING_MSG_T*     topNodePtr = NULL;
    le_sls_Link_t*              linkPtr = NULL;
    bool                        stop = false;

    linkPtr = le_sls_Pop(&_inMessageList);

    response[0] = 0;

    while (linkPtr)
    {
        topNodePtr = CONTAINER_OF(linkPtr, ST_MQTT_INCOMING_MSG_T, link);

        if ( (strlen(response) + strlen(topNodePtr->message) + 1) < responseSize)
        {
            strcat(response, topNodePtr->message);
            strcat(response, "\n");

            le_mem_Release(topNodePtr);
        }
        else
        {
            //no room to return this item, put it back to the list, on the top
            topNodePtr->link = LE_SLS_LINK_INIT;

            // Insert the new node to the tail.
            le_sls_Stack(&_inMessageList, &(topNodePtr->link));

            stop = true;
        }
        
        if (stop)
        {
            break;
        }

        linkPtr = le_sls_Pop(&_inMessageList);
    };

    return ret;
}

//--------------------------------------------------------------------------------------------

le_result_t HandleConfigCommand(int argIndex, char argument[MAX_ARGS][MAX_ARG_LENGTH], char* response, size_t responseSize)
{
    le_result_t  ret = LE_BAD_PARAMETER;

    if (strlen(argument[argIndex]) == 0)
    {
        GetConfig(response, responseSize);

        return LE_OK;
    }
    else if (strcasecmp("get", argument[argIndex])==0)
    {
        GetConfig(response, responseSize);

        return LE_OK;
    }
    else if (strcasecmp("set", argument[argIndex])==0)
    {
        if (strcasecmp("projectId", argument[argIndex+1])==0)
        {
            strcpy(_gProjectId, argument[argIndex+2]);
            ret = LE_OK;
        }
        else if (strcasecmp("location", argument[argIndex+1])==0)
        {
            strcpy(_gLocation, argument[argIndex+2]);
            ret = LE_OK;
        }
        else if (strcasecmp("registry", argument[argIndex+1])==0)
        {
            strcpy(_gRegistry, argument[argIndex+2]);
            ret = LE_OK;
        }
        else if (strcasecmp("deviceId", argument[argIndex+1])==0)
        {
            strcpy(_gDeviceId, argument[argIndex+2]);
            ret = LE_OK;
        }
        else if (strcasecmp("rsaPrivateKey", argument[argIndex+1])==0)
        {
            strcpy(_gRsaPrivateKeyFilename, argument[argIndex+2]);
            ret = LE_OK;
        }
        
        if (LE_OK == ret)
        {
            PrintMessage("config: config changed");

            sprintf(_gPublishTopicName, "/devices/%s/state", _gDeviceId);

            GetConfig(response, responseSize);
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
        Connect();
        ret = LE_OK;
    }
    else if (strcasecmp("stop", argument[argIndex])==0)
    {
        Disconnect();
        ret = LE_OK;
    }
    else if (strcasecmp("status", argument[argIndex])==0)
    {
        if (mqttClient_IsConnected(_cliMqttRef))
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

le_result_t HandlePublishCommand(int argIndex, char argument[MAX_ARGS][MAX_ARG_LENGTH], char * response, size_t responseSize)
{
    le_result_t     ret = LE_BAD_PARAMETER;

    //command format: pub <data>

    if (strlen(argument[argIndex]))
    {
        int rc = mqttClient_Publish(_cliMqttRef, (uint8_t *) argument[argIndex], strlen(argument[argIndex]), _gPublishTopicName);

        if (rc)
        {
            PrintMessage("Pub: failed to Publish data");
            ret = LE_FAULT;
        }
        else
        {
            
            PrintMessage("Pub : Publish OK");
            ret = LE_OK;
        }
    }
    else
    {
        PrintMessage("Pub : bad_parameter");
    }    

    return ret;
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

void ExitApp()
{
    exit(EXIT_SUCCESS);
}

void ExitTimer()
{
    le_timer_Ref_t timer = le_timer_Create("gMqttTimerEXit");
    LE_FATAL_IF(timer == NULL, "timerApp timer ref is NULL");

    le_clk_Time_t   interval = { 1, 0 };
    le_result_t     res;

    res = le_timer_SetInterval(timer, interval);
    LE_FATAL_IF(res != LE_OK, "set interval to %lu seconds: %d", interval.sec, res);

    res = le_timer_SetRepeat(timer, 1);
    LE_FATAL_IF(res != LE_OK, "set repeat to once: %d", res);

    le_timer_SetHandler(timer, ExitApp);

    le_timer_Start(timer);
}
//--------------------------------------------------------------------------------------------------
void mqttGoogleCliSvr_ExecuteCommand
(
    const char* userCommand,
    int32_t*    returnCodePtr,
    char*       response,            ///< [OUT] Retrieved string
    size_t      responseSize         ///< [IN] String buffer size in bytes
)
{
    *returnCodePtr = LE_NOT_FOUND;

    LE_INFO("mqttGoogleCliSvr_ExecuteCommand");

    char  argument[MAX_ARGS][MAX_ARG_LENGTH];
    int   argCount = 0;

    argCount = decodeArgument(userCommand, argument);

    response[0] = 0;
            
    if (argCount > 0)
    {
        if (strcasecmp("quit", argument[0])==0)
        {
            Disconnect();

            *returnCodePtr = LE_OK;

            ExitTimer();
        }
        else if (strcasecmp("config", argument[0])==0)
        {
            *returnCodePtr = HandleConfigCommand(1, argument, response, responseSize);
        }
        else if (strcasecmp("session", argument[0])==0)
        {
            *returnCodePtr = HandleSessionCommand(1, argument, response, responseSize);
        }
        else if (strcasecmp("pub", argument[0])==0)
        {
            *returnCodePtr = HandlePublishCommand(1, argument, response, responseSize);
        }
        else if (strcasecmp("queued", argument[0])==0)
        {
            *returnCodePtr = HandleQueuedCommand(response, responseSize);
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
    LE_INFO("---------- Launching Google Cloud MQTT Command Line Interface ----------");


    char  config[256];
    GetConfig(config, sizeof(config)); //display config for CLI
    PrintMessage(config);

    LE_INFO("Create Timer for Mqtt Yield: %d seconds", YIELD_INTERVAL_SECOND);
    _timerRef = le_timer_Create("gMqttTimerApp");
    LE_FATAL_IF(_timerRef == NULL, "timerApp timer ref is NULL");

    le_clk_Time_t   interval = { YIELD_INTERVAL_SECOND, 0 };
    le_result_t     res;

    res = le_timer_SetInterval(_timerRef, interval);
    LE_FATAL_IF(res != LE_OK, "set interval to %lu seconds: %d", interval.sec, res);

    res = le_timer_SetRepeat(_timerRef, 1);
    LE_FATAL_IF(res != LE_OK, "set repeat to once: %d", res);

    le_timer_SetHandler(_timerRef, timerHandler);
}

