CC=g++
CFLAGS=-std=c++11 -c
EX=readscene readgameexe parsess
HEADERS=Structs.h Helper.h
ifeq ($(OS),Windows_NT)
EXE = $(EX:=.exe)
RM=del /Q
else
EXE = $(EX)
#RM=rm -f
endif

all: $(EXE)
	
readscene: ReadScene.o Helper.o
	$(CC) -o $@ $^

readgameexe: ReadGameExe.o Helper.o
	$(CC) -o $@ $^

extractpck: ExtractPack.o Helper.o
	$(CC) -o $@ $^

parsess: ParseSiglusScript.o Helper.o
	$(CC) -o $@ $^

%.o: %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $<

os:
	$(info OS: $(OS))

clean:
	-$(RM) *.o $(EXE)