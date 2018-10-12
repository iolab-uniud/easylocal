#pragma once

#include "../../../crow/crow.h"

/** A REST Tester represents the web service interface of a easylocal solver. Differently from the regular tester, this class is State-less (w.r.t. easylocal state)
 @ingroup Testers
 */
template <template<class Input, class State, class CostStructure> class StateManager, template <class Input, class Output, class State, class CostStructure>>
class RESTTester
{
//  typedef typename CostStructure::CFtype CFtype;
  
public:
  RESTTester() {}
  /** Virtual destructor. */
  virtual ~RESTTester() {}
//  void AddMoveTester(MoveTester<Input, Output, State, CostStructure> &amt);
//  void AddKickerTester(KickerTester<Input, Output, State, CostStructure> &kt);
//  void AddRunnerTester(RunnerTester<Input, Output, State, CostStructure> &rt)
  void Run() {}
protected:
//  std::vector<RunnerTester<Input, Output, State, CostStructure>*> runner_testers;
  Output out;                                          /**< The output object. */
  
//  crow::SimpleApp app;
};
