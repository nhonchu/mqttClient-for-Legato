TARGETS := ar7 wp76xx wp77xx ar86 wp85 localhost

CURDIR = $(shell pwd)

.PHONY: all $(TARGETS)
all: $(TARGETS)

$(TARGETS):
	export TARGET=$@ ; \
	mkapp -v -t $@ \
		  -i $(LEGATO_ROOT)/interfaces/dataConnectionService \
		  -i $(LEGATO_ROOT)/interfaces/modemServices \
		  -i $(CURDIR)/../mqttClientApi \
		  mqttClientSample.adef

clean:
	rm -rf _build_* *.ar7 *.wp7 *.ar86 *.wp85 *.localhost
