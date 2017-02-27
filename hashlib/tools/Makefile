C++ = g++

CCFLAGS = -I../src/

%.o : %.cpp	
	$(C++) $< -c $(CCFLAGS)

LIBS = -lhl++

hello: hello.o 
	$(C++) $^ -o $@ $(LIBS)

APP = hello

all : $(APP) 
