# EasyLocal++

EasyLocal++ is a framework for modeling and solving combinatorial optimization problems through local search metaheuristics. The framework is entirely written in C++ and makes broad use of template metaprogramming to achieve both separation of concerns and performance. 

Typically, to solve a problem, it is sufficient to implement the necessary methods to compute the problem-specific **cost function** and to enumerate the problem-specific **local search moves**. The framework takes care of calling the user-defined hook methods to solve the problem using one of the implemented meta-heuristics (e.g. simulated annealing, tabu search, hill climbing, ...).

This repository contains the last iteration (currently 3.0) of the EasyLocal++ framework. Examples of solvers implemented with EasyLocal++ can be found in `easylocalpp-examples`, while a seed project to use as a starting point for EasyLocal++ projects is in `easylocalpp-seedproject`.

## How to install EasyLocal++

The build system of EasyLocal++ is based on [CMake (Cross Platform Make)](http://www.cmake.org). CMake allows one to generate build scripts for most platforms, e.g., Makefiles, Visual Studio Solutions, Xcode Projects, Eclipse Projects, etc. The suggested way to compile EasyLocal++ is the following

    mkdir build
    cd build
    cmake -G <IDE> ..

Check the supported IDEs on your platform by running `cmake --help` (section Generators). Once the build scripts have been generated the project can be built. In case of Makefiles, the following steps must be executed

    make

the build will place a tiny library in the `lib` directory, and the test binaries in `bin`. Projects built with EasyLocal++ must link against the `libEasyLocalpp` library in `lib` and be able to reach the header files in `include`.

## Seed project

TODO.

## Citing EasyLocal++

We have invested a lot of time and effort in creating EasyLocal++, if you find this software useful and if you use it for research purposes we would be grateful if you can cite EasyLocal++ in your publications.

The reference papers about EasyLocal++ are:

1. L. Di Gaspero and A. Schaerf. EasyLocal++: An object-oriented framework for flexible design of local search algorithms.  Software — Practice & Experience, 33(8):733–765, July 2003. 
2. L. Di Gaspero and A. Schaerf. Writing local search algorithms using EasyLocal++. In S. Voß and D.L. Woodruff, editors,  Optimization Software Class Libraries, OR/CS. Kluwer Academic Publisher, Boston (MA), USA, 2002.

## License

EasyLocal++ is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

EasyLocal++ is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details. 

You should have received a copy of the GNU General Public License along with EasyLocal++. If not, see <http://www.gnu.org/licenses/>.

