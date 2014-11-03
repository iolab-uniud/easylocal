//
//  shiftingpenaltyrunner.hh
//  EasyLocalExamples
//
//  Created by Luca Di Gaspero on 03/11/14.
//
//

#ifndef _shiftingpenaltyrunner_hh
#define _shiftingpenaltyrunner_hh

#include "easylocal/runners/moverunner.hh"

namespace EasyLocal {
  namespace Core {
    
    template <class Input, class State, class Move, typename CFtype, class MR>
    class ShiftingPenaltyRunner : public MR
    {
      // TODO: currently it performs weight adaptation at each iteration
      // FIXME: check the SelectMove() in all runners according to the ShiftingPenalty (i.e., whether to consider weighted cost or total cost)
    public:
      using MR::MR;
      
      void RegisterParameters()
      {
        MR::RegisterParameters();
      }
      
      void InitializeRun() throw (ParameterNotSet, IncorrectParameterValue)
      {
        MR::InitializeRun();
        this->weights.resize(CostComponent<Input, State, CFtype>::NumberOfCostComponents(), 1.0);
      }
      
      void CompleteMove()
      {
        MR::CompleteMove();
        std::cout << "new weights {";
        for (size_t i = 0; i < CostComponent<Input, State, CFtype>::NumberOfCostComponents(); i++)
        {
          if (this->current_state_cost.all_components[i] == 0)
            this->weights[i] /= Random::Double(1.02, 1.15);
          if (this->current_state_cost.all_components[i] > 0)
            this->weights[i] *= Random::Double(1.02, 1.15);
          if (i > 0)
            std::cout << ", ";
          std::cout << this->weights[i];
        }
        std::cout << "}" << std::endl;
      }
    };    
  }
}


#endif
