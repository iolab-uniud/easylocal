#pragma once

#include "easylocal/utils/crow_all.h"
#include <memory>
#include <map>
#include "easylocal/testers/tester.hh"
#include "easylocal/utils/json.hpp"
#include "easylocal/utils/url.hh"

namespace EasyLocal {
  namespace Debug {
    
    using json = nlohmann::json;
    
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
      
      auto json_response = [](int code, const json& body) {
        crow::response res = crow::response(code, body.dump());
        res.set_header("Content-Type", "application/json");
        return res;
      };

      for (auto r : this->runners)
      {
        runner_urls.push_back("/runner/" + r->name);
        runners[r->name] = r;
      }
      
      // This endpoint just lists the available services
      CROW_ROUTE(app, "/")([json_response, runner_urls]() {
        json response;
        response["runners"] = runner_urls;
        return json_response(200, response);
      });
      
      // This endpoint allows to interact with a specific runner
      CROW_ROUTE(app, "/runner/<string>")
      .methods("GET"_method)([json_response, runners](std::string name) {
        json response;
        auto it = runners.find(name);
        if (it == runners.end())
        {
          response["reason"] = "Runner `" + name + "` does not exist or is not active.";
          return json_response(404, response);
        }
        response["parameters"] = it->second->ParametersDescriptionToJSON();
        return json_response(200, response);
      });
      
      CROW_ROUTE(app, "/runner/<string>")
      .methods("POST"_method)([json_response, runners](const crow::request& req, crow::response& res, std::string name) {
        json response;
        if (runners.find(name) == runners.end())
        {
          response["reason"] = "Runner `" + name + "` does not exist or is not active.";
          res = json_response(404, response);
          res.end();
          return;
        }
        json parameters = json::parse(URLDecode(req.url_params.get("parameters")));
        json payload = json::parse(req.body);
        response["you-sent"] = parameters;
        res = json_response(200, response);
        res.end();
      });
      
      // This endpoint allow to check the status of a specific runner request
      CROW_ROUTE(app, "/runner/<string>/<int>")
      .methods("GET"_method)([json_response, runners](std::string name, int id) {
        json response;
        if (runners.find(name) == runners.end())
        {
          response["reason"] = "Runner `" + name + "` does not exist or is not active.";
          return json_response(404, response);
        }
        response["parameters"] = { id };
        return json_response(200, response);
      });
      
      app.port(8080).multithreaded().run();
    }
    
  }
}
