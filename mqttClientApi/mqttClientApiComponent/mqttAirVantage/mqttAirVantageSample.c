/*******************************************************************************************************************
 
 MQTT Sample app to interface with AirVantage server
	This sample showcases the following features using the mqttAirVantage interface :
		- Publish simple message to AirVantage
		- Receive AirVantage commands along with command parameters
		- Receive Software/Firmware Installation (FOTA/SOTA) Request from AirVantage
			and ACKing the SW installation request
	Communication with AirVantage performed over MQTT prococol, with ot without secured transport : TLS
	Although the Software Package URL is provided by AirVantage, this sample does not perform
	software package download over https and does not handle software installation (platform specific).
	N. Chu
	June 2018
*******************************************************************************************************************/


#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#include "mqttAirVantage.h"


//-------------------------------------------------------------------------------------------------------
int 					g_toStop = 0;						//set to 1 in onExit() to exit program
int						g_ackSWinstall = 0;
char					g_uidSWinstall[64] = {0};

mqtt_instance_st*		g_mqttObject = NULL;


//-------------------------------------------------------------------------------------------------------
void onExit(int sig)
{
	signal(SIGINT, NULL);
	g_toStop = 1;

	fprintf(stdout, "\nterminating app...\n");
	fflush(stdout);
}

//-------------------------------------------------------------------------------------------------------
static void OnIncomingMessage(
                const char* topicName,
                const char* key,    //could be empty
                const char* value,  //aka payload
                const char* timestamp,  //could be empty
                void*       pUserContext)
{
    fprintf(stdout, "Received message from topic %s:", topicName);
	fprintf(stdout, "   Message timestamp epoch: %s\n", timestamp);
	fprintf(stdout, "   Parameter Name: %s\n", key);
	fprintf(stdout, "   Parameter Value: %s\n", value);
}

//-------------------------------------------------------------------------------------------------------
static void OnSoftwareInstallRequest(
				const char* uid,
				const char* type,
				const char* revision,
				const char* softwarePkgUrl,
				const char* timestamp,
                void *      pUserContext)
{
	fprintf(stdout, "Received Software Installation request from AirVanage:\n");
	fprintf(stdout, "   Operation uid: %s\n", uid);
	fprintf(stdout, "   Timestamp epoch: %s\n", timestamp);
	fprintf(stdout, "   SW pkg type: %s\n", type);
	fprintf(stdout, "   SW revision: %s\n", revision);
	fprintf(stdout, "   SW pkg download Url: %s\n", softwarePkgUrl);

	/*
		Your device should be :
		- downloading the software/firmware package with the provide url (not done in this sample)
		- authenticating the issuer of package, checking package integrity (not done here)
		- installing the software/firmware (not done here)
		- acking the SW installation operation to AirVantage (done below with a delay) */
	//Let's delay this ACK
	fprintf(stdout, "\nwill be ACKing this request in few seconds...\n");
	g_ackSWinstall = 5;
	strcpy(g_uidSWinstall, uid);
}


//-------------------------------------------------------------------------------------------------------
int main(int argc, char** argv)
{
	if (argc < 3)
	{
		printf("Usage: mqttAirVantageSample serial password [tls]\n");
		return 1;
	}

	signal(SIGINT, onExit);
	signal(SIGTERM, onExit);

	int useTls = 0;

	if (argc > 3)
	{
		if (strcasecmp("tls", argv[3]) == 0)
		{
			useTls = 1;	//use TLS if specified in 3rd argument
		}
	}

	mqtt_config_t		mqttConfig;

	mqtt_avGetDefaultConfig(&mqttConfig);

	strcpy(mqttConfig.deviceId, argv[1]);
	strcpy(mqttConfig.secret, argv[2]);
	mqttConfig.useTLS = useTls;


	if (!g_mqttObject)
	{
		g_mqttObject = mqtt_CreateInstance(&mqttConfig);
	}

	if (SUCCESS == mqtt_StartSession(g_mqttObject))
	{		
		fprintf(stdout, "MQTT session started OK\n");
	}
	else
	{
		fprintf(stdout, "Failed to start MQTT session on AirVantage server\n");
		return 1;
	}

	//Set handler for AirVantage incoming message/command/SW-install
	mqtt_avSetCommandHandler(g_mqttObject, (incomingMessageHandler) OnIncomingMessage, NULL);
	mqtt_avSetSoftwareInstallRequestHandler(g_mqttObject, (softwareInstallRequestHandler) OnSoftwareInstallRequest, NULL);

	

	int 	count = 0;
	int 	i = 0;
	char	data[16] = {0};

	while (!g_toStop)
	{
		fprintf(stdout, ".");
		fflush(stdout);
		//Must call this on a regular basis in order to process inbound mqtt messages & keep alive
		if (mqtt_ProcessEvent(g_mqttObject, 1000) < -2)
		{
			fprintf(stdout, "MQTT connection is lost");
			fflush(stdout);
			break;
		}

		sleep(1);

		count++;
		i++;

		if (i >= 5)
		{
			i = 0;

			sprintf(data, "%d", count);
			//Let's publish data
			mqtt_avPublishData(g_mqttObject, "counter", data);
		}

		if (g_ackSWinstall > 0)
		{
			//If there is a SW install request, ack the operation here after some delay
			//the delay intends to simulate the SW download and install processes
			//This ACKing should be performed after installation procedure
			g_ackSWinstall--;
			if (0 == g_ackSWinstall)
			{
				//ACK the SW install request
				fprintf(stdout, "\nNow, ACKing the pending SW Install request\n");
				mqtt_avPublishAck(g_mqttObject, g_uidSWinstall, 0, "install success");
			}
		}
	}

	//Close the session and quit
	mqtt_DeleteInstance(g_mqttObject);

	return 0;
}
