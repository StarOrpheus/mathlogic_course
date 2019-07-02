.PHONY: all, run, clean

COMPILER=g++
OPTIONS=-O9 -D NDEBUG -march=native --std=c++17 -o main
SOURCES=main.cpp parsex.h parsex.cpp proof.cpp proof.h ast_record.cpp ast_record.h templates.cpp templates.h

all: $(SOURCES)
	$(COMPILER) $(SOURCES) $(OPTIONS)
run:
	./main
clean:
	rm ./main
