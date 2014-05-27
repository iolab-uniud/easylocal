How to install EasyLocal++.

The classes composing EasyLocal++ framework are of two types:

- template-free classes (mainly utility classes), which have to be independently
	compiled and form the libEasyLocal++ library;
- template classes, whose source code (contained in a set of include files) 
  have to be compiled against the actual EasyLocal++ application.

In order to install EasyLocal++ the user is required to configure the whole
system (by means of the `configure' script), to build the library and to
install the template classes include files.
The steps for doing that are the following:

- run "./configure" with the options set appropriately for your installation
  (see 'CONFIGURE' note below)

- run "make" 

- OPTIONAL: run "make check" if you want to perform some checks (namely unit testing 
  the nQueens application)
	
- OPTIONAL: run "make doxygen-doc" if you want to build the documentation (Doxygen, 
  see http://www.doxygen.org, is needed to be installed on your system)

- run "make install"

--------------------------------------------------------------------------------

CONFIGURE: a set of options can be passed to the `configure' script. Among others
the main ones are the following (a * indicates the default value):

- --enable-debug, --disable-debug*: asks to compile the library part of the 
  framework without optimization and including the debug informations.

- --enable-threading*, --disable-threading: sets up the system to enable
  the multi-threading capabilities.
	
- --enable-cpu-time*, --disable-cpu-time: sets up the system to employ
  cpu time instead of wall-clock time for computing timeouts.
	
--------------------------------------------------------------------------------



This file is part of EasyLocal++: a C++ Object-Oriented framework
aimed at easing the development of Local Search algorithms.
Copyright (C) 2001--2008 Andrea Schaerf, Luca Di Gaspero. 

We have invested a lot of time and effort in creating EasyLocal++,
if you find this software useful and if you use it for developing
software for scientific publications we will be grateful if you
cite EasyLocal++ in your publications.

In order to do that use:

L. Di Gaspero and A. Schaerf. EasyLocal++: An object-oriented 
framework for flexible design of local search algorithms. 
Software — Practice & Experience, 33(8):733–765, July 2003. 

and/or

L. Di Gaspero and A. Schaerf. Writing local search algorithms 
using EasyLocal++. In S. Voß and D.L. Woodruff, editors, 
Optimization Software Class Libraries, OR/CS. 
Kluwer Academic Publisher, Boston (MA), USA, 2002.

The BibTeX entries for LaTeX users convenience are:

@article{DiSc03,
	Title = {\textsc{EasyLocal++}: An object-oriented framework 
				   for flexible design of local search algorithms},
	Author = {Di Gaspero, Luca and Schaerf, Andrea},
	Journal = {Software --- Practice \& Experience},
	Month = {July},
	Volume = {33},
	Number = {8},
	Pages = {733--765},
	Publisher = {John Wiley \& Sons},
	Address = {Chirchester, United Kingdom},
	Year = {2003}}

@incollection{DiSc02,
	Title = {Writing Local Search Algorithms using \textsc{EasyLocal++}},
	Author = {Di Gaspero, Luca and Schaerf, Andrea},
	Booktitle = {Optimization Software Class Libraries},
	Editor = {Vo\ss{}, Stefan and Woodruff, David L.},
	Publisher = {Kluwer Academic Publisher},
	Address = {Boston (MA), USA},
	Series = {OR/CS},
	Year = {2002}}

EasyLocal++ is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

EasyLocal++ is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with EasyLocal++.  If not, see <http://www.gnu.org/licenses/>.

