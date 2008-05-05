/** @file main.cc
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
#include "helpers/SwapNeighborhoodExplorer.hh"
#include "helpers/QueensOutputManager.hh"
#include "helpers/QueensStateManager.hh"
#include "helpers/QueensTabuListManager.hh"
#include "helpers/PrimaryDiagonalCostComponent.hh"
#include "helpers/PrimaryDiagonalDeltaCostComponent.hh"
#include "helpers/SecondaryDiagonalCostComponent.hh"
#include "helpers/SecondaryDiagonalDeltaCostComponent.hh"
#include <helpers/DeltaCostComponent.hh>
#include "kickers/QueensKicker.hh"

#include <helpers/RunnerObserver.hh>
#include <helpers/GeneralizedLocalSearchObserver.hh>

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
#include <utils/clparser/CLParser.hh>
#include <utils/clparser/ValArgument.hh>
#include <utils/Chronometer.hh>

int main(int argc, char* argv[])
{
  CLParser cl(argc, argv);
  ValArgument<unsigned> arg_size("size", "s", true, cl);
  ValArgument<std::string> arg_solmethod("method", "m", false, cl);
  ValArgument<unsigned> arg_plot_level("plot", "p", false, 0u, cl);
  ValArgument<unsigned> arg_verbosity_level("verbose", "v", false, 0u, cl);
	ValArgument<float> arg_timeout("timeout", "to", false, 0.0f, cl);
  cl.MatchArgument(arg_size);
  
  // data classes
  unsigned in(arg_size.GetValue());
  
  // cost components
  PrimaryDiagonalCostComponent cc1(in);
  SecondaryDiagonalCostComponent cc2(in);
  //PrimaryDiagonalDeltaCostComponent dcc1(in, cc1);
  //SecondaryDiagonalDeltaCostComponent dcc2(in, cc2);
  EmptyDeltaCostComponent<unsigned, std::vector<unsigned>, Swap> dcc1(in, cc1, "Primary diagonal delta");
  EmptyDeltaCostComponent<unsigned, std::vector<unsigned>, Swap> dcc2(in, cc2, "Secondary diagonal delta");
  
  // helpers
  QueensStateManager qsm(in);
  QueensTabuListManager qtlm;
  SwapNeighborhoodExplorer qnhe(in, qsm);
  QueensOutputManager qom(in);
  
  // kickers
  QueensKicker qk(in, qnhe);
  
  // runners
  HillClimbing<unsigned, std::vector<unsigned>, Swap> qhc(in, qsm, qnhe, "SwapHillClimbing", cl);
  SteepestDescent<unsigned, std::vector<unsigned>, Swap> qsd(in, qsm, qnhe, "SwapSteepestDescent");
  TabuSearch<unsigned, std::vector<unsigned>, Swap> qts(in, qsm, qnhe, qtlm, "SwapTabuSearch", cl);
  SimulatedAnnealing<unsigned, std::vector<unsigned>, Swap> qsa(in, qsm, qnhe, "SwapSimulatedAnnealing", cl);
  TabuSearchWithShiftingPenalty<unsigned, std::vector<unsigned>, Swap> qtsw(in, qsm, qnhe, qtlm);
  
  SimpleLocalSearch<unsigned, ChessBoard, std::vector<unsigned> > qss(in, qsm, qom, "QueensSLS", cl);
  GeneralizedLocalSearchSolver<unsigned, ChessBoard, std::vector<unsigned> > qgls(in, qsm, qom, "QueensGLS", cl);
  VNDSolver<unsigned, ChessBoard, std::vector<unsigned> > qvnd(in, qsm, qom, 3);

	cl.MatchArguments();
	
  RunnerObserver<unsigned, std::vector<unsigned>, Swap> ro(arg_verbosity_level.GetValue(), arg_plot_level.GetValue());
  GeneralizedLocalSearchObserver<unsigned, ChessBoard, std::vector<unsigned> > so(arg_plot_level.GetValue());

	
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
		MoveTester<unsigned, ChessBoard, std::vector<unsigned>, Swap> move_test(in,qsm,qom,qnhe, "Swap move");
		KickerTester<unsigned, ChessBoard, std::vector<unsigned> > monokicker_test(in,qsm,qom, qk, "Monomodal kick");
		Tester<unsigned, ChessBoard, std::vector<unsigned> > tester(in,qsm,qom);
		
		tester.AddMoveTester(move_test);
		tester.AddKickerTester(monokicker_test);
		tester.AddRunner(qhc);
		tester.AddRunner(qsd);
		tester.AddRunner(qts);
		tester.AddRunner(qsa);  
		tester.AddRunner(qtsw);
		
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
