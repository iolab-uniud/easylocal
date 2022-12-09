#pragma once

#include <chrono>
#include "runners/simulatedannealingevaluationbased.hh"

namespace EasyLocal
{
  namespace Core
  {
    struct LearningData
    {
      int improving, sideways, accepted;
      int global_improvement;
      std::chrono::nanoseconds global_evaluation_time;
    };

    template <class Input, class Solution, class Move, class CostStructure = DefaultCostStructure<int>>
    class SimulatedAnnealingWithLearning : public SimulatedAnnealingEvaluationBased<Input, Solution, Move, CostStructure>
    {
    public:
      SimulatedAnnealingWithLearning(const Input &in, SolutionManager<Input, Solution, CostStructure> &sm,
                                    NeighborhoodExplorer<Input, Solution, Move, CostStructure> &ne,
                                    std::string name) : SimulatedAnnealingEvaluationBased<Input, Solution, Move, CostStructure>(in, sm, ne, name)
      {
        learning_data.resize(ne.Modality());
        learning_rate = 0.05;  // higher values imply faster learning
        min_threshold = 0.05;  // probabilities are lower-bounded by this value
      }
      void SetLearningRate(double r) {learning_rate = r;}
      void SetThreshold(double t) {min_threshold = t;}
      void CompleteMove() override 
      {
        SimulatedAnnealingEvaluationBased<Input, Solution, Move, CostStructure>::CompleteMove();
        //size_t i = Impl::MoveDispatcher<decltype(this->current_move), this->ne.Modality() - 1>::get_first_active(this->current_move, 0);
        
        int active_move_index = this->ne.GetActiveMove(this->current_move.move);
        CostStructure move_cost = this->current_move.cost;
        
        learning_data[active_move_index].accepted++;
        if(move_cost.total < 0 )
          {
            learning_data[active_move_index].improving++;
            learning_data[active_move_index].global_improvement-=move_cost.total;
          }
        else if(move_cost.total == 0 )
          learning_data[active_move_index].sideways++;    
        // update counters:
        //   - retrieve the active move (how?)
        //   - retrieve the delta_cost
        //   - update the learning data of the neighborhood
      }

      void CompleteIteration() override 
      {
        if (this->neighbors_sampled >= this->max_neighbors_sampled || this->neighbors_accepted >= this->max_neighbors_accepted)
          {  // the batch is finished -> update probabilities and reset learning data
            //vector<double> avg_improvement(this->ne.Modality(),0);
            vector<double> reward(this->ne.Modality(),0);
            //double total_avg_improvement = 0;
            double total_reward = 0;
    #if VERBOSE >= 1
            std::cerr << "(" << this->number_of_temperatures << ")" 
                << " T = " << this->Temperature() 
                << ", OF = [" << this->current_state_cost.total << "/" << this->best_state_cost.total << "] ";
            std::cerr << "rates: (";
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
                std::cerr << "" << learning_data[i].accepted/(this->ne.GetBias(i)*this->neighbors_sampled) << (i < this->ne.Modality() -1 ? "/" : ") ");
              }
    #endif
            for(unsigned int i = 0; i < this->ne.Modality(); i++)
              {
    #if VERBOSE >= 1
                std::cerr << i << "[" << learning_data[i].accepted << "/"
                    << learning_data[i].improving << "/"
                    << learning_data[i].sideways << "/"
                    << learning_data[i].global_improvement << "] ";
    #endif
                //cerr << "learning_data[i].accepted = " <<learning_data[i].accepted<< ", learning_data[i].global_evaluation_time.count() = " << learning_data[i].global_evaluation_time.count() << endl;
                if(learning_data[i].global_improvement > 0)
                  reward[i] = (static_cast<double>(learning_data[i].global_improvement)/(this->ne.GetBias(i)*this->neighbors_sampled)) * (static_cast<double>(learning_data[i].accepted)/learning_data[i].global_evaluation_time.count());
                else
                  reward[i] = 0;
                total_reward += reward[i];
              }

            //I give the reward (reinforcement rule)
    #if VERBOSE >= 1
            std::cerr << "rewards: (";
    #endif
            double used_threshold = 0.0;
            vector<bool> applied_threshold(this->ne.Modality(),false);
            int how_many_threshold_applied = 0;
            for(unsigned int i = 0; i < this->ne.Modality(); i++)
              {
                if(total_reward!=0)
                  reward[i] = reward[i] / total_reward;
                else
                  reward[i] = 1.0/this->ne.Modality();
    #if VERBOSE >= 1
                std::cerr << "" << reward[i] << (i < this->ne.Modality() -1 ? "/" : ") ");
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
                learning_data[i].global_improvement = 0;
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
            std::cerr << endl;
    #endif
          }
        SimulatedAnnealingEvaluationBased<Input, Solution, Move, CostStructure>::CompleteIteration();
      }
     #ifndef DUMMY
      void SelectMove() override 
      {
        bool accepted = false;
        do
        {
          std::chrono::time_point<std::chrono::system_clock> start = std::chrono::high_resolution_clock::now(); 
          this->ne.RandomMove(*this->p_current_state, this->current_move.move); //TO DO: ,this->weights);
          this->current_move.cost = this->ne.DeltaCostFunctionComponents(*this->p_current_state, this->current_move.move);
          this->current_move.is_valid = true;
          if (this->current_move.cost <= 0 || this->current_move.cost < (-this->temperature * log(std::max(Random::Uniform<double>(0.0, 1.0), std::numeric_limits<double>::epsilon()))))
          {
            accepted = true;
            learning_data[this->ne.GetActiveMove(this->current_move.move)].global_evaluation_time+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()-start);
            cout << "["<< this->ne.GetActiveMove(this->current_move.move) <<"]" << std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()-start).count() << endl;
          }
          this->neighbors_sampled++;
          this->evaluations++;
        } while(!accepted);
     
      }
     #endif
    protected:
      vector<LearningData> learning_data;
      double learning_rate; 
      double min_threshold;
      // batch size is equal to max_neighbors_sampled in this version
    };
  }
}
