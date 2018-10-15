#pragma once

#include "easylocal/utils/crow_all.h"
#include <memory>
#include <map>
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
      {
        this->AddRunners();
      }
      /** Virtual destructor. */
      virtual ~RESTTester() {}
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
      std::vector<string> runner_urls;
      std::map<string, Runner<Input, State, CostStructure>*> runners;

      for (auto r : this->runners)
      {
        runner_urls.push_back("/runner/" + r->name);
        runners[r->name] = r;
      }
      
      CROW_ROUTE(app, "/")([runner_urls](){
        crow::json::wvalue services;
        services["runners"] = runner_urls;
        return services;
      });
      
      CROW_ROUTE(app, "/runner/<string>")([runners](string name){
        return "Hello from " + name;
      });
      
      app.port(8080).multithreaded().run();
    }
    
  }
}
