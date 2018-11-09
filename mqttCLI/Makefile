TARGETS := $(MAKECMDGOALS)

CURDIR = $(shell pwd)

.PHONY: all $(TARGETS)
all: $(TARGETS)

$(TARGETS):
	mkapp -v -t $@ \
		  -i $(LEGATO_ROOT)/interfaces/dataConnectionService \
		  -i $(LEGATO_ROOT)/interfaces/modemServices \
		  -i $(CURDIR)/../mqttClientApi \
		  -i $(CURDIR) \
		  mqttCLI.adef

clean:
	rm -rf _build_* *.update

