#pragma once

#include "easylocal/utils/crow_all.h"
#include "easylocal/testers/tester.hh"
#include "easylocal/utils/json.hpp"
#include "easylocal/utils/url.hh"
#include "easylocal/utils/parameter.hh"
#include <memory>
#include <map>
#include <deque>
#include <ctime>
#include <regex>
// TODO: use it only if CURL is available
#include "easylocal/utils/uccurl.hh"


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
      
      static std::string getISOTimestamp(const std::chrono::system_clock::time_point& t)
      {
        auto itt = std::chrono::system_clock::to_time_t(t);
        std::ostringstream ss;
        ss << std::put_time(gmtime(&itt), "%FT%TZ");
        return ss.str();
      }
      
      class Task
      {
      public:
        Task(std::string task_id,
             std::shared_ptr<Input> p_in,
             std::shared_ptr<State> p_st,
             std::shared_ptr<Runner<Input, State, CostStructure>> p_r,
             std::chrono::milliseconds timeout,
             std::string callback_url = "")
        : task_id(task_id), p_in(p_in), p_st(p_st), p_r(p_r), timeout(timeout), submitted(std::chrono::system_clock::now()), finished(false), running(false), callback_url(callback_url)
        {
          // TODO: check url
          if (callback_url != "")
          {
            if (!std::regex_match(callback_url, std::regex(R"(^(([^:\/?#]+):)?(//([^\/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?)", std::regex::extended)))
              throw std::logic_error("Callback url " + callback_url + " is not a valid URL");
            this->callback_url = callback_url;
          }
        }
        std::string task_id;
        std::shared_ptr<Input> p_in;
        std::shared_ptr<State> p_st;
        std::shared_ptr<Runner<Input, State, CostStructure>> p_r;
        std::chrono::milliseconds timeout;
        std::chrono::system_clock::time_point submitted;
        bool finished;
        bool running;
        std::chrono::system_clock::time_point started;
        std::chrono::system_clock::time_point completed;
        std::string callback_url;
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
          queue.pop_front();
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
          queue.pop_front();
          return true;
        }
        
        void Enqueue(T value)
        {
          std::lock_guard<std::mutex> lock(qmutex);
          queue.push_back(std::move(value));
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
          std::deque<Task>().swap(queue);
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
        
        void Remove(std::function<bool(const T& t)> pred)
        {
          std::lock_guard<std::mutex> lock(qmutex);
          std::remove_if(queue.begin(), queue.end(), pred);
        }
        
      private:
        std::atomic<bool> valid{true};
        mutable std::mutex qmutex;
        std::deque<T> queue;
        std::condition_variable changed;
      };
      
      class AuthorizationMiddleware
      {
      public:
        struct context
        {};
        
        void before_handle(crow::request& req, crow::response& res, context& /*ctx*/)
        {
          if (authorization_key == "")
            return;
          auto auth = req.headers.find("Authorization");
          std::regex exp("([bB]earer\\s+)?" + authorization_key);
          if (auth == req.headers.end() || !std::regex_match(auth->second, exp))
          {
            res = JSONResponse::make_error(401, "You are not authorized to access this service");
            res.end();
            CROW_LOG_ERROR << "Unauthorized request";
            return;
          }
        }
        
        void after_handle(crow::request& /*req*/, crow::response& /*res*/, context& /*ctx*/)
        { /* no-op */ }
        
        void SetAuthorization(std::string authorization_key)
        {
          this->authorization_key = std::move(authorization_key);
        }
      protected:
        std::string authorization_key;
      };
      
    public:
      RESTTester(StateManager<Input, State, CostStructure>& sm, OutputManager<Input, Output, State>& om);
      /** Virtual destructor. */
      virtual ~RESTTester() { Destroy(); }
      void Run();
    private:
      // a single thread worker that takes care of the task execution
      void Worker();
      // the garbage collector that frees the results in memory
      void Cleaner(std::chrono::minutes interval);
      void Destroy();
      void InitializeParameters();
      
      const unsigned int numThreads;
      
      StateManager<Input, State, CostStructure>& sm;
      OutputManager<Input, Output, State>& om;
      
      // utils
      std::vector<string> runner_urls;
      std::map<string, Runner<Input, State, CostStructure>*> runner_map;
      
      // endpoints management
      void RootEndpoint(const crow::request& req, crow::response& res) const;
      
      std::shared_ptr<Task> CreateTask(float timeout, std::unique_ptr<Input> p_in, std::unique_ptr<State> p_st, std::unique_ptr<Runner<Input, State, CostStructure>> p_r, json parameters, std::string callback_url);
      json TaskStatus(std::string task_id) const;
      json Solution(std::string task_id, bool force_partial) const;
      json RemoveTask(std::string task_id);

      TaskQueue<std::shared_ptr<Task>> task_queue;
      mutable std::mutex task_status_mutex;
      std::map<std::string, std::shared_ptr<Task>> task_status;
      std::atomic<bool> done;
      std::vector<std::thread> workers;
      // the port to bind the service to
      Parameter<unsigned int> port;
      // toward a basic authorization mechanism
      Parameter<std::string> authorization;
    };
    
    template <class Input, class Output, class State, class CostStructure>
    RESTTester<Input, Output, State, CostStructure>::RESTTester(StateManager<Input, State, CostStructure>& sm, OutputManager<Input, Output, State>& om)
    : Parametrized("REST", "REST tester"), numThreads(std::max(std::thread::hardware_concurrency(), 2u) - 1u), sm(sm), om(om)
    {
      for (auto r : this->runners)
      {
        runner_urls.push_back("/runner/" + r->name);
        runner_map[r->name] = r;
      }
      done = false;
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
      // default: 18080
      port = 18080;
      authorization("authorization", "Authorization key", this->parameters);
      // default: no authorization
      authorization = "";
    }
    
    template <class Input, class Output, class State, class CostStructure>
    void RESTTester<Input, Output, State, CostStructure>::Worker()
    {
      while (!done)
      {
        std::shared_ptr<Task> task;
        if (task_queue.WaitDequeue(task))
        {
          try
          {
            {
              std::lock_guard<std::mutex> lock(task_status_mutex);
              task->running = true;
              task->started = std::chrono::system_clock::now();
            }
            CROW_LOG_INFO << "Starting execution of task_id " << task->task_id << " with runner " << task->p_r->name;
            // run the task synchronously (already are in a different thread)
            task->p_r->SyncRun(task->timeout, *(task->p_in), *(task->p_st));
            CROW_LOG_INFO << "Ended execution of task_id " << task->task_id << " with runner " << task->p_r->name;
            {
              std::lock_guard<std::mutex> lock(task_status_mutex);
              task->running = false;
              task->finished = true;
              task->completed = std::chrono::system_clock::now();
            }
            // TODO: this could also be in another async thread
            if (task->callback_url != "")
            {
              CROW_LOG_INFO << "Sending callback of task_id " << task->task_id << " to url " << task->callback_url;
              json result = this->Solution(task->task_id, false);
              uc::curl::easy curl(task->callback_url);
              auto header = uc::curl::create_slist("Content-Type: application/json");
              curl.header(header).postfields(result.dump()).perform();
              CROW_LOG_INFO << "Callback of task_id " << task->task_id << " to url " << task->callback_url << " answered " <<  curl.getinfo<CURLINFO_RESPONSE_CODE>();
            }
          }
          catch (exception& e)
          {
            // this is meant to keep the worker alive also in case of exceptions
            CROW_LOG_ERROR << "Exception occurred in worker " << e.what();
          }
        }
        std::this_thread::yield();
      }
    }
    
    template <class Input, class Output, class State, class CostStructure>
    void RESTTester<Input, Output, State, CostStructure>::Cleaner(std::chrono::minutes interval)
    {
      while (!done)
      {
        std::this_thread::sleep_for(interval);
        {
          std::lock_guard<std::mutex> lock(task_status_mutex);
          std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
          unsigned int count = 0;
          for (auto it = task_status.begin(); it != task_status.end(); )
          {
            const auto& task = it->second;
            if (task->finished && (now - task->completed) > interval)
            {
              it = task_status.erase(it);
              count++;
            }
            else
              ++it;
          }
          CROW_LOG_INFO << "Cleaning performed, removed " << count << " old tasks";
        }
      }
    }
    
        
    template <class Input, class Output, class State, class CostStructure>
    void RESTTester<Input, Output, State, CostStructure>::Run()
    {
      // prepare workers
      try
      {
        for (unsigned i = 0; i < numThreads; i++)
          workers.emplace_back(&RESTTester<Input, Output, State, CostStructure>::Worker, this);
        // run the solution cleaner every 15 minutes
        workers.emplace_back(&RESTTester<Input, Output, State, CostStructure>::Cleaner, this, std::chrono::minutes(15));
      }
      catch(...)
      {
        Destroy();
        throw;
      }

      crow::App<AuthorizationMiddleware> app;
      // setup authorization middleware
      if (std::string(authorization) == "")
        ;
      else
      {
        AuthorizationMiddleware& mw = app.template get_middleware<AuthorizationMiddleware>();
        mw.SetAuthorization(authorization);
      }
      
      // NOTE: standard CROW_ROUTE dispatching seems not to work with this compiler and the Middleware enabled version of the app.
      //       Currently it has been changed to dynamic_routes (maybe less efficient, but this is not the point here)
      
      // This endpoint just lists the available services
      app.route_dynamic("/")([this](const crow::request& req, crow::response& res) { this->RootEndpoint(req, res); });
      
      // This endpoint allows to interact with a specific runner
      app.route_dynamic("/runner/<string>")
      .methods("GET"_method)([this](const crow::request& req, crow::response& res, std::string name) {
        json response;
        auto it = this->runner_map.find(name);
        if (it == this->runner_map.end())
        {
          res = JSONResponse::make_error(404, "Runner `" + name + "` does not exist or is not active");
          res.end();
          return;
        }
        response["parameters"] = it->second->ParametersDescriptionToJSON();
        // TODO: also add to the response the list of the running instances of this runner
        res = JSONResponse::make_response(200, response);
        res.end();
      });
      
      app.route_dynamic("/runner/<string>")
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
            CROW_LOG_ERROR << "Wrong Content-Type";
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
            CROW_LOG_ERROR << "Input file did not comply with the format expected by the system";
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
              CROW_LOG_ERROR << "Initial solution did not comply with the format expected by the system";
              return;
            }
          }
          float timeout = req.url_params.get("timeout") ? std::atof(req.url_params.get("timeout")) : 0.0;
          std::string callback_url = req.url_params.get("callback_url") ? URLDecode(req.url_params.get("callback_url")) : "";
          CROW_LOG_INFO << callback_url;
          auto p_r = it->second->Clone();
          std::shared_ptr<Task> task = this->CreateTask(timeout, std::move(in), std::move(p_st), std::move(p_r), parameters, callback_url);
          {
            std::lock_guard<std::mutex> lock(task_status_mutex);
            response["task_id"] = task->task_id;
            response["url"] = "/running/" + task->task_id;
            response["submitted"] = getISOTimestamp(task->submitted);
          }
          res = JSONResponse::make_response(200, response);
          res.end();
          CROW_LOG_INFO << "Submitted " << task->task_id << " on runner " << task->p_r->name;
          return;
        }
        catch (std::exception& e)
        {
          res = JSONResponse::make_error(405, e.what());
          res.end();
          CROW_LOG_ERROR << "Error: " << e.what();
          return;
        }
      });
      
      // This endpoint allow to check the status of a specific runner request and abort it
      app.route_dynamic("/running/<string>")
      .methods("GET"_method)([this](std::string task_id) {
        json response = this->TaskStatus(task_id);
        if (response.find("error") == response.end())
          return JSONResponse::make_response(200, response);
        else
          return JSONResponse::make_error(404, response["error"]);
      });
      // TODO: add better fine-grained authorization (e.g. JWT)
      app.route_dynamic("/running/<string>")
      .methods("DELETE"_method)([this](const crow::request& req, crow::response& res, std::string task_id) {
        json response = this->RemoveTask(task_id);
        res = JSONResponse::make_response(200, response);
        res.end();
        CROW_LOG_INFO << "Handling removal of task_id " << task_id;
      });
      
      // This endpoint allow to access a solution, when available
      app.route_dynamic("/solution/<string>")
      .methods("GET"_method)([this](const crow::request& req, crow::response& res, std::string task_id) {
        bool force_partial = req.url_params.get("partial") ? std::string(req.url_params.get("partial")) == "true" : false;
        json response = this->Solution(task_id, force_partial);
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
        const auto task = task_it.second;
        response["tasks"].push_back({
          { "runner", task->p_r->name },
          { "task_id", task->task_id },
          { "submitted", getISOTimestamp(task->submitted) },
          { "started", task->running || task->finished ? getISOTimestamp(task->started) : std::string("") },
          { "completed", task->finished ? getISOTimestamp(task->completed) : std::string("") },
          { "finished", task->finished },
          { "running", task->running },
          { "url", "/running/" + task->task_id }
        });
      }
      res = JSONResponse::make_response(200, response);
      res.end();
    }
    
    template <class Input, class Output, class State, class CostStructure>
    std::shared_ptr<typename RESTTester<Input, Output, State, CostStructure>::Task> RESTTester<Input, Output, State, CostStructure>::CreateTask(float timeout, std::unique_ptr<Input> p_in, std::unique_ptr<State> p_st, std::unique_ptr<Runner<Input, State, CostStructure>> p_r, json parameters, std::string callback_url)
    {
      static unsigned long counter = 0;
      // the lock is here, because also counter has to be guarded
      std::lock_guard<std::mutex> lock(task_status_mutex);
      std::string task_id = std::to_string(std::hash<std::string>()(p_r->name + std::to_string(counter)));
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
      std::shared_ptr<Task> task = std::make_shared<Task>(task_id, std::move(p_in), std::move(p_st), std::move(p_r), _timeout, callback_url);
      task_status[task_id] = task;
      task_queue.Enqueue(task);
      return task;
    }
    
    template <class Input, class Output, class State, class CostStructure>
    json RESTTester<Input, Output, State, CostStructure>::TaskStatus(std::string task_id) const
    {
      std::lock_guard<std::mutex> lock(task_status_mutex);
      json status;
      status["task_id"] = task_id;
      auto it = task_status.find(task_id);
      if (it == task_status.end()) // task_id does not exist
      {
        status["error"] = "The task `" + task_id + "` does not exist (or it has been removed because too old)";
        return status;
      }
      const auto task = it->second;
      status["runner"] = task->p_r->name;
      if (task->finished)
      {
        status["finished"] = true;
        status["submitted"] = getISOTimestamp(task->submitted);
        status["started"] = getISOTimestamp(task->started);
        status["completed"] = getISOTimestamp(task->completed);
        status["cost"] = sm.JSONCostFunctionComponents(*(task->p_in), *(task->p_st));
        status["solution_url"] = "/solution/" + task->task_id;
      }
      else if (task->running)
      {
        status["finished"] = false;
        status["running"] = true;
        status["submitted"] = getISOTimestamp(task->submitted);
        status["started"] = getISOTimestamp(task->started);
        // FIXME: call best_state_cost instead, however it's not jsonized yet
        status["cost"] = sm.JSONCostFunctionComponents(*(task->p_in), *(task->p_r->GetCurrentBestState()));
      }
      else
      {
        status["finished"] = false;
        status["running"] = false;
        status["submitted"] = getISOTimestamp(task->submitted);
      }
      return status;
    }
    
    template <class Input, class Output, class State, class CostStructure>
    json RESTTester<Input, Output, State, CostStructure>::Solution(std::string task_id, bool force_partial) const
    {
      std::lock_guard<std::mutex> lock(task_status_mutex);
      json status;
      status["task_id"] = task_id;
      auto it = task_status.find(task_id);
      if (it == task_status.end()) // task_id does not exist
      {
        status["error"] = "The task `" + task_id + "` does not exist (or it has been removed because too old)";
        return status;
      }
      const auto task = it->second;
      status["runner"] = task->p_r->name;
      if (task->finished)
      {
        status["finished"] = true;
        status["submitted"] = getISOTimestamp(task->submitted);
        status["started"] = getISOTimestamp(task->started);
        status["completed"] = getISOTimestamp(task->completed);
        status["solution"] = om.ConvertToJSON(*(task->p_in), *(task->p_st));
      }
      else if (!force_partial)
      {
        status["error"] = "The task `" + task_id + "` has not finished yet";
      }
      else
      {
        auto p_st = task->p_r->GetCurrentBestState();
        status["finished"] = false;
        status["running"] = true;
        status["submitted"] = getISOTimestamp(task->submitted);
        status["started"] = getISOTimestamp(task->started);
        status["cost"] = sm.JSONCostFunctionComponents(*(task->p_in), *(p_st));
        status["solution"] = om.ConvertToJSON(*(task->p_in), *(p_st));
      }
      return status;
    }
    
    template <class Input, class Output, class State, class CostStructure>
    json RESTTester<Input, Output, State, CostStructure>::RemoveTask(std::string task_id)
    {
      std::lock_guard<std::mutex> lock(task_status_mutex);
      json status;
      status["task_id"] = task_id;
      auto it = task_status.find(task_id);
      if (it == task_status.end()) // task_id does not exist
      {
        status["error"] = "The task `" + task_id + "` does not exist (or it has been removed because too old)";
        return status;
      }
      const auto& task = it->second;
      status["runner"] = task->p_r->name;
      task->p_r->Abort(); // if in doubt, is better to ensure that the runner stops
      task_status.erase(it);
      task_queue.Remove([task_id](const std::shared_ptr<Task>& task) {
        return task->task_id == task_id;
      });
      status["message"] = "Removal of task `" + task_id + "` successful";
      return status;
    }
  }
}
