#pragma once

#include "../../../crow/crow_all.h"
#include <memory>

namespace EasyLocal {
  namespace Debug {
    
    /** A REST Tester represents the web service interface of a easylocal solver. Differently from the regular tester, this class is State-less (w.r.t. easylocal state)
     @ingroup Testers
     */
    template <class Input, class Output, class State, class CostStructure, class StateManager, class OutputManager>
    class RESTTester
    {
      typedef typename CostStructure::CFtype CFtype;
      
    public:
      RESTTester() {}
      /** Virtual destructor. */
      virtual ~RESTTester() {}
      //  void AddMoveTester(MoveTester<Input, Output, State, CostStructure> &amt);
      //  void AddKickerTester(KickerTester<Input, Output, State, CostStructure> &kt);
      //  void AddRunnerTester(RunnerTester<Input, Output, State, CostStructure> &rt)
      void Run();
    protected:
      //  std::vector<RunnerTester<Input, Output, State, CostStructure>*> runner_testers;                                         /**< The output object. */
      
      //  crow::SimpleApp app;
    };
    
    /** Factory method to create a rest_tester providing a state and an output manager.
        Note: To be “strictly” used with  auto type (see the very involved signature) */
    template <class StateManager, class OutputManager>
    std::unique_ptr<RESTTester<typename StateManager::Input, typename OutputManager::Output, typename StateManager::State, typename StateManager::CostStructure, StateManager, OutputManager>> create_rest_tester()
    {
      return std::unique_ptr<RESTTester<typename StateManager::Input, typename OutputManager::Output, typename StateManager::State, typename StateManager::CostStructure, StateManager, OutputManager>>(new RESTTester<typename StateManager::Input, typename OutputManager::Output, typename StateManager::State, typename StateManager::CostStructure, StateManager, OutputManager>());
    }
    
    template <class Input, class Output, class State, class CostStructure, class StateManager, class OutputManager>
    void RESTTester<Input, Output, State, CostStructure, StateManager, OutputManager>::Run()
    {
      crow::SimpleApp app;
      
      CROW_ROUTE(app, "/")([](){
        return "Hello world";
      });
      
      app.port(8080).multithreaded().run();
    }
    
  }
}
