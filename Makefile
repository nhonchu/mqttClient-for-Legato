TARGETS := $(MAKECMDGOALS)

.PHONY: all $(TARGETS)
all: $(TARGETS)

$(TARGETS):
	mkapp -v -t $@ \
		  -i $(LEGATO_ROOT)/interfaces/dataConnectionService \
		  -i $(LEGATO_ROOT)/interfaces/modemServices \
		  -i mqttMainComponent/inc \
		  -i mqttMainComponent/inc/mqtt \
		  mqttClient.adef

clean:
	rm -rf _build_* *.update

