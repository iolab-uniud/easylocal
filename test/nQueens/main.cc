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
 are respectively @a int (for @ref Input), @ref ChessBoard, @a std::vector<int> 
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
 (@ref FilledDeltaCostComponent), which avoids to really apply the move to 
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
#include <helpers/MultimodalNeighborhoodExplorer.hh>
#include <helpers/MultimodalTabuListManager.hh>
#include <kickers/QueensKicker.hh>
#include <observers/RunnerObserver.hh>
#include <observers/GeneralizedLocalSearchObserver.hh>
#include <runners/HillClimbing.hh>
#include <runners/SteepestDescent.hh>
#include <runners/TabuSearch.hh>
#include <runners/SimulatedAnnealing.hh>
#include <runners/TabuSearchWithShiftingPenalty.hh>
#include <solvers/SimpleLocalSearch.hh>
#include <solvers/GeneralizedLocalSearch.hh>
#include <solvers/VariableNeighborhoodDescent.hh>
#include <testers/Tester.hh>
#include <testers/MoveTester.hh>
#include <testers/KickerTester.hh>
#include <utils/CLParser.hh>
#include <utils/Chronometer.hh>
#include <kickers/MultimodalKicker.hh>

int main(int argc, char* argv[])
{
	// The CLParser object parses the command line arguments
  CLParser cl(argc, argv);
	// The set of arguments added are the following:
  ValArgument<int> arg_size("size", "s", true, cl); /*< The size of the chessboard, that is, the input of the problem */
  ValArgument<std::string> arg_solmethod("method", "m", false, cl);
  ValArgument<unsigned> arg_plot_level("plot", "p", false, 0u, cl);
  ValArgument<unsigned> arg_verbosity_level("verbose", "v", false, 0u, cl);
  ValArgument<double> arg_timeout("timeout", "to", false, 0.0, cl);
  ValArgument<int> arg_random_seed("random_seed", "rs", false, cl);
  cl.MatchArgument(arg_size);
  
  // data classes
  int in(arg_size.GetValue());
  
  // cost components
  PrimaryDiagonalCostComponent cc1(in);
  SecondaryDiagonalCostComponent cc2(in);
  PrimaryDiagonalDeltaCostComponent dcc1(in, cc1);
  SecondaryDiagonalDeltaCostComponent dcc2(in, cc2);
  //EmptyDeltaCostComponent<int, std::vector<int>, Swap> dcc1(in, cc1, "Primary diagonal delta");
  //EmptyDeltaCostComponent<int, std::vector<int>, Swap> dcc2(in, cc2, "Secondary diagonal delta");
  
  // helpers
  QueensStateManager qsm(in);
  QueensTabuListManager qtlm;
  SwapNeighborhoodExplorer qnhe(in, qsm);
  QueensOutputManager qom(in);
    
  // kickers
  QueensKicker qk(in, qnhe);
  
  // runners
  HillClimbing<int, std::vector<int>, Swap> qhc(in, qsm, qnhe, "SwapHillClimbing", cl);
  SteepestDescent<int, std::vector<int>, Swap> qsd(in, qsm, qnhe, "SwapSteepestDescent");
  TabuSearch<int, std::vector<int>, Swap> qts(in, qsm, qnhe, qtlm, "SwapTabuSearch", cl);
  SimulatedAnnealing<int, std::vector<int>, Swap> qsa(in, qsm, qnhe, "SwapSimulatedAnnealing", cl);
  TabuSearchWithShiftingPenalty<int, std::vector<int>, Swap> qtsw(in, qsm, qnhe, qtlm, "SwapTabuSearchWithShiftingPenalty", cl);
  
  SimpleLocalSearch<int, ChessBoard, std::vector<int> > qss(in, qsm, qom, "QueensSLS", cl);
  GeneralizedLocalSearch<int, ChessBoard, std::vector<int> > qgls(in, qsm, qom, "QueensGLS", cl);
  VariableNeighborhoodDescent<int, ChessBoard, std::vector<int> > qvnd(in, qsm, qom, 3);
  
  typedef PrepareSetUnionNeighborhoodExplorerTypes<int, std::vector<int>, TYPELIST_2(SwapNeighborhoodExplorer, SwapNeighborhoodExplorer)> MultimodalTypes;
  typedef MultimodalTypes::MoveList DoubleSwap; // this line is not mandatory, it just aliases the movelist type for reader's convenience
  MultimodalTypes::NeighborhoodExplorer qmmnhe(in, qsm, "Multimodal Swap");
  qmmnhe.AddNeighborhoodExplorer(qnhe);
  qmmnhe.AddNeighborhoodExplorer(qnhe);
  typedef PrepareSetUnionTabuListManager<std::vector<int>, TYPELIST_2(QueensTabuListManager, QueensTabuListManager)> MultimodalTabuListManagerTypes;
  MultimodalTabuListManagerTypes::TabuListManager qmmtlm;
  qmmtlm.AddTabuListManager(qtlm);
  qmmtlm.AddTabuListManager(qtlm);
  TabuSearch<int, std::vector<int>, DoubleSwap> qmmts(in, qsm, qmmnhe, qmmtlm, "DoubleSwapTabuSearch", cl); 
  /* typedef PrepareCartesianProductNeighborhoodExplorerTypes<int, std::vector<int>, TYPELIST_2(SwapNeighborhoodExplorer, SwapNeighborhoodExplorer)> MultimodalTypes;
  typedef MultimodalTypes::MoveList DoubleSwap; // this line is not mandatory, it just aliases the movelist type for reader's convenience
  MultimodalTypes::NeighborhoodExplorer qmmnhe(in, qsm, "Multimodal Swap");
  qmmnhe.AddNeighborhoodExplorer(qnhe);
  qmmnhe.AddNeighborhoodExplorer(qnhe);
  typedef PrepareCartesianProductTabuListManager<std::vector<int>, TYPELIST_2(QueensTabuListManager, QueensTabuListManager)> MultimodalTabuListManagerTypes;
  MultimodalTabuListManagerTypes::TabuListManager qmmtlm;
  qmmtlm.AddTabuListManager(qtlm);
  qmmtlm.AddTabuListManager(qtlm);
  TabuSearch<int, std::vector<int>, DoubleSwap> qmmts(in, qsm, qmmnhe, qmmtlm, "DoubleSwapTabuSearch", cl); */
  
  
  class QueensKicker2 : public MultimodalKicker<int,std::vector<int>,DoubleSwap>
  {
  public:
    QueensKicker2(const int& bs, MultimodalTypes::NeighborhoodExplorer& qnhe, int s = 2)
    : MultimodalKicker<int,std::vector<int>,DoubleSwap>(bs, qnhe, s, "QueensKicker2") 
    {}
    bool RelatedMoves(const DoubleSwap&, const DoubleSwap&) const
    { return true; } 
  } qk2(in, qmmnhe);  	  
  

  cl.MatchArguments();
  if (arg_random_seed.IsSet())
    Random::Seed(arg_random_seed.GetValue());
  
  RunnerObserver<int, std::vector<int>, Swap> ro(arg_verbosity_level.GetValue(), arg_plot_level.GetValue());
  GeneralizedLocalSearchObserver<int, ChessBoard, std::vector<int> > so(arg_verbosity_level.GetValue(), arg_plot_level.GetValue());
	
	if (arg_plot_level.IsSet())
	{
		qhc.AttachObserver(ro);
		qsd.AttachObserver(ro);
		qts.AttachObserver(ro);
		qsa.AttachObserver(ro);
	}
	if (arg_verbosity_level.IsSet())
		qgls.AttachObserver(so);

  qvnd.SetKicker(qk);
  
  qgls.SetKicker(qk);

  qsm.AddCostComponent(cc1);
  qsm.AddCostComponent(cc2);
  qnhe.AddDeltaCostComponent(dcc1);
  qnhe.AddDeltaCostComponent(dcc2);
	
  if (!arg_solmethod.IsSet())
	{
		// testers
		MoveTester<int, ChessBoard, std::vector<int>, Swap> swap_move_test(in,qsm,qom,qnhe, "Swap move");
    MoveTester<int, ChessBoard, std::vector<int>, DoubleSwap> multimodal_move_test(in,qsm,qom,qmmnhe, "Multimodal swap move");
		KickerTester<int, ChessBoard, std::vector<int> > monokicker_test(in,qsm,qom, qk, "Monomodal kick");
    KickerTester<int, ChessBoard, std::vector<int> > multikicker_test(in,qsm,qom, qk2, "Multimodal kick");
		Tester<int, ChessBoard, std::vector<int> > tester(in,qsm,qom);
		
		tester.AddMoveTester(swap_move_test);
    tester.AddMoveTester(multimodal_move_test);
		tester.AddKickerTester(monokicker_test);
    tester.AddKickerTester(multikicker_test);
		tester.AddRunner(qhc);
		tester.AddRunner(qsd);
		tester.AddRunner(qts);
		tester.AddRunner(qsa);  
		tester.AddRunner(qtsw);
    tester.AddRunner(qmmts);
		
		tester.RunMainMenu();
	}
  else
	{
		if (arg_solmethod.GetValue() == "VND") 
		{
			qvnd.Solve();
			std::cout << qvnd.GetOutput() << std::endl << qvnd.GetCurrentCost() << std::endl;
		}
		else if (arg_solmethod.GetValue() == "GLS") 
		{
			qgls.AddRunner(qhc);
			qgls.AddRunner(qsd);
			Chronometer chrono;
			if (arg_timeout.IsSet())
				qgls.SetTimeout(arg_timeout.GetValue());
			chrono.Start();
			qgls.GeneralSolve();
			chrono.Stop();
			std::cout << qgls.GetOutput() << std::endl << qgls.GetCurrentCost() << ' ' << chrono.TotalTime() << std::endl;
		}
		
	}
	
  return 0;
}
