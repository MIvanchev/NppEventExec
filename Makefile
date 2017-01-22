CC=gcc
CXX=g++
INCDIRS=-IE:\Develop\Libs\CPP\boost_1_58_0
CFLAGS+=$(INCDIRS) -Wall -pedantic
CXXFLAGS+=$(INCDIRS) -Wall -pedantic
LDFLAGS+=-LE:\Develop\Libs\CPP\boost_1_58_0\stage\lib
LDLIBS=-lgdi32 -lcomctl32
BIN_NAME=NppEventExec

ifndef CONFIGURATION
    CONFIGURATION=Debug
endif

ifndef PLATFORM
    PLATFORM=Win32
endif

ifneq ($(CONFIGURATION), Release)
ifneq ($(CONFIGURATION), Debug)
    $(error Unknown configuration: $(CONFIGURATION). Supported configurations are Debug and Release.)
endif
endif

ifneq ($(PLATFORM), Win32)
ifneq ($(PLATFORM), x64)
    $(error Unknown platform: $(PLATFORM). Supported platforms are Win32 and x64.)
endif
endif

OUTDIR=Build\Mingw\$(CONFIGURATION)\$(PLATFORM)
BIN_EXE=$(OUTDIR)\$(BIN_NAME).exe
BIN_DLL=$(OUTDIR)\$(BIN_NAME).dll
BIN=$(BIN_DLL)

ifeq ($(CONFIGURATION), Debug)
    CPPFLAGS += -g -DDEBUG
    LDLIBS += -lboost_regex-mgw62-mt-sd-1_58
    BIN += $(BIN_EXE)
else
    CPPFLAGS += -O2
    LDLIBS += -lboost_regex-mgw62-mt-s-1_58
endif

CSRC=$(wildcard *.c)
CXXSRC=$(wildcard *.cpp)
RES=$(wildcard *.rc)

OBJ=$(addprefix $(OUTDIR)\, $(notdir $(CSRC:.c=.o) $(CXXSRC:.cpp=.o) $(RES:.rc=.res)))

all: $(OUTDIR) $(BIN)

$(OUTDIR)\about_dlg.o: mem.h plugin.h resource.h util.h Notepad_plus_msgs.h
$(OUTDIR)\csv.o: event_map.h mem.h util.h utf8.h plugin.h
$(OUTDIR)\event_map.o: Notepad_plus_msgs.h
$(OUTDIR)\exec.o: mem.h plugin.h rule.h Scintilla.h resource.h util.h Notepad_plus_msgs.h nppexec_msgs.h
$(OUTDIR)\plugin.o: csv.h mem.h match.h rule.h util.h Scintilla.h exec.h resource.h about_dlg.h PluginInterface.h nppexec_msgs.h
$(OUTDIR)\$(BIN_NAME).res: $(wildcard resources\*)
$(OUTDIR)\rule.o: event_map.h csv.h mem.h plugin.h rule.h util.h Notepad_plus_msgs.h
$(OUTDIR)\util.o: mem.h plugin.h

$(OUTDIR)\\%.o: %.c %.h base.h
$(OUTDIR)\\%.o: %.cpp %.h base.h

$(OUTDIR):
	mkdir $(OUTDIR)

$(OUTDIR)\\%.o: %.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

$(OUTDIR)\\%.o: %.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) -o $@ $<

$(OUTDIR)\\%.res: %.rc base.h resource.h
	windres -i $< --input-format=rc -o $@ -O coff

$(BIN_DLL): $(OBJ)
	g++ $(LDFLAGS) -static-libgcc -static-libstdc++ -shared -o $@  $^ $(LDLIBS)

$(BIN_EXE): $(OBJ)
	g++ $(LDFLAGS) -mconsole -o $@ $^ $(LDLIBS)

clean:
	del /Q $(OUTDIR)\*
	rmdir $(OUTDIR)

.PHONY: all clean
