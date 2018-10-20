#pragma once

#include "easylocal/utils/crow_all.h"
#include <memory>
#include <map>
#include <queue>
#include "easylocal/testers/tester.hh"
#include "easylocal/utils/json.hpp"
#include "easylocal/utils/url.hh"
#include "easylocal/utils/parameter.hh"


// TODO: use a task manager (or define one) for ensuring that just the right number of workers are used and the system is not overloaded
// TODO: add a "garbage collector" task that will destroy the unneeded resources

namespace EasyLocal {
  namespace Debug {
    
    using json = nlohmann::json;
    
    /** A REST Tester represents the web service interface of a easylocal solver. Differently from the regular tester, this class is State-less (w.r.t. easylocal state)
     @ingroup Testers
     */
    template <class Input, class Output, class State, class CostStructure = DefaultCostStructure<int>>
    class RESTTester : public AbstractTester<Input, State, CostStructure>, public Parametrized
    {
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
      
      template <typename T>
      class TaskQueue
      {
      public:
        ~TaskQueue()
        {
          Invalidate();
        }
        
        bool TryDequeue(T& out)
        {
          std::lock_guard<std::mutex> lock(qmutex);
          if(queue.empty() || !valid)
            return false;
          out = std::move(queue.front());
          queue.pop();
          return true;
        }
        
        bool WaitDequeue(T& out)
        {
          std::unique_lock<std::mutex> lock(qmutex);
          changed.wait(lock, [this]() {
            return !queue.empty() || !valid;
          });
          /*
           * Using the condition in the predicate ensures that spurious wakeups with a valid
           * but empty queue will not proceed, so only need to check for validity before proceeding.
           */
          if(!valid)
            return false;
          out = std::move(queue.front());
          queue.pop();
          return true;
        }
        
        void Enqueue(T value)
        {
          std::lock_guard<std::mutex> lock(qmutex);
          queue.push(std::move(value));
          changed.notify_one();
        }
        
        bool Empty() const
        {
          std::lock_guard<std::mutex> lock(qmutex);
          return queue.empty();
        }
        
        void Clear(void)
        {
          std::lock_guard<std::mutex> lock(qmutex);
          while(!queue.empty())
            queue.pop();
          changed.notify_all();
        }
        
        bool IsValid(void) const
        {
          std::lock_guard<std::mutex> lock(qmutex);
          return valid;
        }
        
        void Invalidate(void)
        {
          std::lock_guard<std::mutex> lock(qmutex);
          valid = false;
          changed.notify_all();
        }
        
      private:
        std::atomic<bool> valid{true};
        mutable std::mutex qmutex;
        std::queue<T> queue;
        std::condition_variable changed;
      };
      
    public:
      RESTTester(StateManager<Input, State, CostStructure>& sm, OutputManager<Input, Output, State>& om);
      /** Virtual destructor. */
      virtual ~RESTTester() { Destroy(); }
      void Run();
    private:
      void Worker();
      void Destroy();
      void InitializeParameters();
      
      const unsigned int numThreads;
      
      StateManager<Input, State, CostStructure>& sm;
      OutputManager<Input, Output, State>& om;
      crow::SimpleApp app;
      
      // utils
      std::vector<string> runner_urls;
      std::map<string, Runner<Input, State, CostStructure>*> runner_map;
      
      // endpoints management
      void RootEndpoint(const crow::request& req, crow::response& res) const;
      
      std::string CreateTask(float timeout, std::unique_ptr<Input> p_in, std::unique_ptr<Runner<Input, State, CostStructure>> p_r, json parameters, std::unique_ptr<State> p_st);
      json TaskStatus(std::string run_id) const;
      json Solution(std::string run_id) const;
      
      typedef std::tuple<std::string, std::unique_ptr<Input>, std::unique_ptr<State>, std::unique_ptr<Runner<Input, State, CostStructure>>, std::chrono::milliseconds> Task;

      TaskQueue<std::shared_ptr<Task>> task_queue;
      mutable std::mutex task_status_mutex;
      std::map<std::string, std::tuple<bool, std::string, std::shared_ptr<Task>>> task_status;
      std::atomic<bool> done;
      std::vector<std::thread> workers;
      Parameter<unsigned int> port;
    };
    
