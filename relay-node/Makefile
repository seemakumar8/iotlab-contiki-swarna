CONTIKI_PROJECT = udp-relay
all: $(CONTIKI_PROJECT)
PROJECT_SOURCEFILES += ../utility-functions.c list_utility.c

CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
MODULES += core/net/mac/tsch
MODULES += core/net/ipv6/multicast

CONTIKI_WITH_IPV6 = 1
MAKE_WITH_ORCHESTRA = 1 # force Orchestra from command line

APPS += orchestra
MODULES += core/net/mac/tsch

ifeq ($(MAKE_WITH_ORCHESTRA),1)
	CFLAGS += -DWITH_ORCHESTRA=1
endif

CONTIKI=../../..

include $(CONTIKI)/Makefile.include
