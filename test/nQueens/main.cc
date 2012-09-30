// $Id$
// This file is part of EasyLocalpp: a C++ Object-Oriented framework
// aimed at easing the development of Local Search algorithms.
// Copyright (C) 2001--2008 Andrea Schaerf, Luca Di Gaspero. 
//
// EasyLocalpp is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// EasyLocalpp is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with EasyLocalpp. If not, see <http://www.gnu.org/licenses/>.

/** 
 @file main.cc
 
 @brief Main file for the nQueens application.
 
 @details
 This file contains the main function of the nQueens test 
 application. It constructs and links all the objects of an EasyLocalpp 
 application (Helpers, Runners, Kickers, and Solvers), and it sets up 
 the default text user interface provided by the @ref Tester "Tester"s.
 
 @page nQueens A case study: the nQueens problem
 
 The nQueens problem is the problem of placing @a n queens on
 a @a n x @a n chessboard so that no pair of queens attack each other.
 
 In order to solve the problem with Local Search through the EasyLocalpp 
 framework we have first to define the data classes, namely the classes 
 handling the @ref Input and the @ref Output of the problem and the 
 classes for the @ref State representation of the Search Space and 
 the @ref Move features. In the case of the nQueens problem these classes
 are respectively @a int (for @ref Input), @ref ChessBoard, @a vector<int> 
 (for @ref State, representing a permutation, whose values correspond to the rows 
 assigned to each queen) and @ref Swap.
 
 Then a few @ref Helper classes should be defined to manage the data
 objects. 
 
 The @ref StateManager (@ref QueensStateManager) manipulates 
 the rows permutation vector. The cost function on a given state is
 computed by a set of @ref CostComponent "CostComponent"s; in this application
 there are two of those, namely dealing with queens' attacks on the
 primary diagonals (i.e., up-left to down-right) and on the
 secondary diagonals (up-right to down-left).
 
 The class @ref SwapExplorer is a @ref NeighborhoodExplorer, which
 prescribes the strategy for enumerating all the @ref Move "Move"s in 
 the neighborhood of a given @ref State. The @ref DeltaCostComponent "DeltaCostComponent"s
 compute the difference of the cost function due to the application
 of a @ref Move; in this application they have a concrete implementation 
 (@ref DeltaCostComponent), which avoids to really apply the move to 
 the curent state for evaluating its effects but, for efficiency 
 purposes, it only simulates its behavior.
 
 A few other helpers are needed: a @ref TabuListManager 
 (and a @ref FrequencyTabuListManager for long-term memory management) is
 needed to manage the prohibition mechanisms of @ref TabuSearch, an 
 @ref OutputManager for handling the translations between the @ref State
 (a permutation) and the @ref Output.
 
 In this example we also make use of a @ref Kicker, an intensification
 component that can be used to implement a Variable Neighborhood exploration
 strategy.
 
 All these classes are instantiated in the file @ref main.cc.
 
 We also provide an example of using the Unit Testing facilities coming
 with the framework. These facilities are abstractly implemented in EasyLocalpp
 and a set of concrete derived classes is needed to instantiate the proper 
 templates and to create and load data for the concrete needed objects.
 
 @see UnitTesting for more details about setting up the classes for automatic unit testing.
 
 @version 1.0-r$Revision$
 @date $Date$
 @author Luca Di Gaspero, Andrea Schaerf
 */

#include <iostream>     
#include <data/ChessBoard.hh>
#include <data/Swap.hh>
#include <helpers/SwapNeighborhoodExplorer.hh>
#include <helpers/QueensOutputManager.hh>
#include <helpers/QueensStateManager.hh>
#include <helpers/QueensTabuListManager.hh>
#include <helpers/PrimaryDiagonalCostComponent.hh>
#include <helpers/PrimaryDiagonalDeltaCostComponent.hh>
#include <helpers/SecondaryDiagonalCostComponent.hh>
#include <helpers/SecondaryDiagonalDeltaCostComponent.hh>
#include <helpers/DeltaCostComponent.hh>
#include <kickers/QueensKicker.hh>
#include <observers/RunnerObserver.hh>
#include <observers/GeneralizedLocalSearchObserver.hh>
#include <runners/HillClimbing.hh>
#include <runners/SteepestDescent.hh>
#include <runners/TabuSearch.hh>
#include <runners/SimulatedAnnealing.hh>
#include <runners/SimulatedAnnealingWithReheating.hh>
#include <runners/LateAcceptanceHillClimbing.hh>
#include <solvers/SimpleLocalSearch.hh>
#include <solvers/VariableNeighborhoodDescent.hh>
#include <testers/Tester.hh>
#include <testers/MoveTester.hh>
#include <testers/KickerTester.hh>
#include <chrono>
#if defined(HAVE_LINKABLE_BOOST)
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#endif

//#include <kickers/MultimodalKicker.hh>

using namespace std;
using namespace chrono;
typedef std::chrono::duration<double, std::ratio<1>> secs;

