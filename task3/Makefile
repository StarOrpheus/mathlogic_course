.PHONY: all, run, clean

COMPILER=g++
OPTIONS=-O9 --std=c++17 -o main
SOURCES=testing.cpp parsex.h parsex.cpp templates.cpp templates.h 

all: $(SOURCES)
	$(COMPILER) $(SOURCES) $(OPTIONS)
run:
	./main
clean:
	rm ./main
