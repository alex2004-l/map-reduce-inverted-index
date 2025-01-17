CC = g++
CFLAGS = -std=c++17 -Wall -Werror -lpthread

build: main.o utils.o
		$(CC) $(CFLAGS) -o tema1 main.o utils.o

main.o: main.cpp utils.hpp
		$(CC) $(CFLAGS) -c main.cpp -o main.o

utils.o: utils.cpp utils.hpp
		$(CC) $(CFLAGS) -c utils.cpp -o utils.o

clean:
		rm tema1 main.o utils.o