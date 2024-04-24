# EasyLocal++

EasyLocal++ is a framework for modeling and solving combinatorial optimization problems through local search metaheuristics. It is entirely written in C++ and extensively uses template metaprogramming to separate concerns and improve performance. 

Typically, to solve a problem, it is sufficient to implement the necessary methods to compute the problem-specific **cost function** and to enumerate the problem-specific **local search moves**. The framework takes care of calling the user-defined hook methods to solve the problem using one of the implemented meta-heuristics (e.g. simulated annealing, tabu search, hill climbing, ...).

This repository contains the last iteration (currently 4.0) of the EasyLocal++ framework. A seed project to use as a starting point for EasyLocal++ projects will soon be available.

## How to install EasyLocal++

The build system of EasyLocal++ is based on [CMake (Cross Platform Make)](http://www.cmake.org). CMake allows one to **generate build scripts** for most platforms and development environments, including Unix Makefiles, Visual Studio, Xcode, Eclipse, etc. (see [CMake Generators](http://www.cmake.org/cmake/help/v3.0/manual/cmake-generators.7.html) for more information on the supported IDEs). The framework is available in the form of a header-only library. Therefore, no compilation is needed. The `CMakeLists.txt` will provide for suitable compiler options, including coroutines and concepts.

In general, to avoid mixing temporary CMake files with the project sources, it is advised to create a dedicated `build` subdirectory where all the building activities happen.

## Citing EasyLocal++

We have invested a lot of time and effort in creating EasyLocal++; if you find this software useful and if you use it for research purposes, we would be grateful if you could cite EasyLocal++ in your publications.

The reference papers about EasyLocal++ are:

1. Luca Di Gaspero and Andrea Schaerf. EasyLocal++: An object-oriented framework for flexible design of local search algorithms.  Software — Practice & Experience, 33(8):733–765, July 2003. 
2. Luca Di Gaspero and Andrea Schaerf. Writing local search algorithms using EasyLocal++. In S. Voß and D.L. Woodruff, editors,  Optimization Software Class Libraries, OR/CS. Kluwer Academic Publisher, Boston (MA), USA, 2002.

## License

 Copyright (c) 2001-2024 Sara Ceschia, Francesca Da Ros, Luca Di Gaspero, Andrea Schaerf, Tommaso Urli - University of Udine, Italy 
 
 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
