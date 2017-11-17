CXX=g++
# only need gnu extensions for _wfopen on windows (mingw)
WFLAGS= -pedantic -Wall -Wextra
CXXFLAGS=-g -std=gnu++11 -c $(WFLAGS)
LDFLAGS=-g
TARGETS=readscene readgameexe parsess extractpck
HEADERS=Structs.h Helper.h

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
$(BINDIR)/parsess $(BINDIR)/parsess.exe: ParseSiglusScript.o BytecodeParser.o

$(EXE): Helper.o | $(BINDIR)
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $<

ParseSiglusScript.o BytecodeParser.o: BytecodeParser.h

$(BINDIR):
	$(MKDIR_P) $@

os:
	$(info OS: $(OS))

clean:
	-$(RM) *.o
	-$(RM) $(BINDIR)