TARGETS := $(MAKECMDGOALS)

CURDIR = $(shell pwd)

.PHONY: all $(TARGETS)
all: $(TARGETS)

$(TARGETS):
	@echo ${MQTT_API_ROOT}
	mkapp -v -t $@ \
		-i $(LEGATO_ROOT)/interfaces/modemServices \
		-i $(CURDIR)/mqttClientApiComponent/paho \
	    -i $(CURDIR)/mqttClientApiComponent/mqttAirVantage \
	    -i $(CURDIR)/mqttClientApiComponent/mqttGeneric \
	    -i $(CURDIR)/mqttClientApiComponent/tlsInterface \
	    -i $(CURDIR)/mqttClientApiComponent/mbedtls/include \
		  mqttClient.adef

clean:
	rm -rf _build_* *.update

