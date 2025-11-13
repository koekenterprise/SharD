TARGET=shard
CXX=g++
DEBUG=-g
OPT=-O0
WARN=-Wall -Wno-unknown-pragmas
CXX_STD=-std=c++17
NCURSES=-lncurses -ltinfo
FILESYSTEM_LIB=-lstdc++fs 
CXXFLAGS=$(DEBUG) $(OPT) $(WARN) $(CXX_STD) $(NCURSES) -pipe
LD=g++
LDFLAGS=$(NCURSES) $(FILESYSTEM_LIB)
OBJS= main.o shard.o

all: $(OBJS)
	$(LD) -o $(TARGET) $(OBJS) $(LDFLAGS)
	@rm -rf *.o
	
main.o: main.cpp
	$(CXX) -c $(CXXFLAGS) main.cpp -o main.o
	
shard.o: shard.cpp
	$(CXX) -c $(CXXFLAGS) shard.cpp -o shard.o
