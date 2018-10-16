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
    
    struct JSONResponse
    {
      static inline crow::response make_response(int code, json body)
      {
        crow::response res(code, body.dump());
        res.set_header("Content-Type", "application/json");
        return res;
      }
      
      static inline crow::response make_error(int code, std::string message, std::string additional_info="")
      {
        json j;
        j["status"] = "error";
        j["reason"] = message;
        if (additional_info != "")
          j["additional_info"] = additional_info;
        return JSONResponse::make_response(code, j);
      }
    };
    
    /** A REST Tester represents the web service interface of a easylocal solver. Differently from the regular tester, this class is State-less (w.r.t. easylocal state)
     @ingroup Testers
     */
    template <class Input, class Output, class State, class CostStructure = DefaultCostStructure<int>>
    class RESTTester : AbstractTester<Input, State, CostStructure>
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
      StateManager<Input, State, CostStructure>& sm;
      OutputManager<Input, Output, State>& om;
      crow::SimpleApp app;
      
      unsigned long GoRunner(std::unique_ptr<Input> p_in, std::unique_ptr<Runner<Input, State, CostStructure>> p_r);
      std::list<std::tuple<std::unique_ptr<Input>, std::unique_ptr<State>, std::shared_future<CostStructure>, std::unique_ptr<Runner<Input, State, CostStructure>>>> running;
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
      
      // This endpoint just lists the available services
      CROW_ROUTE(app, "/")([runner_urls]() {
        json response;
        response["runners"] = runner_urls;
        return JSONResponse::make_response(200, response);
      });
      
      // This endpoint allows to interact with a specific runner
      CROW_ROUTE(app, "/runner/<string>")
      .methods("GET"_method)([runners](std::string name) {
        json response;
        auto it = runners.find(name);
        if (it == runners.end())
          return JSONResponse::make_error(404, "Runner `" + name + "` does not exist or is not active.");
        response["parameters"] = it->second->ParametersDescriptionToJSON();
        // TODO: also add to the response the list of the running instances of this runner
        return JSONResponse::make_response(200, response);
      });
      
      CROW_ROUTE(app, "/runner/<string>")
      .methods("POST"_method)([this, runners](const crow::request& req, crow::response& res, std::string name) {
        json response;
        auto it = runners.find(name);
        if (it == runners.end())
        {
          res = JSONResponse::make_error(404, "Runner `" + name + "` does not exist or is not active.");
          res.end();
          return;
        }
        try
        {
          json parameters;
          if (req.url_params.get("parameters") != nullptr)
            parameters = json::parse(URLDecode(req.url_params.get("parameters")));
          auto ct = req.headers.find("Content-Type");
          if (ct == req.headers.end() || ct->second != "application/json")
          {
            // TODO: handle content negotiation
            res = JSONResponse::make_error(415, "Wrong Content-Type, only application/json is possible.");
            res.end();
            return;
          }
          json payload = json::parse(req.body);
          std::unique_ptr<Input> in;
          try
          {
            in = std::make_unique<Input>(payload);
          }
          catch (std::exception& e)
          {
            res = JSONResponse::make_error(422, "The input file does not comply with the format expected by the system.", e.what());
            res.end();
            return;
          }
          response["run_id"] = this->GoRunner(std::move(in), it->second->Clone());
          response["running"] = true;
          res = JSONResponse::make_response(200, response);
          res.end();
          return;
        }
        catch (std::exception& e)
        {
          res = JSONResponse::make_error(405, e.what());
          res.end();
          return;
        }
      });
      
      // This endpoint allow to check the status of a specific runner request
      CROW_ROUTE(app, "/runner/<string>/<int>")
      .methods("GET"_method)([runners](std::string name, int id) {
        json response;
        if (runners.find(name) == runners.end())
        {
          return JSONResponse::make_error(404, "Runner `" + name + "` does not exist or is not active.");
        }
        response["parameters"] = { id };
        return JSONResponse::make_response(200, response);
      });
      
      app.port(8080).multithreaded().run();
    }
    
    template <class Input, class Output, class State, class CostStructure>
    unsigned long RESTTester<Input, Output, State, CostStructure>::GoRunner(std::unique_ptr<Input> p_in, std::unique_ptr<Runner<Input, State, CostStructure>> p_r)
    {
      auto timeout = std::chrono::milliseconds((long long)(10 * 1000));
      std::unique_ptr<State> p_st = std::make_unique<State>(*p_in);
      sm.RandomState(*p_in, *p_st);
      running.emplace_back(std::make_tuple(std::move(p_in), std::move(p_st), std::move(p_r->AsyncRun(timeout, *p_in, *p_st)), std::move(p_r)));
      return running.size();
    }
  }
}
