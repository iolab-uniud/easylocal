#pragma once

#include "easylocal/runners/moverunner.hh"

namespace EasyLocal
{
  namespace Core
  {
    
    template <class BaseMoveRunner>
    class ShiftingPenaltyRunner : public BaseMoveRunner
    {
      // TODO: currently it performs weight adaptation at each iteration
    public:
      
      typedef typename BaseMoveRunner::Input Input;
      typedef typename BaseMoveRunner::State State;
      typedef typename BaseMoveRunner::Move Move;
      typedef typename BaseMoveRunner::CostStructure CostStructure;
      
      using BaseMoveRunner::BaseMoveRunner;
      
      std::unique_ptr<Runner<Input, State, CostStructure>> Clone() const override
      {
        return std::make_unique<ShiftingPenaltyRunner>(*this);
      }
      
      void InitializeParameters() override
      {
        BaseMoveRunner::InitializeParameters();
        feasible_iterations("feasible_iterations", "Number of feasible iterations before perturbing the weight", this->parameters);
        feasible_iterations = 1;
        infeasible_iterations("infeasible_iterations", "Number of infeasible iterations before perturbing the weight", this->parameters);
        infeasible_iterations = 1;
        min_perturbation("min_perturbation", "Minimum perturbation ratio applied to the weight (value > 1.0)", this->parameters);
        min_perturbation = 1.03;
        max_perturbation("max_perturbation", "Maximum perturbation ratio applied to the weight (value > 1.0)", this->parameters);
        max_perturbation = 1.08;
        min_range("min_range", "Minimum value for the weight", this->parameters);
        min_range = 0.001;
        max_range("min_range", "Maximum value for the weight", this->parameters);
        max_range = 10.0;
      }
      
      void InitializeRun(const Input& in) override
      {
        BaseMoveRunner::InitializeRun(in);
        if (min_perturbation <= 1.0)
        {
          throw IncorrectParameterValue(min_perturbation, "should be greater than one");
        }
        if (max_perturbation <= 1.0)
        {
          throw IncorrectParameterValue(max_perturbation, "should be greater than one");
        }
        if (min_perturbation > max_perturbation)
        {
          throw IncorrectParameterValue(max_perturbation, "should be greater than min_perturbation");
        }
        if (min_range < 0.0)
        {
          throw IncorrectParameterValue(min_range, "should be greater than zero");
        }
        if (max_range < 0.0)
        {
          throw IncorrectParameterValue(max_range, "should be greater than zero");
        }
        if (min_range > max_range)
        {
          throw IncorrectParameterValue(max_range, "should be greater than min_range");
        }
        if (feasible_iterations == 0)
        {
          throw IncorrectParameterValue(feasible_iterations, "should be greater than zero");
        }
        if (infeasible_iterations == 0)
        {
          throw IncorrectParameterValue(infeasible_iterations, "should be greater than zero");
        }
        this->weights.assign(this->sm.CostComponents(), 1.0);
        number_of_feasible_iterations.assign(this->sm.CostComponents(), 0);
        number_of_infeasible_iterations.assign(this->sm.CostComponents(), 0);
      }
      
      void CompleteMove(const Input& in) override
      {
        BaseMoveRunner::CompleteMove(in);
        for (size_t i = 0; i < this->sm.CostComponents(); i++)
        {
          if (this->sm.GetCostComponent(i).IsHard() && this->current_state_cost.all_components[i] == 0)
          {
            number_of_feasible_iterations[i]++;
            number_of_infeasible_iterations[i] = 0;
            if (number_of_feasible_iterations[i] % (unsigned int)feasible_iterations == 0)
              this->weights[i] = std::max((double)min_range, this->weights[i] / Random::Double(min_perturbation, max_perturbation));
          }
          if (this->sm.GetCostComponent(i).IsHard() && this->current_state_cost.all_components[i] > 0)
          {
            number_of_infeasible_iterations[i]++;
            number_of_feasible_iterations[i] = 0;
            if (number_of_infeasible_iterations[i] % (unsigned int)infeasible_iterations == 0)
              this->weights[i] = std::min((double)max_range, this->weights[i] * Random::Double(min_perturbation, max_perturbation));
          }
        }
      }
      
    protected:
      std::vector<unsigned int> number_of_feasible_iterations, number_of_infeasible_iterations;
      
      Parameter<double> min_range, max_range;
      Parameter<double> min_perturbation, max_perturbation;
      Parameter<unsigned int> feasible_iterations, infeasible_iterations;
    };
  } // namespace Core
} // namespace EasyLocal
