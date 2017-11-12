CXX=g++
CXXFLAGS=-std=c++11 -c
TARGETS=readscene readgameexe parsess extractpck
HEADERS=Structs.h Helper.h
ifeq ($(OS),Windows_NT)
EXE = $(TARGETS:%=$(BINDIR)/%.exe)
RM=del /Q
else
EXE = $(TARGETS:%=$(BINDIR)/%)
#RM=rm -f
endif
BINDIR=bin


all: $(EXE)

# gods this is ugly
$(BINDIR)/readscene $(BINDIR)/readscene.exe: ReadScene.o
$(BINDIR)/readgameexe $(BINDIR)/readgameexe.exe: ReadGameExe.o
$(BINDIR)/extractpck $(BINDIR)/extractpck.exe: ExtractPack.o
$(BINDIR)/parsess $(BINDIR)/parsess.exe: ParseSiglusScript.o

$(EXE): Helper.o
	$(CXX) -o $@ $^

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $<

os:
	$(info OS: $(OS))

clean:
	-$(RM) *.o
	-$(RM) $(BINDIR)/*