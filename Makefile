#
# The purpose of the following lines is to extract the directory of the
# makefile. Essentially, this is done by
#
# $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
#
# However, since dir doesn't operate correctly on paths containing spaces, the
# spaces are first replaced with double quotes, because double quotes cannot
# appear in paths on Windows. Once dir is invoked, the quotes are converted
# back to spaces.
#
# The directory is needed in order to call the supplementary script dircmd.bat
# which allows us to use the shell's internal commands mkdir and rmdir.
# Otherwise the makefile will have to depend on external programs for the
# purpose of creating and deleting directories.
#

esc=$(subst $(empty) $(empty),",$1)
unesc=$(subst ", ,$1)

MAKEFILE_DIR=$(call unesc,$(dir $(call esc, $(subst /,\,$(realpath $(lastword $(MAKEFILE_LIST)))))))
MKDIR=$(MAKEFILE_DIR)dircmd.bat mkdir
RMDIR=$(MAKEFILE_DIR)dircmd.bat rmdir

CC=gcc
CXX=g++
INCDIRS=-IE:\Develop\Libs\CPP\boost_1_58_0
CFLAGS+=$(INCDIRS) -Wall -pedantic
CXXFLAGS+=$(INCDIRS) -Wall -pedantic
LDFLAGS+=-LE:\Develop\Libs\CPP\boost_1_58_0\stage\lib
LDLIBS=-lgdi32 -lcomctl32 -lshell32
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

BUILDDIR=Build
OUTDIR=$(BUILDDIR)\Mingw\$(CONFIGURATION)\$(PLATFORM)
BIN_EXE=$(OUTDIR)\$(BIN_NAME).exe
BIN_DLL=$(OUTDIR)\$(BIN_NAME).dll
BIN=$(BIN_DLL)

ifeq ($(CONFIGURATION), Debug)
    CPPFLAGS += -g -DDEBUG
    LDLIBS += -lboost_regex-mgw62-mt-sd-1_58
    BIN += $(BIN_EXE)
else
    CPPFLAGS += -O2 -DNDEBUG
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
$(OUTDIR)\rule.o: event_map.h csv.h mem.h plugin.h rule.h util.h Notepad_plus_msgs.h
$(OUTDIR)\util.o: mem.h plugin.h

$(OUTDIR):
	$(MKDIR) $@

$(OUTDIR)\\%.o: %.c %.h base.h
	$(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

$(OUTDIR)\\%.o: %.cpp %.h base.h
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) -o $@ $<

$(OUTDIR)\\%.res: %.rc base.h resource.h $(wildcard resources\*)
	windres -i $< --input-format=rc -o $@ -O coff

$(BIN_DLL): $(OBJ)
	g++ $(LDFLAGS) -static-libgcc -static-libstdc++ -shared -o $@  $^ $(LDLIBS)

$(BIN_EXE): $(OBJ)
	g++ $(LDFLAGS) -mconsole -o $@ $^ $(LDLIBS)

clean:
	$(RMDIR) $(BUILDDIR)

.PHONY: all clean
