# EasyLocal++

EasyLocal++ is a framework for modeling and solving combinatorial optimization problems through local search metaheuristics. The framework is entirely written in C++ and makes broad use of template metaprogramming to achieve both separation of concerns and performance. 

Typically, to solve a problem, it is sufficient to implement the necessary methods to compute the problem-specific **cost function** and to enumerate the problem-specific **local search moves**. The framework takes care of calling the user-defined hook methods to solve the problem using one of the implemented meta-heuristics (e.g. simulated annealing, tabu search, hill climbing, ...).

This repository contains the last iteration (currently 3.0) of the EasyLocal++ framework. Examples of solvers implemented with EasyLocal++ can be found in `easylocalpp-examples`, while a seed project to use as a starting point for EasyLocal++ projects is in `easylocalpp-seedproject`.

## How to install EasyLocal++

The build system of EasyLocal++ is based on [CMake (Cross Platform Make)](http://www.cmake.org). CMake allows one to **generate build scripts** for most platforms and development environment, including Unix Makefiles, Visual Studio, Xcode, Eclipse, etc. (see [CMake Generators](http://www.cmake.org/cmake/help/v3.0/manual/cmake-generators.7.html) for more information on the supported IDEs).

In general, to avoid mixing temporary CMake files with the project sources, it is advised to create a dedicated `build` subdirectory where all the building activities happen.

### Generating Unix Makefiles

If your would like CMake to generate for you a suite of Unix Makefiles to build EasyLocal++, proceed as follows

	mkdir build
	cd build
	cmake ..
	
then, to build and install EasyLocal on your system, run in `build` the following commands

    make
    make install

`make` will generate a `libEasyLocal.a` static library in the `lib` subdirectory, while `make install` will install both the library and the headers in the `lib` and `include/easylocal` directories under `/usr/local`, which is the recommended option. To replace `/usr/local` with something different, redefine the `CMAKE_INSTALL_PREFIX` variable by passing it to cmake in the following way

	cmake -DCMAKE_INSTALL_PREFIX <prefix> ..

### Generating an Xcode project

If you use Xcode, the commands to generate the EasyLocal++ Xcode project are

    mkdir build
    cd build
    cmake -G Xcode ..
    
this will generate a `EasyLocal.xcodeproj` file in the `build` subdirectory. This project is already configured to build EasyLocal and install it on the system.

### Generating a Visual Studio solution

Similarly to Xcode, CMake can generate a visual studio solution with the command

	cmake -G "Visual Studio <version>"

## Seed project

TODO.

## Citing EasyLocal++

We have invested a lot of time and effort in creating EasyLocal++, if you find this software useful and if you use it for research purposes we would be grateful if you can cite EasyLocal++ in your publications.

The reference papers about EasyLocal++ are:

1. Luca Di Gaspero and Andrea Schaerf. EasyLocal++: An object-oriented framework for flexible design of local search algorithms.  Software — Practice & Experience, 33(8):733–765, July 2003. 
2. Luca Di Gaspero and Andrea Schaerf. Writing local search algorithms using EasyLocal++. In S. Voß and D.L. Woodruff, editors,  Optimization Software Class Libraries, OR/CS. Kluwer Academic Publisher, Boston (MA), USA, 2002.

## License

EasyLocal++ is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

EasyLocal++ is distributed in the hope that it will be useful, but **WITHOUT ANY WARRANTY**; without even the implied warranty of
**MERCHANTABILITY** or **FITNESS FOR A PARTICULAR PURPOSE**.  See the GNU General Public License for more details. 

You should have received a copy of the GNU General Public License along with EasyLocal++. If not, see [http://www.gnu.org/licenses](http://www.gnu.org/licenses).

