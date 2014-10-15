CC=gcc
CXX=g++
CSRCS=main.c
CXXSRCS=build.cc
COBJS=$(CSRCS:.c=.o)
CXXOBJS=$(CXXSRCS:.cc=.oo)
OBJS=$(COBJS) $(CXXOBJS)
CFLAGS=-g3 -O0 -std=c99 $(EXTRA_CFLAGS)
LIBS=-lreadline
CXXFLAGS=-g3 -O0 -std=c++11
APP=coogle

all: $(APP)

$(APP): $(OBJS) 
	$(CXX) $^ $(LIBS) $(CXXFLAGS) -o $@

%.o: %.c
	$(CC) -c $^ $(CFLAGS) -o $@

%.oo: %.cc
	$(CXX) -c $^ $(CXXFLAGS) -o $@

.PHONY: test
test: $(APP)
	./$(APP) cscope.out

clean:
	$(RM) $(APP) $(OBJS)
