#pragma once

#include <chrono>
#include <cmath>
#include "runners/simulatedannealingtimebased.hh"

namespace EasyLocal
{
  namespace Core
  {
    struct LearningData
    {
      int improving = 0, sideways = 0, accepted = 0, evaluated = 0;
      double global_improvement = 0.0;
      std::chrono::nanoseconds global_evaluation_time;
    };

    template <class Input, class Solution, class Move, class CostStructure = DefaultCostStructure<int>>
    class SimulatedAnnealingWithLearning : public SimulatedAnnealingTimeBased<Input, Solution, Move, CostStructure>
    {
    public:
      SimulatedAnnealingWithLearning(const Input &in, SolutionManager<Input, Solution, CostStructure> &sm,
                                    NeighborhoodExplorer<Input, Solution, Move, CostStructure> &ne,
                                    std::string name) : SimulatedAnnealingTimeBased<Input, Solution, Move, CostStructure>(in, sm, ne, name)
      {
        learning_data.resize(ne.Modality());
        learning_rate = 0.05;        // higher values imply faster learning
        min_threshold = 0.05;        // probabilities are lower-bounded by this value
        time_smoother = 1.0;                  // 0 = no time_smoother, 1 = linear, 2 = sqrt, 3 = log10

      }
      void SetLearningRate(double r) {learning_rate = r;}
      void SetThreshold(double t) {min_threshold = t;}
      void SetTimeSmoother(double t) {time_smoother = t;} 
      void CompleteMove() override 
      {
        SimulatedAnnealingTimeBased<Input, Solution, Move, CostStructure>::CompleteMove();
        //size_t i = Impl::MoveDispatcher<decltype(this->current_move), this->ne.Modality() - 1>::get_first_active(this->current_move, 0);
        
        int active_move_index = this->ne.GetActiveMove(this->current_move.move);
        CostStructure move_cost = this->current_move.cost;
        
        learning_data[active_move_index].accepted++;
        if(move_cost.total < 0 )
          {
            learning_data[active_move_index].improving++;
            learning_data[active_move_index].global_improvement-=move_cost.total;  // Note: move_cost.total is negative when improving
          }
        else if(move_cost.total == 0 )
          learning_data[active_move_index].sideways++;    
        // update counters:
        //   - retrieve the active move (how?)
        //   - retrieve the delta_cost
        //   - update the learning data of the neighborhood
      }

      virtual void ApplyLearning()
      {
            //vector<double> avg_improvement(this->ne.Modality(),0);
            std::vector<double> reward(this->ne.Modality(),0);
            //double total_avg_improvement = 0;
            double total_reward = 0;
    #if VERBOSE >= 1
            std::cerr << "V1 ";
            this->PrintStatus(std::cerr);
            std::cerr << ", time = " << std::chrono::duration_cast<std::chrono::milliseconds>(this->temperature_start_time-this->run_start).count()/1000.0;
            std::cerr << ", rates: (";
            double sum_rates = 0;
            for(unsigned int i = 0; i < this->ne.Modality(); i++)
              {
                std::cerr << "" << this->ne.GetBias(i) << (i < this->ne.Modality() -1 ? "/" : ") ");
                sum_rates+=this->ne.GetBias(i);
              }
            std::cerr << "[" << sum_rates << "] ";
           std::cerr << "acceptance: (";
            for(unsigned int i = 0; i < this->ne.Modality(); i++)
              {
                std::cerr << "" << static_cast<double>(learning_data[i].accepted)/learning_data[i].evaluated << (i < this->ne.Modality() -1 ? "/" : ") ");
              }
    #elif VERBOSE >= 2
          if(this->number_of_temperatures == 1) //I print the header
          {
            std::cerr << "n_temp,"
                      << "time,"
                      << "temperature,"
                      << "sampled,"
                      << "accepted,"
                      << "acceptance,"
                      << "current_cost,"
                      << "best_cost,";
                      for(unsigned int i = 0; i < this->ne.Modality(); i++)
                        std::cerr<< "rate_"<<i<<",";
                      std::cerr << "check,";  
                      for(unsigned int i = 0; i < this->ne.Modality(); i++)
                        std::cerr<< "acceptance_"<<i<<",";
                      for(unsigned int i = 0; i < this->ne.Modality(); i++)
                      {
                        std::cerr << "evaluated_"<<i<<","
                                  << "accepted_"<<i<<","
                                  << "improving_"<<i<<","
                                  << "sideways_"<<i<<","
                                  << "global_improvement_"<<i<<","
                                  << "avgtime_"<<i<<",";
                      }     
                      for(unsigned int i = 0; i < this->ne.Modality(); i++)
                      {
                        std::cerr << "reward_"<<i<<"";
                        if(i < this->ne.Modality()-1)
                          std::cerr << ",";
                        else
                          std::cerr << std::endl;
                      }                
          }
          std::cerr << this->number_of_temperatures << ","
                    << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()- this->run_start).count()/1000.0 << ","
                    << this->Temperature() << ","
                    << this->neighbors_sampled << "," 
                    << this->neighbors_accepted << ","
                    << static_cast<double>(this->neighbors_accepted)/this->neighbors_sampled << ","
                    << this->current_state_cost.total << "," 
                    << this->best_state_cost.total << ",";
            double sum_rates = 0;
            for(unsigned int i = 0; i < this->ne.Modality(); i++)
              {
                std::cerr << this->ne.GetBias(i) << ",";
                sum_rates+=this->ne.GetBias(i);
              }
            std::cerr << sum_rates << ",";
            for(unsigned int i = 0; i < this->ne.Modality(); i++)
              {
                std::cerr << static_cast<double>(learning_data[i].accepted)/learning_data[i].evaluated << ",";
              }
    #endif
            for(unsigned int i = 0; i < this->ne.Modality(); i++)
              {
    #if VERBOSE == 1
                std::cerr << i << "[" 
                          << learning_data[i].evaluated << "/"
                          << learning_data[i].accepted << "/"
                          << learning_data[i].improving << "/"
                          << learning_data[i].sideways << "/"
                          << learning_data[i].global_improvement << "/";
                          if(learning_data[i].accepted > 0)
                            std::cerr << learning_data[i].global_evaluation_time.count()/static_cast<double>(learning_data[i].accepted) << "] ";
                          else  
                            std::cerr << "nan" << "] ";
    #elif VERBOSE >= 2
                std::cerr << learning_data[i].evaluated << ","
                          << learning_data[i].accepted << ","
                          << learning_data[i].improving << ","
                          << learning_data[i].sideways << ","
                          << learning_data[i].global_improvement << ",";
                          if(learning_data[i].accepted > 0)
                            std::cerr << learning_data[i].global_evaluation_time.count()/static_cast<double>(learning_data[i].accepted) << ",";
                          else  
                            std::cerr << "nan" << ",";
    #endif    
                

                reward[i] = ComputeNHReward(i);

                total_reward += reward[i];
              }

