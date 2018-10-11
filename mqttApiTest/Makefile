TARGETS := ar7 wp76xx ar86 wp85 localhost

MQTT_API_ROOT := ~/github-nhonchu/mqttClient-for-Legato/mqttClientApi

.PHONY: all $(TARGETS)
all: $(TARGETS)

$(TARGETS):
	export TARGET=$@ ; \
	mkapp -v -t $@ \
		  -i $(LEGATO_ROOT)/interfaces/dataConnectionService \
		  -i $(LEGATO_ROOT)/interfaces/modemServices \
		  -i $(MQTT_API_ROOT) \
		  mqttClientSample.adef

clean:
	rm -rf _build_* *.ar7 *.wp7 *.ar86 *.wp85 *.localhost
