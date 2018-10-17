#pragma once

#include "easylocal/utils/crow_all.h"
#include <memory>
#include <map>
#include "easylocal/testers/tester.hh"
#include "easylocal/utils/json.hpp"
#include "easylocal/utils/url.hh"

// TODO: use a task manager (or define one) for ensuring that just the right number of workers are used and the system is not overloaded
// TODO: add a "garbage collector" task that will destroy the unneeded resources

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
        for (auto r : this->runners)
        {
          runner_urls.push_back("/runner/" + r->name);
          runner_map[r->name] = r;
        }
      }
      /** Virtual destructor. */
      virtual ~RESTTester() {}
      void Run();
    protected:
      StateManager<Input, State, CostStructure>& sm;
      OutputManager<Input, Output, State>& om;
      crow::SimpleApp app;
      
      // utils
      std::vector<string> runner_urls;
      std::map<string, Runner<Input, State, CostStructure>*> runner_map;
      
      // endpoints management
      void RootEndpoint(const crow::request& req, crow::response& res) const;
      
      size_t LaunchRunner(float timeout, std::unique_ptr<Input> p_in, std::unique_ptr<Runner<Input, State, CostStructure>> p_r, json parameters);
      json RunnerStatus(size_t run_id) const;
      
      mutable std::mutex running_mutex;
      std::map<size_t, std::tuple<std::unique_ptr<Input>, std::unique_ptr<State>, std::shared_future<CostStructure>, std::unique_ptr<Runner<Input, State, CostStructure>>>> running;
    };
    
    template <class Input, class Output, class State, class CostStructure>
    void RESTTester<Input, Output, State, CostStructure>::Run()
    {
      
      // This endpoint just lists the available services
      CROW_ROUTE(app, "/")([this](const crow::request& req, crow::response& res) { this->RootEndpoint(req, res); });
      
      // This endpoint allows to interact with a specific runner
      CROW_ROUTE(app, "/runner/<string>")
      .methods("GET"_method)([this](std::string name) {
        json response;
        auto it = this->runner_map.find(name);
        if (it == this->runner_map.end())
          return JSONResponse::make_error(404, "Runner `" + name + "` does not exist or is not active");
        response["parameters"] = it->second->ParametersDescriptionToJSON();
        // TODO: also add to the response the list of the running instances of this runner
        return JSONResponse::make_response(200, response);
      });
      
      CROW_ROUTE(app, "/runner/<string>")
      .methods("POST"_method)([this](const crow::request& req, crow::response& res, std::string name) {
        json response;
        auto it = this->runner_map.find(name);
        if (it == this->runner_map.end())
        {
          res = JSONResponse::make_error(404, "Runner `" + name + "` does not exist or is not active");
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
            res = JSONResponse::make_error(415, "Wrong Content-Type, only application/json is possible");
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
            res = JSONResponse::make_error(422, "The input file does not comply with the format expected by the system", e.what());
            res.end();
            return;
          }
          float timeout = 0.0;
          if (!parameters.empty() && parameters.find("timeout") != parameters.end())
            timeout = parameters["timeout"];
          size_t run_id = this->LaunchRunner(timeout, std::move(in), it->second->Clone(), parameters);
          response["run_id"] = std::to_string(run_id);
          response["url"] = "/running/" + std::to_string(run_id);
          response["started"] = true;
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
      CROW_ROUTE(app, "/running/<string>")
      .methods("GET"_method)([this](std::string _run_id) {
        size_t run_id = std::stoul(_run_id);
        json response = this->RunnerStatus(run_id);
        if (response.find("error") == response.end())
          return JSONResponse::make_response(200, response);
        else
          return JSONResponse::make_error(404, response);
      });
      
      app.port(8080).multithreaded().run();
    }
    
    template <class Input, class Output, class State, class CostStructure>
    void RESTTester<Input, Output, State, CostStructure>::RootEndpoint(const crow::request& req, crow::response& res) const
    {
      json response;
      response["runners"] = runner_urls;
      response["tasks"] = {};
      std::lock_guard<std::mutex> lock(running_mutex);
      for (const auto& run_it : running)
      {
        const auto& t = run_it.second;
        bool finished = std::get<2>(t).wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        response["tasks"].push_back({ { "runner", std::get<3>(t)->name }, { "run_id", run_it.first }, { "finished", finished }, { "url", "/running/" + std::to_string(run_it.first) } });
      }
      res = JSONResponse::make_response(200, response);
      res.end();
    }
    
    template <class Input, class Output, class State, class CostStructure>
    size_t RESTTester<Input, Output, State, CostStructure>::LaunchRunner(float timeout, std::unique_ptr<Input> p_in, std::unique_ptr<Runner<Input, State, CostStructure>> p_r, json parameters)
    {
      static unsigned long counter = 0;
      // the lock is here, because also counter has to be guarded
      std::lock_guard<std::mutex> lock(running_mutex);
      size_t run_id = std::hash<std::string>()(p_r->name + std::to_string(counter));
      counter++;
      auto _timeout = std::chrono::milliseconds((long long)(timeout * 1000));
      std::unique_ptr<State> p_st = std::make_unique<State>(*p_in);
      if (parameters.find("initial_state_strategy") != parameters.end() && parameters["initial_state_strategy"] == "greedy")
        sm.GreedyState(*p_in, *p_st);
      else
        sm.RandomState(*p_in, *p_st);
      // forward runner parameters to the runner itself
      if (!parameters.empty())
        p_r->ParametersFromJSON(parameters);
      auto cost = p_r->AsyncRun(_timeout, *p_in, *p_st);
      running[run_id] = std::make_tuple(std::move(p_in), std::move(p_st), std::move(cost), std::move(p_r));
      return run_id;
    }
    
    template <class Input, class Output, class State, class CostStructure>
    json RESTTester<Input, Output, State, CostStructure>::RunnerStatus(size_t run_id) const
    {
      std::lock_guard<std::mutex> lock(running_mutex);
      json status;
      status["run_id"] = run_id;
      auto it = running.find(run_id);
      if (it == running.end()) // run_id does not exist
      {
        status["error"] = "The run `" + std::to_string(run_id) + "` does not exist (or it has been removed because too old)";
        return status;
      }
      const auto& t = it->second;
      status["runner"] = std::get<3>(t)->name;
      if (std::get<2>(t).wait_for(std::chrono::seconds(0)) == std::future_status::ready)
      {
        status["finished"] = true;
        status["cost"] = sm.JSONCostFunctionComponents(*std::get<0>(t), *std::get<1>(t).get());
      }
      else
      {
        status["finished"] = false;
        auto st = std::get<3>(t)->GetCurrentBestState();
        status["cost"] = sm.JSONCostFunctionComponents(*std::get<0>(t), *st);
      }
      return status;
    }
  }
}
