#pragma once

#include "easylocal/utils/crow_all.h"
#include <memory>
#include <type_traits>

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
    template <class StateManager, class OutputManager>
    class RESTTester
    {
      typedef typename StateManager::CostStructure::CFtype CFtype;
      typedef typename StateManager::Input Input;
      typedef typename StateManager::State State;
      typedef typename OutputManager::Output Output;
      typedef typename StateManager::CostStructure CostStructure;
      
    public:
      RESTTester()
      {
        static_assert(std::is_same<typename StateManager::Input, typename OutputManager::Input>::value, "StateManager and OutputManager have different Input");
        static_assert(std::is_same<typename StateManager::State, typename OutputManager::State>::value, "StateManager and OutputManager have different State");
      }
      /** Virtual destructor. */
      virtual ~RESTTester() {}
      //  void AddMoveTester(MoveTester<Input, Output, State, CostStructure> &amt);
      //  void AddKickerTester(KickerTester<Input, Output, State, CostStructure> &kt);
      //  void AddRunnerTester(RunnerTester<Input, Output, State, CostStructure> &rt)
      void Run();
    protected:
      //  std::vector<RunnerTester<Input, Output, State, CostStructure>*> runner_testers;                                         /**< The output object. */
      
      crow::SimpleApp app;
    };
    
    template <class StateManager, class OutputManager>
    void RESTTester<StateManager, OutputManager>::Run()
    {
      CROW_ROUTE(app, "/")([](){
        return "Hello world";
      });
      
      app.port(8080).multithreaded().run();
    }
    
  }
}
