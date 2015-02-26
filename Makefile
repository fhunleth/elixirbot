ifeq ($(NERVES_ROOT),)
    $(error Make sure that you source nerves-env.sh)
endif

ERL_LIB = $(NERVES_SDK_SYSROOT)/usr/lib/erlang/lib
REL2FW = $(NERVES_ROOT)/scripts/rel2fw.sh

# This should get the one built by nerves assuming that the Nerves
# environment is loaded.
FWUP ?= $(shell which fwup)

all: deps
	mix compile
	ln -sfT $(ERL_LIB) rel/nerves_system_libs
	mix release
	$(REL2FW) rel/elixirbot

deps:
	mix deps.get

# Replace everything on the SDCard with new bits
burn-complete:
	sudo $(FWUP) -a -i $(firstword $(wildcard _images/*.fw)) -t complete

# Upgrade the image on the SDCard (app data won't be removed)
# This is usually the fastest way to update an SDCard that's already
# been programmed. It won't update bootloaders, so if something is
# really messed up, do a burn-complete.
burn-upgrade:
	sudo $(FWUP) -a -i $(firstword $(wildcard _images/*.fw)) -t upgrade
	sudo $(FWUP) -y -a -i /tmp/finalize.fw -t on-reboot
	sudo rm /tmp/finalize.fw

clean:
	mix clean; rm -fr _build _images rel/elixirbot

distclean: clean
	-rm -fr ebin deps

.PHONY: deps burn-complete burn-upgrade clean distclean all