int main(int argc, char* argv[])
{
	// The CLParser object parses the command line arguments
  /* ValArgument<double> arg_timeout("timeout", "to", false, 0.0, cl);
  ValArgument<int> arg_random_seed("random_seed", "rs", false, cl); */
  //cl.MatchArgument(arg_size);
  
  // input classe
  int in;
  
#if defined(HAVE_LINKABLE_BOOST)
  ParameterBox main_parameters("main", "Main Program options");
  // Main program parameters
  Parameter<int> size("size", "Chessboard size", main_parameters);
  Parameter<std::string> solution_method("method", "Solution method (none for tester)", main_parameters);
  Parameter<unsigned int> verbosity_level("verbosity", "Verbosity level", main_parameters);
  Parameter<unsigned int> plot_level("plot", "Plot level", main_parameters);
  Parameter<unsigned long> random_seed("random_seed", "Random seed", main_parameters);
  
  boost::program_options::variables_map vm_for_input;
  boost::program_options::parsed_options parsed_for_input = boost::program_options::command_line_parser(argc, argv).options(main_parameters.cl_options).allow_unregistered().run();

  boost::program_options::store(parsed_for_input, vm_for_input);
  boost::program_options::notify(vm_for_input);
  
  bool terminate = true;

  if (size.IsSet())
  {
    in = size;
    terminate = false;
  }
  
#else
  // the value of in shoud be read from the command line
#endif
  
  // cost components
  PrimaryDiagonalCostComponent cc1(in);
  SecondaryDiagonalCostComponent cc2(in);
  PrimaryDiagonalDeltaCostComponent dcc1(in, cc1);
  SecondaryDiagonalDeltaCostComponent dcc2(in, cc2);
  
  // helpers
  QueensStateManager qsm(in);
  QueensTabuListManager qtlm;
  SwapNeighborhoodExplorer qnhe(in, qsm);
  QueensOutputManager qom(in);
  
  // tester
  Tester<int, ChessBoard, vector<int> > tester(in,qsm,qom);
    
  // kickers
  QueensKicker qk(in, qnhe);
  
  // runners
  HillClimbing<int, vector<int>, Swap> qhc(in, qsm, qnhe, "SwapHillClimbing");
  tester.AddRunner(qhc);
  SteepestDescent<int, vector<int>, Swap> qsd(in, qsm, qnhe, "SwapSteepestDescent");
  tester.AddRunner(qsd);
  TabuSearch<int, vector<int>, Swap> qts(in, qsm, qnhe, qtlm, "SwapTabuSearch");
  tester.AddRunner(qts);
  SimulatedAnnealing<int, vector<int>, Swap> qsa(in, qsm, qnhe, "SwapSimulatedAnnealing");
  tester.AddRunner(qsa);
  SimulatedAnnealingWithReheating<int, vector<int>, Swap> qsawr(in, qsm, qnhe, "SwapSimulatedAnnealingWithReheating");
  tester.AddRunner(qsawr);
  LateAcceptanceHillClimbing<int, vector<int>, Swap> qlhc(in, qsm, qnhe, "LateAcceptanceHillClimbing");
  tester.AddRunner(qlhc);
  
  //TabuSearchWithShiftingPenalty<int, vector<int>, Swap> qtsw(in, qsm, qnhe, qtlm, "SwapTabuSearchWithShiftingPenalty", cl);
  
  SimpleLocalSearch<int, ChessBoard, vector<int> > qss(in, qsm, qom, "QueensSLS");
  VariableNeighborhoodDescent<int, ChessBoard, vector<int> > qvnd(in, qsm, qom, 3);  

  if (random_seed.IsSet())
    Random::Seed(random_seed);
  
#if defined(HAVE_LINKABLE_BOOST)
  boost::program_options::options_description cmdline_options; 
  boost::program_options::variables_map vm;
  for (auto pb : ParameterBox::overall_parameters)
    cmdline_options.add(pb->cl_options);
  
  cmdline_options.add_options()
  ("help", "Produce help message");
  
  boost::program_options::parsed_options parsed = boost::program_options::command_line_parser(argc, argv).options(cmdline_options).allow_unregistered().run();
  
  std::vector<std::string> unrecognized_options = boost::program_options::collect_unrecognized(parsed.options, boost::program_options::include_positional);
  
  if (unrecognized_options.size() > 0)
  {
    std::cout << "Unrecognized options: ";
    for (const std::string& o : unrecognized_options)
      std::cout << o << " ";
    std::cout << std::endl << "Run " << argv[0] << " --help for the allowed options" << std::endl;
    return 1;
  }
  boost::program_options::store(parsed, vm);
  boost::program_options::notify(vm);
  
  if (vm.count("help") || terminate)
  {
    std::cout << cmdline_options << std::endl;
    return 1;
  }
#endif
  
  qsm.AddCostComponent(cc1);
  qsm.AddCostComponent(cc2);
  qnhe.AddDeltaCostComponent(dcc1);
  qnhe.AddDeltaCostComponent(dcc2);
	
	if (plot_level.IsSet() && verbosity_level.IsSet())
	{
    RunnerObserver<int, vector<int>, Swap> ro(verbosity_level, plot_level);
    GeneralizedLocalSearchObserver<int, ChessBoard, vector<int> > so(verbosity_level, plot_level);

		qhc.AttachObserver(ro);
		qsd.AttachObserver(ro);
		qts.AttachObserver(ro);
		qsa.AttachObserver(ro);
    qlhc.AttachObserver(ro);
	}


  //  qvnd.SetKicker(qk);  
	
  if (!solution_method.IsSet())
	{
		// testers
		MoveTester<int, ChessBoard, vector<int>, Swap> swap_move_test(in,qsm,qom,qnhe, "Swap move", tester);
    // MoveTester<int, ChessBoard, vector<int>, DoubleSwap> multimodal_move_test(in,qsm,qom,qmmnhe, "Multimodal swap move", tester);
		KickerTester<int, ChessBoard, vector<int> > monokicker_test(in,qsm,qom, qk, "Monomodal kick");
    //KickerTester<int, ChessBoard, vector<int> > multikicker_test(in,qsm,qom, qk2, "Multimodal kick");
		
		tester.AddKickerTester(monokicker_test);
    //tester.AddKickerTester(multikicker_test);	
		
		tester.RunMainMenu();
	}
  /*
  else if (solution_method == "VND")
  {
    qvnd.Solve();
    cout << qvnd.GetOutput() << endl << qvnd.GetCurrentCost() << endl;
  } */
	
  return 0;
}
