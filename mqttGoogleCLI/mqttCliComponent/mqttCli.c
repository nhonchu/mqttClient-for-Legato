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
            "\nGoogle Cloud Platform - Iot Core - MQTT client command line usage:\n"
            "\n"
            "    gmqtt config get - to display current config\n"
            "    gmqtt config set projectId/location/registry/deviceId/rsaPrivateKey [newValue]\n"
            "\n"
            "    gmqtt session start  - start connection\n"
            "    gmqtt session stop   - close connection\n"
            "    gmqtt session status - get connection status\n"
            "\n"
            "    gmqtt pub <data> - publish data to 'events' topic\n"
            "\n"
            "    gmqtt queued - display incoming message\n"
            "\n"
            "    gmqtt quit - quit MQTT Command Line Interface\n"
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
        
        LE_INFO("gmqtt-cmd - %s", userCmd);

        if (strlen(userCmd) == 0)
        {
            PrintUsage();
            exit(EXIT_FAILURE);
        }

        int32_t nRet = 0;
        char    response[1024] = {0};
        mqttGoogleCliSvr_ExecuteCommand((const char*) userCmd, &nRet, response, sizeof(response));
        
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
