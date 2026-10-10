# Builds the tutorial's programs without CMake (docs/tutorial/17-building.md).
#
#     make EASYLOCAL=/usr/local                    the programs of Core
#     make EASYLOCAL=/usr/local tsp_tui            one optional component
#
# EASYLOCAL is an installation prefix or a source checkout: both hold the
# headers under include/. The optional components also need their own library,
# whose prefix is FTXUI, CROW or TOMLPP.

EASYLOCAL ?= /usr/local
CXX       ?= c++
CXXFLAGS  ?= -std=c++23 -O2 -Wall -Wextra
CPPFLAGS  += -I$(EASYLOCAL)/include

FTXUI  ?= /usr/local
CROW   ?= /usr/local
TOMLPP ?= /usr/local

# The flags of each optional component, derived from its prefix: override a
# prefix, or the flags themselves, on the command line. The third-party
# headers come in with -isystem, which leaves their own warnings out.
TUI_CPPFLAGS  ?= -isystem $(FTXUI)/include
TUI_LDLIBS    ?= -L$(FTXUI)/lib -lftxui-component -lftxui-dom -lftxui-screen -pthread
REST_CPPFLAGS ?= -isystem $(CROW)/include
REST_LDLIBS   ?= -pthread
TOML_CPPFLAGS ?= -isystem $(TOMLPP)/include

# The programs of Core, built by default, and those of the optional components.
PROGRAMS  := tsp tsp_cli tsp_checks
OPTIONAL  := tsp_tui tsp_rest tsp_toml

all: $(PROGRAMS)

tsp: main.o
tsp_cli: cli_main.o
tsp_checks: checks.o
tsp_tui: tui_main.o
tsp_rest: rest_main.o
tsp_toml: toml_main.o

# A target-specific variable reaches the object files of its program too.
tsp_tui: CPPFLAGS += $(TUI_CPPFLAGS)
tsp_tui: LDLIBS += $(TUI_LDLIBS)
tsp_rest: CPPFLAGS += $(REST_CPPFLAGS)
tsp_rest: LDLIBS += $(REST_LDLIBS)
tsp_toml: CPPFLAGS += $(TOML_CPPFLAGS)

$(PROGRAMS) $(OPTIONAL):
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.cpp tsp.hpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

clean:
	rm -f $(PROGRAMS) $(OPTIONAL) *.o

.PHONY: all clean