            //I give the reward (reinforcement rule)

    #if VERBOSE == 1
            std::cerr << "rewards: (";
    #endif
            double used_threshold = 0.0;
            std::vector<bool> applied_threshold(this->ne.Modality(),false);
            int how_many_threshold_applied = 0;
            for(unsigned int i = 0; i < this->ne.Modality(); i++)
              {
                if(total_reward > 0.0)
                  reward[i] = reward[i] / total_reward;
                else
                  reward[i] = 1.0/this->ne.Modality();
    #if VERBOSE == 1
                std::cerr << "" << reward[i] << (i < this->ne.Modality() -1 ? "/" : ") ");
    #elif VERBOSE >= 2
                std::cerr << reward[i] << (i < this->ne.Modality() -1 ? "," : "");
    #endif 
                if((1-learning_rate)*this->ne.GetBias(i) + learning_rate*reward[i] < min_threshold)
                  {
                    used_threshold+=(min_threshold-((1-learning_rate)*this->ne.GetBias(i) + learning_rate*reward[i]));
                    applied_threshold[i] = true;
                    this->ne.SetBias(i, min_threshold);
                    how_many_threshold_applied++;
                  }
                else
                  {
                    this->ne.SetBias(i, (1-learning_rate)*this->ne.GetBias(i) + learning_rate*reward[i]);
                  }
                learning_data[i].accepted = 0;
                learning_data[i].improving = 0;
                learning_data[i].sideways = 0;
                learning_data[i].evaluated = 0;
                learning_data[i].global_improvement = 0.0;
                learning_data[i].global_evaluation_time = std::chrono::nanoseconds(0);
              }
            //I re balance the threshold
            if(how_many_threshold_applied > 0 && how_many_threshold_applied < static_cast<int>(this->ne.Modality()))
              {
                for(unsigned int i = 0; i < this->ne.Modality(); i++)
                  if(!applied_threshold[i])
                    this->ne.SetBias(i, this->ne.GetBias(i) - used_threshold/(this->ne.Modality() - how_many_threshold_applied));
              }

    #if VERBOSE >= 1
            std::cerr << std::endl;
    #endif
          
      }


      void CompleteIteration() override 
      {
        if (this->CoolingNeeded()) // the batch is finished -> update probabilities and reset learning data
          ApplyLearning();
        SimulatedAnnealingTimeBased<Input, Solution, Move, CostStructure>::CompleteIteration();
      }


      void SelectMove() override 
      {
        bool accepted = false;
        do
        {
          //          std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::high_resolution_clock::now(); 
          std::chrono::time_point<std::chrono::system_clock> start = std::chrono::high_resolution_clock::now(); 
          //          auto start = std::chrono::high_resolution_clock::now(); 
          this->ne.RandomMove(*this->p_current_state, this->current_move.move); //TO DO: ,this->weights);
          this->current_move.cost = this->ne.DeltaCostFunctionComponents(*this->p_current_state, this->current_move.move);
          this->current_move.is_valid = true;
          if (this->current_move.cost <= 0 || this->current_move.cost < (-this->temperature * log(std::max(Random::Uniform<double>(0.0, 1.0), std::numeric_limits<double>::epsilon()))))
          {
            accepted = true;
            //learning_data[this->ne.GetActiveMove(this->current_move.move)].global_evaluation_time+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::time_point<std::chrono::steady_clock>(std::chrono::high_resolution_clock::now())-start);
            learning_data[this->ne.GetActiveMove(this->current_move.move)].global_evaluation_time+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()-start);
          }
          this->neighbors_sampled++;
          this->evaluations++;
          learning_data[this->ne.GetActiveMove(this->current_move.move)].evaluated++;
        } while(!accepted);
      }

      virtual double ComputeNHReward(unsigned int i)
      {
          double this_reward;
          if(learning_data[i].global_improvement > 0.0)
            this_reward = (learning_data[i].global_improvement/learning_data[i].evaluated)
                      /pow(learning_data[i].global_evaluation_time.count()/static_cast<double>(learning_data[i].accepted),1.0/time_smoother);
          else
            this_reward = 0;
          return this_reward;
      }
    protected:
      std::vector<LearningData> learning_data;
      double learning_rate; 
      double min_threshold;
      double time_smoother;
      // batch size is equal to max_neighbors_sampled in this version
    };
  }
}
