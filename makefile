CXX=g++
CXXFLAGS=-std=c++11 -c -Wall -Wextra -pedantic
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
$(BINDIR)/parsess $(BINDIR)/parsess.exe: ParseSiglusScript.o

$(EXE): Helper.o | $(BINDIR)
	$(CXX) -o $@ $^

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BINDIR):
	$(MKDIR_P) $@

os:
	$(info OS: $(OS))

clean:
	-$(RM) *.o
	-$(RM) $(BINDIR)/*