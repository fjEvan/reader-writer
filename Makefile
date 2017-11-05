LD=g++
LDFLAGS=-lpthread
CPPFLAGS=-std=c++11 
LINK.o = $(LINK.cc)
all: rw3
rw: rw.c
rwcpp: rw.cpp
	g++ -o rwcpp -std=c++11 rw.cpp -lpthread
semaphore.o: semaphore.h
rw2.o, rw3.o: semaphore.h readerwriter.h
rw2: rw2.o semaphore.o
rw3: rw3.o semaphore.o