    template <class Input, class Output, class State, class CostStructure>
    RESTTester<Input, Output, State, CostStructure>::RESTTester(StateManager<Input, State, CostStructure>& sm, OutputManager<Input, Output, State>& om)
    : Parametrized("REST", "REST tester"), numThreads(2), sm(sm), om(om)
    {
      for (auto r : this->runners)
      {
        runner_urls.push_back("/runner/" + r->name);
        runner_map[r->name] = r;
      }
      done = false;
      try
      {
        for (unsigned i = 0; i < numThreads; i++)
          workers.emplace_back(&RESTTester<Input, Output, State, CostStructure>::Worker, this);
      }
      catch(...)
      {
        Destroy();
        throw;
      }
    }
    
    template <class Input, class Output, class State, class CostStructure>
    void RESTTester<Input, Output, State, CostStructure>::Destroy()
    {
      done = true;
      task_queue.Invalidate();
      for (auto& worker : workers)
      {
        if (worker.joinable())
          worker.join();
      }
    }
    
    template <class Input, class Output, class State, class CostStructure>
    void RESTTester<Input, Output, State, CostStructure>::InitializeParameters()
    {
      port("port", "TCP/IP port", this->parameters);
      port = 18080;
    }
    
    template <class Input, class Output, class State, class CostStructure>
    void RESTTester<Input, Output, State, CostStructure>::Worker()
    {
      while (!done)
      {
        std::shared_ptr<Task> task;
        if (task_queue.WaitDequeue(task))
        {
          // dispatch the task
          auto run_id = std::get<0>(*task);
          auto& p_in = std::get<1>(*task);
          auto& p_st = std::get<2>(*task);
          auto& p_r = std::get<3>(*task);
          auto& timeout = std::get<4>(*task);
          // run it synchronously (w already are in a different thread)
          auto cost = p_r->SyncRun(timeout, *p_in, *p_st);
          std::lock_guard<std::mutex> lock(task_status_mutex);
          std::get<0>(task_status[run_id]) = true;
        }
      }
    }
        
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
          // conventionally an initial solution, if available, is passed into a "initial_solution" field in the payload
          std::unique_ptr<State> p_st;
          if (payload.find("initial_solution") != payload.end() && !payload["initial_solution"].is_null())
          {
            try
            {
              Output out(*in);
              p_st = std::make_unique<State>(*in);
              std::istringstream is(payload["initial_solution"].dump());
              is >> out;
              om.InputState(*in, *p_st, out);
            }
            catch (std::exception & e)
            {
              res = JSONResponse::make_error(422, "The initial solution does not comply with the format expected by the system", e.what());
              res.end();
              return;
            }
          }
          float timeout = req.url_params.get("timeout") ? std::atof(req.url_params.get("timeout")) : 0.0;
          std::string run_id = this->CreateTask(timeout, std::move(in), it->second->Clone(), parameters, std::move(p_st));
          response["run_id"] = run_id;
          response["url"] = "/running/" + run_id;
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
      
      // This endpoint allow to check the status of a specific runner request and abort it
      CROW_ROUTE(app, "/running/<string>")
      .methods("GET"_method)([this](std::string run_id) {
        json response = this->TaskStatus(run_id);
        if (response.find("error") == response.end())
          return JSONResponse::make_response(200, response);
        else
          return JSONResponse::make_error(404, response["error"]);
      });
      // TODO: add authorization (e.g. JWT)
      CROW_ROUTE(app, "/running/<string>")
      .methods("DELETE"_method)([](const crow::request& req, crow::response& res, std::string _run_id) {
        json response;
        response["message"] = "Currently not implemented, will be ready soon";
        res = JSONResponse::make_response(200, response);
        res.end();
      });
      
      // This endpoint allow to access a solution, when available
      CROW_ROUTE(app, "/solution/<string>")
      .methods("GET"_method)([this](const crow::request& req, crow::response& res, std::string run_id) {
        json response = this->Solution(run_id);
        if (response.find("error") == response.end())
          res = JSONResponse::make_response(200, response);
        else
          res = JSONResponse::make_error(404, response["error"]);
        res.end();
      });
      
      app.port(port).multithreaded().run();
    }
    
