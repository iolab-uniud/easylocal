#if !defined(_SHIFTING_PENALTY_OBSERVER_HH_)
#define _SHIFTING_PENALTY_OBSERVER_HH_

namespace EasyLocal {
    
  namespace Debug {
    
    template <typename CFtype>
    class ShiftingPenaltyManager;

    template <typename CFtype>
    class ShiftingPenaltyObserver
    {
    public:
      ShiftingPenaltyObserver(std::ostream& r_os = std::cout);
      void NotifyReset(ShiftingPenaltyManager<CFtype>& r);
      void NotifyUpdate(ShiftingPenaltyManager<CFtype>& r, CFtype cost);
      void NotifyNewThreshold(ShiftingPenaltyManager<CFtype>& r);
    protected:
      std::ostream& os;
    };

    template <typename CFtype>
    ShiftingPenaltyObserver<CFtype>::ShiftingPenaltyObserver(std::ostream& r_os) 
      : os(r_os) {}

    template <typename CFtype>
    void ShiftingPenaltyObserver<CFtype>::NotifyReset(ShiftingPenaltyManager<CFtype>& r)
    {
      os << "Reset: " << r.name << " " << r.shift << " " << (CFtype)0 << std::endl;
    }

    template <typename CFtype>
    void ShiftingPenaltyObserver<CFtype>::NotifyUpdate(ShiftingPenaltyManager<CFtype>& r, CFtype cost)
    {
      os << "Update: " << r.name << " " << r.shift << " " << cost << std::endl;
    }

    template <typename CFtype>
    void  ShiftingPenaltyObserver<CFtype>::NotifyNewThreshold(ShiftingPenaltyManager<CFtype>& r)
    {
      os << "NewThreshold: " << r.name << " " << r.cost_threshold << std::endl;
    }
                    
  }
}

#endif // _SHIFTING_PENALTY_OBSERVER_HH_
