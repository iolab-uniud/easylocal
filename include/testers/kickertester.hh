#pragma once

#include <chrono>

#include "testers/componenttester.hh"
#include "utils/parameter.hh"
#include "helpers/kicker.hh"

namespace EasyLocal
{

namespace Debug
{

/** The Kicker Tester allows to test a Kicker.
 @ingroup Testers
 */
template <class Input, class Solution, class Move, class CostStructure = DefaultCostStructure<int>>
class KickerTester
: public ComponentTester<Input, Solution, CostStructure>,
public Core::CommandLineParameters::Parametrized,
public ChoiceReader
{
public:
    typedef typename CostStructure::CFtype CFtype;
    
    KickerTester(const Input &in,
                 Core::SolutionManager<Input, Solution, CostStructure> &sm,
                 Core::Kicker<Input, Solution, Move, CostStructure> &k,
                 std::string name, Tester<Input, Solution, CostStructure> &t, std::ostream &o = std::cout);
    virtual size_t Modality() const;
    
    void RunMainMenu(Solution&st);
    
protected:
    void PrintKicks(size_t length, const Solution &st) const;
    void ShowMenu();
    bool ExecuteChoice(Solution&st);
    const Input &in;
    Core::SolutionManager<Input, Solution, CostStructure> &sm; /**< A pointer to the attached
                                                                state manager. */
    int choice;                                          /**< The option currently chosen from the menu. */
    Core::Kicker<Input, Solution, Move, CostStructure> &kicker;
    std::ostream &os;
    Parameter<unsigned int> length;
};

/*************************************************************************
 * Implementation
 *************************************************************************/

template <class Input, class Solution, class Move, class CostStructure>
KickerTester<Input, Solution, Move, CostStructure>::KickerTester(const Input &in,
                                                                 Core::SolutionManager<Input, Solution, CostStructure> &sm,
                                                                 Core::Kicker<Input, Solution, Move, CostStructure> &k, std::string name, Tester<Input, Solution, CostStructure> &t, std::ostream &os)
: ComponentTester<Input, Solution, CostStructure>(name), Core::CommandLineParameters::Parametrized(name, "Kicker tester parameters"), in(in), sm(sm), kicker(k), os(os)
{
    t.AddKickerTester(*this);
    length("kick-length", "Kick length", this->parameters);
    length = 3;
}


/**
 Manages the component tester menu for the given state.
 @param st the state to test
 */
template <class Input, class Solution, class Move, class CostStructure>
void KickerTester<Input, Solution, Move, CostStructure>::RunMainMenu(Solution&st)
{
    Core::CommandLineParameters::Parametrized::ReadParameters();
    bool show_state;
    do
    {
        ShowMenu();
        if (choice != 0)
        {
            
            std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
            show_state = ExecuteChoice(st);
            std::chrono::milliseconds duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start);
            
            if (show_state)
            {
                // FIXME: pretty print state
                os << "CURRENT SOLUTION " << std::endl
                << st << std::endl;
                os << "CURRENT COST : " << sm.CostFunctionComponents(st) << std::endl;
            }
            os << "ELAPSED TIME : " << duration.count() / 1000.0 << " s" << std::endl;
        }
    } while (choice != 0);
    os << "Leaving " << this->name << " menu" << std::endl;
}

/**
 Outputs the menu options.
 */
template <class Input, class Solution, class Move, class CostStructure>
void KickerTester<Input, Solution, Move, CostStructure>::ShowMenu()
{
    os << "Kicker \"" << this->name << "\" Menu:" << std::endl
    << "    (1) Perform Random Kick" << std::endl
    << "    (2) Perform Best Kick" << std::endl
    << "    (3) Perform First Improving Kick" << std::endl
    << "    (4) Show All Kicks" << std::endl
    << "    (0) Return to Main Menu" << std::endl
    << "Your choice : ";
    choice = this->ReadChoice(std::cin);
}

/**
 Execute the menu choice on the given state.
 
 @param st the current state
 */
template <class Input, class Solution, class Move, class CostStructure>
bool KickerTester<Input, Solution, Move, CostStructure>::ExecuteChoice(Solution&st)
{
    bool execute_kick = false;
    Kick<Solution, Move, CostStructure> kick;
    DefaultCostStructure<CFtype> cost;
    try
    {
        switch (choice)
        {
            case 1:
                std::tie(kick, cost) = kicker.SelectRandom(length, st);
                os << kick << " " << cost << std::endl;
                execute_kick = true;
                break;
            case 2:
                std::tie(kick, cost) = kicker.SelectBest(length, st);
                os << kick << " " << cost << std::endl;
                execute_kick = true;
                break;
            case 3:
                std::tie(kick, cost) = kicker.SelectFirst(length, st);
                os << kick << " " << cost << std::endl;
                execute_kick = true;
                break;
            case 4:
                PrintKicks(length, st);
                break;
            default:
                os << "Invalid choice" << std::endl;
        }
        if (execute_kick)
            kicker.MakeKick(st, kick);
        return execute_kick;
    }
    catch (EmptyNeighborhood)
    {
        os << "Empty neighborhood." << std::endl;
        return false;
    }
}

template <class Input, class Solution, class Move, class CostStructure>
void KickerTester<Input, Solution, Move, CostStructure>::PrintKicks(size_t length, const Solution &st) const
{
    for (auto it = kicker.begin(length, st); it != kicker.end(length, st); ++it)
    os << *it << std::endl;
}

template <class Input, class Solution, class Move, class CostStructure>
size_t KickerTester<Input, Solution, Move, CostStructure>::Modality() const
{
    return kicker.Modality();
}
} // namespace Debug

} // namespace EasyLocal