    template <class Input, class Output, class State, class CostStructure>
    void RESTTester<Input, Output, State, CostStructure>::RootEndpoint(const crow::request& req, crow::response& res) const
    {
      json response;
      response["runners"] = runner_urls;
      response["tasks"] = {};
      std::lock_guard<std::mutex> lock(task_status_mutex);
      for (const auto& task_it : task_status)
      {
        const auto& t = task_it.second;
        response["tasks"].push_back({ { "runner", std::get<1>(t) }, { "run_id", task_it.first }, { "finished", std::get<0>(t) }, { "url", "/running/" + task_it.first } });
      }
      res = JSONResponse::make_response(200, response);
      res.end();
    }
    
    template <class Input, class Output, class State, class CostStructure>
    std::string RESTTester<Input, Output, State, CostStructure>::CreateTask(float timeout, std::unique_ptr<Input> p_in, std::unique_ptr<Runner<Input, State, CostStructure>> p_r, json parameters, std::unique_ptr<State> p_st)
    {
      static unsigned long counter = 0;
      // the lock is here, because also counter has to be guarded
      std::lock_guard<std::mutex> lock(task_status_mutex);
      std::string run_id = std::to_string(std::hash<std::string>()(p_r->name + std::to_string(counter)));
      counter++;
      auto _timeout = std::chrono::milliseconds((long long)(timeout * 1000));
      if (!p_st)
      {
        p_st = std::make_unique<State>(*p_in);
        if (parameters.find("initial_state_strategy") != parameters.end() && parameters["initial_state_strategy"] == "greedy")
          sm.GreedyState(*p_in, *p_st);
        else
          sm.RandomState(*p_in, *p_st);
      }
      // forward runner parameters to the runner itself
      if (!parameters.empty())
        p_r->ParametersFromJSON(parameters);
//      auto cost = p_r->AsyncRun(_timeout, *p_in, *p_st);
      std::string name = p_r->name;
      std::shared_ptr<Task> task = std::make_shared<Task>(std::make_tuple(run_id, std::move(p_in), std::move(p_st), std::move(p_r), _timeout));
      task_queue.Enqueue(task);
      task_status[run_id] = std::make_tuple(false, name, task);
      return run_id;
    }
    
    template <class Input, class Output, class State, class CostStructure>
    json RESTTester<Input, Output, State, CostStructure>::TaskStatus(std::string run_id) const
    {
      std::lock_guard<std::mutex> lock(task_status_mutex);
      json status;
      status["run_id"] = run_id;
      auto it = task_status.find(run_id);
      if (it == task_status.end()) // run_id does not exist
      {
        status["error"] = "The task `" + run_id + "` does not exist (or it has been removed because too old)";
        return status;
      }
      const auto& t = it->second;
      status["runner"] = std::get<1>(t);
      const auto& task = std::get<2>(t);
      if (std::get<0>(t))
      {
        status["finished"] = true;
        status["cost"] = sm.JSONCostFunctionComponents(*std::get<1>(*task), *std::get<2>(*task));
        status["solution_url"] = "/solution/" + run_id;
      }
      else
      {
        status["finished"] = false;
        auto st = std::get<3>(*task)->GetCurrentBestState();
        status["cost"] = sm.JSONCostFunctionComponents(*std::get<1>(*task), *st);
      }
      return status;
    }
    
    template <class Input, class Output, class State, class CostStructure>
    json RESTTester<Input, Output, State, CostStructure>::Solution(std::string run_id) const
    {
      std::lock_guard<std::mutex> lock(task_status_mutex);
      json status;
      status["run_id"] = run_id;
      auto it = task_status.find(run_id);
      if (it == task_status.end()) // run_id does not exist
      {
        status["error"] = "The task `" + run_id + "` does not exist (or it has been removed because too old)";
        return status;
      }
      const auto& t = it->second;
      status["runner"] = std::get<1>(t);
      const auto& task = std::get<2>(t);
      if (std::get<0>(t))
      {
        Output out(*std::get<1>(*task));
        status["finished"] = true;
        om.OutputState(*std::get<1>(*task), *std::get<2>(*task), out);
        out.ConvertToJson(status["solution"]);
      }
      else
      {
        status["error"] = "The task `" + run_id + "` has not finished yet";
      }
      return status;
    }
  }
}
