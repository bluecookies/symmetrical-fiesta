CXX=g++
# only need gnu extensions for _wfopen on windows (mingw)
WFLAGS= -pedantic -Wall -Wextra -Wshadow
CXXFLAGS=-g -std=gnu++11 -c $(WFLAGS)
LDFLAGS=-g
TARGETS=readscene readgameexe extractpck decompiless
HEADERS=Structs.h Helper.h Logger.h

ifeq ($(OS),Windows_NT)
EXE = $(TARGETS:%=$(BINDIR)/%.exe)
RM=del /Q
# This isn't really mkdir -p, since it would check if not exist
MKDIR_P= mkdir
else
EXE = $(TARGETS:%=$(BINDIR)/%)
#RM=rm -f
MKDIR_P= mkdir -p
endif
BINDIR=bin


all: $(EXE)

# gods this is ugly
$(BINDIR)/readscene $(BINDIR)/readscene.exe: ReadScene.o
$(BINDIR)/readgameexe $(BINDIR)/readgameexe.exe: ReadGameExe.o
$(BINDIR)/extractpck $(BINDIR)/extractpck.exe: ExtractPack.o
$(BINDIR)/decompiless $(BINDIR)/decompiless.exe: DecompileScript.o ControlFlow.o Expressions.o Statements.o Bitset.o Stack.o

$(EXE): Helper.o | $(BINDIR)
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $<

DecompileScript.o Statements.o Expressions.o Stack.o: Statements.h
ControlFlow.o Bitset.o: Bitset.h
DecompileScript.o ControlFlow.o: ControlFlow.h
DecompileScript.o ControlFlow.o Stack.o: BytecodeParser.h
#Stack.o DecompileScript.o: Stack.h

$(BINDIR):
	$(MKDIR_P) $@

os:
	$(info OS: $(OS))

clean:
	-$(RM) *.o
	-$(RM) $(BINDIR)