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
 
 @version 3.0-r$Revision$
 @date $Date$
 @author Luca Di Gaspero, Andrea Schaerf
 */

#include <iostream>     
#include "data/ChessBoard.hh"
#include "data/Swap.hh"
#include "helpers/SwapNeighborhoodExplorer.hh"
#include "helpers/MultimodalNeighborhoodExplorer.hh"
#include "helpers/QueensOutputManager.hh"
#include "helpers/QueensStateManager.hh"
#include "helpers/QueensTabuListManager.hh"
#include "helpers/PrimaryDiagonalCostComponent.hh"
#include "helpers/PrimaryDiagonalDeltaCostComponent.hh"
#include "helpers/SecondaryDiagonalCostComponent.hh"
#include "helpers/SecondaryDiagonalDeltaCostComponent.hh"
#include <helpers/DeltaCostComponent.hh>
#include "kickers/QueensKicker.hh"
#include <observers/RunnerObserver.hh>
#include <observers/GeneralizedLocalSearchObserver.hh>
#include <runners/HillClimbing.hh>
#include <runners/SteepestDescent.hh>
#include <runners/TabuSearch.hh>
#include <runners/SampleTabuSearch.hh>
#include <runners/FirstImprovementTabuSearch.hh>
#include <runners/TabuSearchWithShiftingPenalty.hh>
#include <runners/GreatDeluge.hh>
#include <runners/SimulatedAnnealing.hh>
#include <runners/SimulatedAnnealingWithReheating.hh>
#include <runners/LateAcceptanceHillClimbing.hh>
#include <solvers/SimpleLocalSearch.hh>
#include <solvers/TokenRingSearch.hh>
#include <solvers/VariableNeighborhoodDescent.hh>
#include <testers/Tester.hh>
#include <testers/MoveTester.hh>
#include <testers/KickerTester.hh>
#include <helpers/MultimodalTabuListManager.hh>
//#include <boost/program_options/options_description.hpp>
//#include <boost/program_options/parsers.hpp>
//#include <boost/program_options/variables_map.hpp>

//#include <kickers/MultimodalKicker.hh>
using namespace std;

