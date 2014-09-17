SOURCE_FILES := $(wildcard *.c *.h)
OBJECTS := $(subst .c,.o,$(filter %.c,$(SOURCE_FILES)))
CPPFLAGS := -Wall -Wextra -g -c
LDFLAGS := -g

all: tcpredirect

tcpredirect: $(OBJECTS)
	gcc $(LDFLAGS) -o $@ $^

%.o: %.c
	gcc $(CPPFLAGS) $^

clean:
	-@rm *.o tcpredirect > /dev/null 2>&1

run:
	./tcpredirect -k boosen: 192.168.1.212:5400:127.0.0.1:80

