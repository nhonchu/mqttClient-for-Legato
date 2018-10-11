TARGETS := $(MAKECMDGOALS)

MQTT_API_ROOT := ~/github-nhonchu/mqttClient-for-Legato/mqttClientApi

.PHONY: all $(TARGETS)
all: $(TARGETS)

$(TARGETS):
	@echo ${MQTT_API_ROOT}
	mkapp -v -t $@ \
		-i $(LEGATO_ROOT)/interfaces/modemServices \
		-i $(MQTT_API_ROOT)/mqttClientApiComponent/paho \
	    -i $(MQTT_API_ROOT)/mqttClientApiComponent/mqttAirVantage \
	    -i $(MQTT_API_ROOT)/mqttClientApiComponent/mqttGeneric \
	    -i $(MQTT_API_ROOT)/mqttClientApiComponent/tlsInterface \
	    -i $(MQTT_API_ROOT)/mqttClientApiComponent/mbedtls/include \
		  mqttClient.adef

clean:
	rm -rf _build_* *.update