int main(int argc, const char* argv[])
{
  try
  {
    // input classe
    int in;
    
    // Since we plan to save the seed we need to generate one
    Random::Seed(Random::Int());

    ParameterBox main_parameters("main", "Main Program options");
    // Main program parameters
    Parameter<int> size("size", "Chessboard size", main_parameters);
    Parameter<std::string> solution_method("method", "Solution method (none for tester)", main_parameters);
    Parameter<unsigned int> verbosity_level("verbosity", "Verbosity level", main_parameters);
    Parameter<unsigned int> plot_level("plot", "Plot level", main_parameters);
    Parameter<unsigned long int> random_seed("random_seed", "Random seed", main_parameters);
    
    // cost components
    PrimaryDiagonalCostComponent cc1(in);
    SecondaryDiagonalCostComponent cc2(in);
    PrimaryDiagonalDeltaCostComponent dcc1(in, cc1);
    SecondaryDiagonalDeltaCostComponent dcc2(in, cc2);
    
    // helpers
    QueensStateManager qsm(in);
    QueensTabuListManager qtlm;
    SwapNeighborhoodExplorer qnhe(in, qsm);
    
    // compose a multimodal neighborhood explorer
    
    SetUnionNeighborhoodExplorer<int, std::vector<int>, int, decltype(qnhe), decltype(qnhe), decltype(qnhe)> qnheumm(in, qsm, "SwapUnion", qnhe, qnhe, qnhe);
    
    SetUnionTabuListManager<std::vector<int>, int, decltype(qtlm), decltype(qtlm), decltype(qtlm)> qtlmumm(qtlm, qtlm, qtlm);
    CartesianProductTabuListManager<std::vector<int>, int, decltype(qtlm), decltype(qtlm), decltype(qtlm)> qtlmxmm(qtlm, qtlm, qtlm);
    
    CartesianProductNeighborhoodExplorer<int, std::vector<int>, int, decltype(qnhe), decltype(qnhe), decltype(qnhe)> qnhexmm(in, qsm, "SwapProduct", qnhe, qnhe, qnhe);
    
    /*
    std::tuple<std::reference_wrapper<decltype(qnhe)>, std::reference_wrapper<decltype(qnhe)>, std::reference_wrapper<decltype(qnhe)>> qnhe3 = make_tuple(
      std::reference_wrapper<decltype(qnhe)>(qnhe),
      std::reference_wrapper<decltype(qnhe)>(qnhe),
      std::reference_wrapper<decltype(qnhe)>(qnhe)
    );
    
    PowerNeighborhoodExplorer<int, std::vector<int>, int, decltype(qnhe), 3> qnhexmm(in, qsm, "SwapProduct", qnhe3);
    */
    
    QueensOutputManager qom(in);
    
    // kickers
    QueensKicker qk(in, qnhe);
    
    // runners
    HillClimbing<int, vector<int>, Swap, int> qhc(in, qsm, qnhe, "SwapHillClimbing");
    SteepestDescent<int, vector<int>, Swap, int> qsd(in, qsm, qnhe, "SwapSteepestDescent");
    TabuSearch<int, vector<int>, Swap, int> qts(in, qsm, qnhe, qtlm, "SwapTabuSearch");
    SampleTabuSearch<int, vector<int>, Swap, int> qsts(in, qsm, qnhe, qtlm, "SwapSampleTabuSearch");
    FirstImprovementTabuSearch<int, vector<int>, Swap, int> qfits(in, qsm, qnhe, qtlm, "SwapFirstImprovementTabuSearch");
    TabuSearch<int, vector<int>, decltype(qnheumm)::ThisMove, int> qtsmm(in, qsm, qnheumm, qtlmumm, "MultiModalTabuSearch");
    SimulatedAnnealing<int, vector<int>, Swap, int> qsa(in, qsm, qnhe, "SwapSimulatedAnnealing");
    SimulatedAnnealingWithReheating<int, vector<int>, Swap, int> qsawr(in, qsm, qnhe, "SwapSimulatedAnnealingWithReheating");
    LateAcceptanceHillClimbing<int, vector<int>, Swap, int> qlhc(in, qsm, qnhe, "SwapLateAcceptanceHillClimbing");
    GreatDeluge<int, vector<int>, Swap, int> qgd(in, qsm, qnhe, "SwapGreatDeluge");
    TabuSearchWithShiftingPenalty<int, vector<int>, Swap, int> qtsw(in, qsm, qnhe, qtlm, "SwapTabuSearchWithShiftingPenalty");
    
    SimpleLocalSearch<int, ChessBoard, vector<int> , int> qss(in, qsm, qom, "QueensSLS");
    TokenRingSearch<int, ChessBoard, vector<int> , int> qtr(in, qsm, qom, "QueensTR");
    //VariableNeighborhoodDescent<int, ChessBoard, vector<int>, int > qvnd(in, qsm, qom, 3, "VNDS");
        
    // parse all command line parameters, including those posted by runners and solvers
    if (!CommandLineParameters::Parse(argc, argv, true))
      return 1;
    
    in = size;
    
    if (random_seed.IsSet())
      Random::Seed(random_seed);
    
    std::cout << "Random seed: " << Random::seed << std::endl;
    qsm.AddCostComponent(cc1);
    qsm.AddCostComponent(cc2);
    qnhe.AddDeltaCostComponent(dcc1);
    qnhe.AddDeltaCostComponent(dcc2);
    
    if (plot_level.IsSet() && verbosity_level.IsSet())
    {
      RunnerObserver<int, vector<int>, Swap, int> ro(verbosity_level, plot_level);
      GeneralizedLocalSearchObserver<int, ChessBoard, vector<int> , int> so(verbosity_level, plot_level);
      
      qhc.AttachObserver(ro);
      qsd.AttachObserver(ro);
      qts.AttachObserver(ro);
      qsa.AttachObserver(ro);
      qlhc.AttachObserver(ro);
    }
    
    
    //  qvnd.SetKicker(qk);
    
    if (!solution_method.IsSet())
    {
      // tester
      // FIXME: now the tester should be defined only when it is used (because of State management)
      Tester<int, ChessBoard, vector<int>, int > tester(in, qsm, qom);
      // testers
      MoveTester<int, ChessBoard, vector<int>, Swap, int> swap_move_test(in,qsm,qom,qnhe, "Swap move", tester);
      MoveTester<int, ChessBoard, vector<int>, decltype(qnheumm)::ThisMove, int> multimodal_move_test_union(in,qsm,qom,qnheumm, "Multimodal union swap move", tester);
      MoveTester<int, ChessBoard, vector<int>, decltype(qnhexmm)::ThisMove, int> multimodal_move_test_product(in,qsm,qom,qnhexmm, "Multimodal product swap move", tester);
      KickerTester<int, ChessBoard, vector<int>, int> monokicker_test(in,qsm,qom, qk, "Monomodal kick");
      //KickerTester<int, ChessBoard, vector<int> > multikicker_test(in,qsm,qom, qk2, "Multimodal kick");
      
      tester.AddKickerTester(monokicker_test);
      //tester.AddKickerTester(multikicker_test);
      
      tester.RunMainMenu();
    }
    else if (solution_method == std::string("simple"))
    {
      qss.SetRunner(qhc);
      
      std::tuple<ChessBoard, int, int, double> res = qss.Solve();
      
      std::cout << std::get<0>(res) << std::endl;
      std::cout << "Violations: " << std::get<1>(res) << std::endl;
      std::cout << "Objectives: " << std::get<2>(res) << std::endl;
      std::cout << "Timeout: " << std::get<3>(res) << " s" << std::endl;
    }
    
    /*
     else if (solution_method == "VND")
     {
     qvnd.Solve();
     cout << qvnd.GetOutput() << endl << qvnd.GetCurrentCost() << endl;
     } */
  }
  catch (std::logic_error e)
  {
    std::cerr << "Exception: " << e.what() << std::endl;
    return 1;
  }
	
  return 0;
}
