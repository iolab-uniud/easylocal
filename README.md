# EasyLocal++

EasyLocal++ is a framework for modeling and solving combinatorial optimization problems through local search metaheuristics. It is entirely written in C++ and uses template metaprogramming extensively to achieve separation of concerns and performance. 

Typically, to solve a problem, it is sufficient to implement the necessary methods to compute the problem-specific **cost function** and to enumerate the problem-specific **local search moves**. The framework takes care of calling the user-defined hook methods to solve the problem using one of the implemented meta-heuristics (e.g. simulated annealing, tabu search, hill climbing, ...).

This repository contains the last iteration (currently 4.0) of the EasyLocal++ framework. A seed project to use as a starting point for EasyLocal++ projects will soon be available.

## How to install EasyLocal++

The build system of EasyLocal++ is based on [CMake (Cross Platform Make)](http://www.cmake.org). CMake allows one to **generate build scripts** for most platforms and development environments, including Unix Makefiles, Visual Studio, Xcode, Eclipse, etc. (see [CMake Generators](http://www.cmake.org/cmake/help/v3.0/manual/cmake-generators.7.html) for more information on the supported IDEs). The framework is available in the form of a header-only library. Therefore, no compilation is needed. The `CMakeLists.txt` will provide for suitable compiler options, including coroutines and concepts.

In general, to avoid mixing temporary CMake files with the project sources, it is advised to create a dedicated `build` subdirectory where all the building activities happen.



## Citing EasyLocal++

We have invested a lot of time and effort in creating EasyLocal++, if you find this software useful and if you use it for research purposes we would be grateful if you can cite EasyLocal++ in your publications.

The reference papers about EasyLocal++ are:

1. Luca Di Gaspero and Andrea Schaerf. EasyLocal++: An object-oriented framework for flexible design of local search algorithms.  Software — Practice & Experience, 33(8):733–765, July 2003. 
2. Luca Di Gaspero and Andrea Schaerf. Writing local search algorithms using EasyLocal++. In S. Voß and D.L. Woodruff, editors,  Optimization Software Class Libraries, OR/CS. Kluwer Academic Publisher, Boston (MA), USA, 2002.

## License

 Copyright (c) 2001-2015 Sara Ceschia, Luca Di Gaspero, Andrea Schaerf, Tommaso Urli - 
 University of Udine, Italy 
 
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

 3. Neither the name of the copyright holder nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

 4. Redistributions of any form whatsoever must retain the following
    acknowledgment: 'This product includes software developed by 
	"Sara Ceschia, Luca Di Gaspero, Andrea Schaerf, Tommaso Urli,
	University of Udine, Italy" (http://satt.diegm.uniud.it/).'
	
 5. The use of the software in source or binary forms that results in
    a scientific publication must be acknowledged by citing the 
	following paper:
	
	Luca Di Gaspero and Andrea Schaerf. EasyLocal++: An object-oriented 
	framework for flexible design of local search algorithms. 
	Software - Practice & Experience, 33(8):733-765, July 2003.
	
	
	  
```
#!latex

@article{DiSc03,
	    address = {Chirchester, United Kingdom},
	    author = {Di Gaspero, Luca and Schaerf, Andrea},
	    journal = {Software --- Practice \& Experience},
	    month = {July},
	    number = {8},
	    pages = {733--765},
	    publisher = {John Wiley \& Sons},
	    title = {\textsc{EasyLocal++}: An object-oriented framework for flexible design of local search algorithms},
	    volume = {33},
	    year = {2003}
	  }
```


 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
