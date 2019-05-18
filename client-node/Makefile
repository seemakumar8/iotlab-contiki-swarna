CONTIKI_PROJECT = udp-client
all: $(CONTIKI_PROJECT)
PROJECT_SOURCEFILES += ../utility-functions.c

CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
#MODULES += os/services/shell
#MAKE_MAC = MAKE_MAC_TSCH
#MODULES += os/services/orchestra
MODULES += core/net/mac/tsch
MODULES += core/net/ipv6/multicast

CONTIKI_WITH_IPV6 = 1
MAKE_WITH_ORCHESTRA = 1 # force Orchestra from command line

APPS += orchestra
MODULES += core/net/mac/tsch

ifeq ($(MAKE_WITH_ORCHESTRA),1)
	CFLAGS += -DWITH_ORCHESTRA=1
endif

#MODULES += os/net/ipv6/multicast
#MAKE_ROUTING = MAKE_ROUTING_RPL_CLASSIC


CONTIKI=../../..
include $(CONTIKI)/Makefile.include
