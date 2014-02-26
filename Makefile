TESTS_SUFFIX=_test
CC=gcc
CFLAGS=-Wall -Werror
DEP_LIST=event.o util.o char_stream.o sat_variable.o sat_table.o sat_parser.o \
	sat_args.o
CONFFILE=sat.mk

-include $(CONFFILE)

ifeq ($(TESTS),y)
PROG_SUFFIX=$(TESTS_SUFFIX)
CFLAGS+=-g
ifeq ($(COLOURS),y)
CFLAGS+=-DCOLOURS
endif
ifeq ($(ALL_TESTS),y)
CFLAGS+=-DALL_TESTS
endif
DEP_LIST+=unit_test.o sat_test.o
else
DEP_LIST+=main.o sat.o sat_ca.o sat_print.o
endif

%.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

.PHONY: all config clean cleanall
PROG=sat$(PROG_SUFFIX)
all: $(PROG)

$(PROG): $(DEP_LIST)
	$(CC) -o $@ $^

config:
	@echo "doing make config"
	set -e; \
	rm -f $(CONFFILE); \
	echo "$(strip $(MAKEFLAGS))" | sed -e 's/ /\r\n/g' > $(CONFFILE);

clean:
	rm -f *.o

cleanconf:
	rm -f $(CONFFILE)

cleantags:
	rm -f tags

cleanall: clean cleanconf cleantags
	file `ls` | grep executable | awk -F: '{ print $$1 }' | xargs rm -f
