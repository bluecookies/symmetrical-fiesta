CC=g++
CFLAGS=-std=c++11 -c
TARGETS=readscene readgameexe parsess extractpck
HEADERS=Structs.h Helper.h
ifeq ($(OS),Windows_NT)
EXE = $(TARGETS:=.exe)
RM=del /Q
else
EXE = $(TARGETS)
#RM=rm -f
endif
BINDIR=bin

all: $(EXE)

	
readscene: ReadScene.o
readgameexe: ReadGameExe.o
extractpck: ExtractPack.o
parsess: ParseSiglusScript.o

$(TARGETS): Helper.o
	$(CC) -o $(BINDIR)/$@ $^

%.o: %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $<

os:
	$(info OS: $(OS))

clean:
	-$(RM) *.o
	-$(RM) $(BINDIR)/*