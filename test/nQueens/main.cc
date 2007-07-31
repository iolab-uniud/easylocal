/** @file main.cpp
    @brief Main file

    This file contains the main function of the n-Queens applications.
    It declares all the objects (Helpers, Runners and Solvers), and
    it passes them to the default text user interface provided by
    the EasyLocal++ framework.

    @author Andrea Schaerf (schaerf@uniud.it), Luca Di Gaspero (l.digaspero@uniud.it) */

#include <iostream>     
#include "data/ChessBoard.hh"
#include "data/Swap.hh"
#include "helpers/QueensFrequencyTabuListManager.hh"
#include "helpers/QueensNeighborhoodExplorer.hh"
#include "helpers/QueensOutputManager.hh"
#include "helpers/QueensStateManager.hh"
#include "helpers/QueensTabuListManager.hh"
#include "helpers/PrimaryDiagonalCostComponent.hh"
#include "helpers/PrimaryDiagonalDeltaCostComponent.hh"
#include "helpers/SecondaryDiagonalCostComponent.hh"
#include "helpers/SecondaryDiagonalDeltaCostComponent.hh"
#include <helpers/DeltaCostComponent.hh>
#include "kickers/QueensKicker.hh"
#include <runners/HillClimbing.hh>
#include <runners/SteepestDescent.hh>
#include <runners/TabuSearch.hh>
#include <runners/SimulatedAnnealing.hh>
#include <extra/runners/TabuSearchWithShiftingPenalty.hh>
#include <solvers/SimpleLocalSearch.hh>
#include <solvers/TokenRingSolver.hh>
#include <solvers/VNDSolver.hh>
#include <testers/Tester.hh>
#include <testers/MoveTester.hh>
#include <testers/StateTester.hh>
#include <testers/KickerTester.hh>
#include <utils/CLParser.hh>
#include <utils/ValArgument.hh>

int main(int argc, char* argv[])
{
  CLParser cl(argc, argv);
  ValArgument<unsigned> arg_size("-size", true);
  arg_size.setAlias("-s");
  cl.addArgument(arg_size);
  ValArgument<std::string> arg_solmethod("-method", false);
  arg_solmethod.setAlias("-m");
  cl.addArgument(arg_solmethod);
  cl.matchArguments();
  
  // data classes
  unsigned in(arg_size.getValue());
  
  // cost components
  PrimaryDiagonalCostComponent cc1(in);
  SecondaryDiagonalCostComponent cc2(in);
  //PrimaryDiagonalDeltaCostComponent dcc1(in,cc1,true);
  SecondaryDiagonalDeltaCostComponent dcc2(in,cc2,true);
	EmptyDeltaCostComponent<unsigned,std::vector<unsigned>,Swap> dcc1(in,cc1,"Primary diagonal delta",true);
  //EmptyDeltaCostComponent<unsigned,std::vector<unsigned>,Swap> dcc2(in,cc2,"Secondary diagonal delta",true);
  
  
  // helpers
  QueensStateManager qsm(in);
  QueensTabuListManager qtlm;
  QueensNeighborhoodExplorer qnhe(in,qsm);
  QueensOutputManager qom(in,qsm);
  
  // kickers
  QueensKicker qk(in,qnhe);
  
  // runners
  HillClimbing<unsigned,std::vector<unsigned>,Swap> qhc(in,qsm,qnhe);
  SteepestDescent<unsigned,std::vector<unsigned>,Swap> qsd(in,qsm,qnhe);
  TabuSearch<unsigned,std::vector<unsigned>,Swap> qts(in,qsm,qnhe,qtlm);
  SimulatedAnnealing<unsigned,std::vector<unsigned>,Swap> qsa(in,qsm,qnhe);

  TabuSearchWithShiftingPenalty<unsigned,std::vector<unsigned>,Swap> qtsw(in,qsm,qnhe,qtlm);
  
  SimpleLocalSearch<unsigned,ChessBoard,std::vector<unsigned> > qss(in,qsm,qom);
  TokenRingSolver<unsigned,ChessBoard,std::vector<unsigned> > qtr(in,qsm,qom);
  VNDSolver<unsigned,ChessBoard,std::vector<unsigned> > qvnd(in,qsm,qom,3);
  qvnd.SetKicker(qk);
  
  qsm.AddCostComponent(cc1);
  qsm.AddCostComponent(cc2);
  qnhe.AddDeltaCostComponent(dcc1);
  qnhe.AddDeltaCostComponent(dcc2);
	
  if (!arg_solmethod.isSet())
    {
      // testers
      MoveTester<unsigned,ChessBoard,std::vector<unsigned>,Swap> move_test(in,qsm,qom,qnhe, "Swap move");
      KickerTester<unsigned,ChessBoard,std::vector<unsigned> > monokicker_test(in,qsm,qom, qk, "Monomodal kick");
      Tester<unsigned,ChessBoard,std::vector<unsigned> > tester(in,qsm,qom);
		
      tester.AddMoveTester(&move_test);
      tester.AddKickerTester(&monokicker_test);
      tester.AddRunner(&qhc);
      tester.AddRunner(&qsd);
      tester.AddRunner(&qts);
      tester.AddRunner(&qsa);  
      tester.AddRunner(&qtsw);
		
      tester.RunMainMenu();
    }
  else
    {
      if (arg_solmethod.getValue() == "VND") 
	{
	  qvnd.Solve();
	  const ChessBoard& board = qvnd.GetOutput();
	  std::cout << board << std::endl
		    << qvnd.GetCost() << std::endl;
	}
    }
	
  return 0;
}
