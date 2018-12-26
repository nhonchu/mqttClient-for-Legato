/**
 * This component acts as MQTT Command Line Interface
 * It makes use of the MqttCliSvr component to interface with a MQTT Broker 
 *
 * 
 * Nhon Chu
 *
 *
 */

#include "legato.h"
#include "interfaces.h"


//----------------------------------------------------------------------------------
void PrintUsage()
{
    puts(
            "\nMQTT client command line usage:\n"
            "\n"
            "    mqtt config get - to display current config\n"
            "    mqtt config set broker/port/kalive/qos/clientId/username/password/passwordFile [newValue]\n"
            "\n"
            "    mqtt tls get - to display current TLS settings\n"
            "    mqtt tls set rootca/cert/privatekey [newValue]\n"
            "\n"
            "    mqtt session start  - start connection\n"
            "    mqtt session stop   - close connection\n"
            "    mqtt session status - get connection status\n"
            "\n"
            "    mqtt sub <topicName> - subscribe to a topic\n"
            "    mqtt unsub <topicName> - unscribe a topic\n"
            "\n"
            "    mqtt pub <topicName> <key/data> [<value>] - publish data\n"
            "\n"
            "    mqtt pubfile <topicName> <filename> - publish file content\n"
            "\n"
            "    mqtt queued - display incoming message\n"
            "\n"
            "    mqtt avSend <dataPath> <value> - send data to AirVantage\n"
            "\n"
            "    mqtt avAck <uid> <errorCode> <message> - send ACK to AirVantage\n"
            "\n"
            "    mqtt quit - quit MQTT Command Line Interface\n"
            );
}



COMPONENT_INIT
{
    int argCount = (int) le_arg_NumArgs();

    LE_INFO("arg count = %d.", argCount);

    if (argCount > 0)
    {
        char userCmd[256];
        int  i = 0;

        for (i=0; i<argCount; i++)
        {
            if (i == 0)
            {
                strcpy(userCmd, le_arg_GetArg(i));
            }
            else
            {
                strcat(userCmd, " ");
                strcat(userCmd, le_arg_GetArg(i));
            }
        }
        
        LE_INFO("mqtt-cmd - %s", userCmd);

        if (strlen(userCmd) == 0)
        {
            PrintUsage();
            exit(EXIT_FAILURE);
        }

        int32_t nRet = 0;
        char    response[1024] = {0};
        mqttCliSvr_ExecuteCommand((const char*) userCmd, &nRet, response, sizeof(response));
        
        if (strlen(response))
        {
            puts(response);
        }

        le_result_t     errCode = (le_result_t) nRet;

        if (LE_BAD_PARAMETER == errCode)
        {
            puts("   bad parameter");
        }
        else if (LE_NOT_FOUND == errCode)
        {
            puts("   invalid command");
        }
        else if (LE_FORMAT_ERROR == errCode)
        {
            puts("   format error");
        }
        else if (LE_DUPLICATE == errCode)
        {
            puts("   already exists");
        }
    }
    else
    {
        PrintUsage();
    }

    exit(EXIT_SUCCESS);
}
