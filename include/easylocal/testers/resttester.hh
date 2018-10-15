#pragma once

#include "easylocal/utils/crow_all.h"
#include <memory>
#include "easylocal/testers/tester.hh"

namespace EasyLocal {
  namespace Debug {
    
    /** A REST Runner Tester handles a single runner
     @ingroup Testers
     */
    template <class Runner>
    class RESTRunnerTester
    {
      typedef typename Runner::CostStructure::CFtype CFtype;
      typedef typename Runner::Input Input;
      typedef typename Runner::State State;
      typedef typename Runner::Move Move;
    public:
      RESTRunnerTester() {}
    };
    
    /** A REST Tester represents the web service interface of a easylocal solver. Differently from the regular tester, this class is State-less (w.r.t. easylocal state)
     @ingroup Testers
     */
    template <class Input, class Output, class State, class CostStructure = DefaultCostStructure<int>>
    class RESTTester : public AbstractTester<Input, State, CostStructure>
    {
    public:
      RESTTester(StateManager<Input, State, CostStructure>& sm, OutputManager<Input, Output, State>& om)
      : sm(sm), om(om)
      {}
      /** Virtual destructor. */
      virtual ~RESTTester() {}
      //  void AddMoveTester(MoveTester<Input, Output, State, CostStructure> &amt);
      //  void AddKickerTester(KickerTester<Input, Output, State, CostStructure> &kt);
      //  void AddRunnerTester(RunnerTester<Input, Output, State, CostStructure> &rt)
      void Run();
      
      
    protected:
      //  std::vector<RunnerTester<Input, Output, State, CostStructure>*> runner_testers;                                         /**< The output object. */
      StateManager<Input, State, CostStructure>& sm;
      OutputManager<Input, Output, State>& om;
      crow::SimpleApp app;
    };
    
    template <class Input, class Output, class State, class CostStructure>
    void RESTTester<Input, Output, State, CostStructure>::Run()
    {
      CROW_ROUTE(app, "/")([](){
        return "Hello world";
      });
      
      app.port(8080).multithreaded().run();
    }
    
  }
}
