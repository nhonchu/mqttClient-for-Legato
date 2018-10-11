TARGETS := $(MAKECMDGOALS)

MQTT_API_ROOT := ~/github-nhonchu/mqttClient-for-Legato/mqttClientApi
MQTT_CLI_ROOT := ~/github-nhonchu/mqttClient-for-Legato/mqttCLI

.PHONY: all $(TARGETS)
all: $(TARGETS)

$(TARGETS):
	mkapp -v -t $@ \
		  -i $(LEGATO_ROOT)/interfaces/dataConnectionService \
		  -i $(LEGATO_ROOT)/interfaces/modemServices \
		  -i $(MQTT_API_ROOT) \
		  -i $(MQTT_CLI_ROOT) \
		  mqttCLI.adef

clean:
	rm -rf _build_* *.update

