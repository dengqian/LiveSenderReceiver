C++ = g++
DEPDIR := .d
$(shell mkdir -p $(DEPDIR) >/dev/null)
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td

ifndef os
   os = LINUX
endif

ifndef arch
   arch = IA32
endif

CCFLAGS = -std=c++11 -Wall -D$(os) -I../src -I../ -I./kodo/include -finline-functions -O3

ifeq ($(arch), IA32)
   CCFLAGS += -DIA32 #-mcpu=pentiumpro -march=pentiumpro -mmmx -msse
endif

ifeq ($(arch), POWERPC)
   CCFLAGS += -mcpu=powerpc
endif

ifeq ($(arch), IA64)
   CCFLAGS += -DIA64
endif

ifeq ($(arch), SPARC)
   CCFLAGS += -DSPARC
endif

#LDFLAGS = -std=c++11 -L../src -L./kodo -ludt -lstdc++ -lpthread -lm
LDFLAGS = -std=c++11 -L../src -L./kodo -ludt -lpthread -lm

ifeq ($(os), UNIX)
   LDFLAGS += -lsocket
endif

ifeq ($(os), SUNOS)
   LDFLAGS += -lrt -lsocket
endif

DIR = $(shell pwd)

APP = appserver appclient sendfile recvfile test push_server sender receiver 
# APP = sender 

all: $(APP)

KODOFLAGS = -Wl,-Bdynamic -lkodoc -Wl,-rpath .

# %.o : %.cpp $(DEPDIR)/%.d
# 	$(C++) $(CCFLAGS) $(DEPFLAGS) $< -c

%.o : %.cpp common.h 
	$(C++) $(CCFLAGS) $< -c

appserver: appserver.o
	$(C++) $^ -o $@ $(LDFLAGS) $(KODOFLAGS)
appclient: appclient.o
	$(C++) $^ -o $@ $(LDFLAGS) $(KODOFLAGS)
sendfile: sendfile.o
	$(C++) $^ -o $@ $(LDFLAGS)
recvfile: recvfile.o
	$(C++) $^ -o $@ $(LDFLAGS)
test: test.o
	$(C++) $^ -o $@ $(LDFLAGS)
push_server: push_server.o
	$(C++) $^ -o $@ $(LDFLAGS)
sender: sender.o ../src/common.o ../src/md5.o
	$(C++) $^ -o $@ $(LDFLAGS) $(KODOFLAGS)
receiver: receiver.o
	$(C++) $^ -o $@ $(LDFLAGS) $(KODOFLAGS)

	
clean:
	rm -f *.o $(APP)

install:
	export PATH=$(DIR):$$PATH
