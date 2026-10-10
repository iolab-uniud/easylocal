# Building without CMake

EasyLocal Core is header-only and depends only on the standard library: a
compiler, the C++23 flag and one include path build a program. This chapter
gives the whole workflow for a build driven by a makefile or by a single
command — what to do before the first compilation, the makefile of the
tutorial's programs, and the headers and libraries each optional component
needs. CMake stays the supported way to consume the library (the
[quick start](../quick-start.md) shows `find_package` and `add_subdirectory`);
a makefile is for a course machine, an existing build system, or a program you
compile by hand.

## Before you start

**A compiler.** The same ones the [quick start](../quick-start.md#requirements)
lists: GCC 15 or 16, Clang 22 or 23, AppleClang from Xcode 26. The flags below
are the GCC and Clang spelling; with clang-cl, use `/std:c++latest` and
`/I<dir>`.

**The headers.** Two sources, with the same content: the build generates no
header.

- *An installation.* Configure EasyLocal once with CMake, then install it
  where you like:

    ```sh
    cmake -S easylocal -B build/release -DCMAKE_BUILD_TYPE=Release
    cmake --install build/release --prefix ~/.local
    ```

    The headers land in `~/.local/include/easylocal/`, together with the
    adapters of the optional components the build enabled and with the
    third-party headers it fetched (see
    [the optional components](#the-optional-components)).

- *A source checkout.* `git clone https://github.com/iolab-uniud/easylocal`
  and point the include path at `easylocal/include`. Nothing is built, and
  the adapters are there too; only the third-party libraries of the optional
  components are missing, since a checkout fetches nothing.

**The libraries of the optional components,** if the program uses one: FTXUI
for the TextUI, Crow for the REST service, toml++ for the TOML configuration.
They are the only link-time dependency EasyLocal ever has; Core needs none.

**No `-ffast-math`,** which lets the compiler assume away the NaN and infinite
values the library checks for, and **`-O2` or better**: the library relies on
inlining, and a search can be an order of magnitude slower with `-O0`.

## One command

A single-file program, such as the one of the quick start, needs no makefile:

```sh
c++ -std=c++23 -O2 -Wall -I$HOME/.local/include main.cpp -o tsp
./tsp
```

`-I` points at the directory that *contains* `easylocal/`, so that
`#include <easylocal/easylocal.hpp>` resolves. There is nothing to link and no
`-D` to define. Core starts no thread of its own; add `-pthread` if your
platform requires it for the standard library, or if your program uses
threads.

## A makefile

The tutorial's programs are built by `examples/tutorial/Makefile`, which is
the recipe for any EasyLocal program:

<!-- snippet: tutorial/Makefile -->
```make
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
```

From the example directory, the three programs of Core are one command away:

```sh
cd examples/tutorial
make EASYLOCAL=$HOME/.local
./tsp                 # the searches of chapters 5 and 6
./tsp_cli --help      # the command line of chapter 11
./tsp_checks          # the component checks of chapter 10
```

Three things are worth copying into a makefile of your own:

- **`CPPFLAGS` carries the include path**, and the compilation rule passes it
  to every object file. Adding a program means adding its `.o` to the list;
  the `%.o: %.cpp tsp.hpp` rule rebuilds an object when the header of the
  problem changes, which a header-only library makes frequent. For more
  headers, list them there, or generate the dependencies with `-MMD -MP`.
- **Every translation unit uses the same flags.** EasyLocal's classes are
  shared across them, and compiling two of them with different standards or
  different `-D` definitions breaks the one-definition rule, usually at link
  time or, worse, not at all.
- **A target-specific variable** (`tsp_tui: CPPFLAGS += ...`) reaches the
  object files built for that program, so the flags of an optional component
  stay out of the programs that do not use it.

## The optional components

Each component is a header of `easylocal/adapters/` plus the third-party
library it adapts. The component's own header needs no `-D` definition and no
EasyLocal library:

| Component | Header | Library | Compile | Link |
| --- | --- | --- | --- | --- |
| Core | `<easylocal/easylocal.hpp>` | — | `-I<prefix>/include` | — |
| TextUI | `<easylocal/adapters/tui.hpp>` | FTXUI ≥ 7 | `-isystem <ftxui>/include` | `-L<ftxui>/lib -lftxui-component -lftxui-dom -lftxui-screen -pthread` |
| REST | `<easylocal/adapters/rest.hpp>` | Crow ≥ 1.3, with standalone Asio | `-isystem <crow>/include -isystem <asio>/include` | `-pthread` |
| ConfigTOML | `<easylocal/adapters/toml.hpp>` | toml++ ≥ 3.4 | `-isystem <toml++>/include` | — |

The three FTXUI libraries are listed in that order: `component` uses `dom`,
which uses `screen`. Crow and toml++ are header-only; Crow includes Asio,
which is header-only as well when it is the standalone version (the one
EasyLocal uses).

Where the libraries come from:

- **A package manager.** FTXUI, Crow and toml++ are in Homebrew, vcpkg and
  several Linux distributions. The prefix is then the package manager's own
  (`/opt/homebrew` or `/usr/local` with Homebrew, `/usr` with a distribution
  package).
- **The EasyLocal installation itself.** A build configured with
  `-DEASYLOCAL_FETCH_DEPENDENCIES=ON` downloads what the enabled components
  need and `cmake --install` puts it in the same prefix: FTXUI under
  `include/ftxui/` with its libraries in `lib/`, Crow under `include/crow/`,
  and toml++ and Asio as private copies under
  `include/easylocal/third_party/`. The prefixes of the makefile then all
  point at the installation:

    ```sh
    make EASYLOCAL=$HOME/.local FTXUI=$HOME/.local tsp_tui
    make EASYLOCAL=$HOME/.local \
         REST_CPPFLAGS="-isystem $HOME/.local/include -isystem $HOME/.local/include/easylocal/third_party/asio" \
         tsp_rest
    make EASYLOCAL=$HOME/.local \
         TOML_CPPFLAGS="-isystem $HOME/.local/include/easylocal/third_party/tomlplusplus" \
         tsp_toml
    ```

- **A source build** of the library, installed wherever you like; its prefix
  is what the makefile wants.

The [dependency policy](../dependency-policy.md) says which versions each
component accepts and why. The rest of the framework — tracing, logging, the
command line, the [tuning](12-tuning.md) files for irace — is Core, so it
needs no flag beyond the include path.

## What CMake does that a makefile does not

A makefile build compiles the same code, with the same behaviour. What you
give up is the checking around it:

- the C++23 and compiler-version check of `find_package(EasyLocal)`, whose
  message says what is missing instead of a wall of template errors;
- the check that a component you ask for is in the installation
  (`COMPONENTS TUI` fails when the TextUI was not enabled, whereas a missing
  header is a compiler error);
- fetching the third-party libraries, and passing their include paths and
  libraries to the compiler for you;
- the deployment-target check on macOS, where AppleClang needs Xcode 26 and a
  target of 26.0 or later for the floating-point `std::from_chars` its
  standard library uses.

The makefile of this chapter is built and run by the test suite
(`easylocal.makefile-build`, which installs EasyLocal, compiles the quick
start with one command and builds every program the installation's components
allow), so the flags on this page are the flags that work.
