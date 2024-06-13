#pragma once

#include "concepts.hh"
#include <vector>
#include <cassert>

namespace easylocal {

template <InputT Input, SolutionT<Input> Solution, Number T>
class CostComponent
{
public:
    /// Solution is passed as a const reference for performance reasons
    /// Indeed the solution is not acquired by the cost component, but it is used only to
    /// compute the cost (this allows to avoid shared_ptr increment/decrement count which
    /// requires atomic operations and thread syncrhonization)
    virtual T ComputeCost(std::shared_ptr<const Solution> s) const = 0;
    virtual ~CostComponent() = default;
};

// TODO: specify the right type for the cost component
template <InputT Input, SolutionT<Input> Solution, Number T, typename Move>
class DeltaCostComponent
{
public:
    /// Solution is passed as a const reference for performance reasons (see above in @CostComponent)
    virtual T ComputeDeltaCost(std::shared_ptr<const Solution> s, const Move& mv) const = 0;
    virtual ~DeltaCostComponent() = default;
};

template <InputT Input, SolutionT<Input> _Solution, Number _T, CostStructureTd _CostStructure, class NeighborhoodExplorer>
class MoveValue;

template <SolutionManagerT SolutionManager, class Move, class NE>
class NeighborhoodExplorer;

template <SolutionManagerT SolutionManager, class NE, class ...NHEs>
requires (NeighborhoodExplorerT<NHEs> && ...)
class UnionNeighborhoodExplorer;

template <InputT _Input, SolutionT<_Input> _Solution, Number _T>
class AggregatedCostStructure;

template <InputT _Input, SolutionT<_Input> _Solution, Number _T, class _CostStructure>
class SolutionValue : std::vector<std::pair<bool, _T>>
{
public:
    using Input = _Input ;
    using Solution = _Solution;
    using T = _T;
    using CostStructure = _CostStructure ;
    friend CostStructure;
    template <InputT I, SolutionT<I> S, Number T_, CostStructureTd CS, class NE> friend class MoveValue;
    template <SolutionManagerT SM, class Move, class NE> friend class NeighborhoodExplorer;
    template <SolutionManagerT SM, class NE, class ...NHEs> requires (NeighborhoodExplorerT<NHEs> && ...) friend class UnionNeighborhoodExplorer;
    
    std::shared_ptr<const Solution> GetSolution() const
    {
        return sol;
    }
    
    // TODO: this is a bit patchy, just to access a single scalar in case of aggregated costs due to its use in TS, review in another life
    template <typename = std::enable_if<std::same_as<CostStructure, class AggregatedCostStructure<Input, Solution, T>>>>
    T AggregatedCost() const
    {
        return cs->ComputeAggregatedCost(*this);
    }
    
    std::vector<T> GetValues() const
    {
        std::vector<T> tmp;
        tmp.reserve(this->size());
        for (size_t i = 0; i < this->size(); ++i)
            tmp.push_back((*this)[i]);
        return tmp;
    }
    
    bool CheckValues() const
    {
        for (size_t i = 0; i < this->size(); ++i)
            if (cs->ComputeCost(sol, i) != (*this)[i])
                return false;
        return true;
    }
    
    T operator[](size_t i) const
    {
        //const std::lock_guard<std::mutex> lock(compute);
        auto& val = const_cast<std::pair<bool, T>&>(std::vector<std::pair<bool, T>>::operator[](i));
        if (!val.first)
        {
            val.second = cs->ComputeCost(sol, i);
            val.first = true;
        }
        return val.second;
    }
    
    using std::vector<std::pair<bool, T>>::size;
    
    template <SolutionValueT<Input, Solution, T, CostStructure> SV>
    auto operator<=>(const SV& other) const
    {
        return cs->spaceship(*this, other);
    }
    
    template <SolutionValueT<Input, Solution, T, CostStructure> SV>
    auto operator<=>(const std::shared_ptr<SV> other) const
    {
        return cs->spaceship(*this, *other);
    }
    
    template <SolutionValueT<Input, Solution, T, CostStructure> SV>
    auto operator==(const SV& other) const
    {
        return cs->equality(*this, other);
    }
    
    template <NeighborhoodExplorerT NeighborhoodExplorer>
    SolutionValue(const MoveValue<Input, Solution, T, _CostStructure, NeighborhoodExplorer>& m) : cs(m.cs), sol(m.GetSolution())
    {
        auto values = m.GetValues();
        this->reserve(m.size());
        for (size_t i = 0; i < values.size(); ++i)
            (*this).push_back({ true, values[i] });
    }
    
    SolutionValue(const SolutionValue<Input, Solution, T, _CostStructure>& s) : cs(s.cs), sol(s.sol), std::vector<std::pair<bool, T>>(s)
    {}
    
protected:
    SolutionValue(std::shared_ptr<const CostStructure> cs, std::shared_ptr<const Solution> sol, size_t components) : cs(cs), sol(sol), std::vector<std::pair<bool, T>>(components, { false, 0 })
    {
        assert(cs && sol);
    }
    using std::vector<std::pair<bool, T>>::begin;
    using std::vector<std::pair<bool, T>>::end;
    using std::vector<std::pair<bool, T>>::at;
    std::shared_ptr<const CostStructure> cs;
    std::shared_ptr<const Solution> sol;
};

template <InputT _Input, SolutionT<_Input> _Solution, Number _T, CostStructureTd _CostStructure, class _NeighborhoodExplorer>
class MoveValue : std::vector<std::pair<bool, _T>>
{
public:
    using Input = _Input;
    using Solution = _Solution ;
    using T = _T;
    using CostStructure = _CostStructure;
protected:
    using Move = typename _NeighborhoodExplorer::Move;
    using SolutionValue = SolutionValue<Input, _Solution, _T, _CostStructure>;
    friend SolutionValue;
    using NeighborhoodExplorer = typename _NeighborhoodExplorer::ThisClass;
    friend NeighborhoodExplorer;
public:
    
    Move GetMove() const
    {
        return mv;
    }
    
    // TODO: this is a bit patchy, just to access a single scalar in case of aggregated costs due to its use in TS, review in another life
    template <typename = std::enable_if<std::same_as<CostStructure, class AggregatedCostStructure<Input, Solution, T>>>>
    T AggregatedCost() const
    {
        return this->GetSolutionValue().AggregatedCost();
    }
    
    std::vector<T> GetValues() const
    {
        std::vector<T> tmp;
        tmp.reserve(this->size());
        for (size_t i = 0; i < this->size(); ++i)
            tmp.push_back((*this)[i]);
        return tmp;
    }
    
    T operator[](size_t i) const
    {
        auto& val = const_cast<std::pair<bool, T>&>(std::vector<std::pair<bool, T>>::operator[](i));
        if (!val.first)
        {
            // the value has to be computed
            if (ne->HasDeltaCostComponent(i, mv))
            {
                val.second = old_sv[i] + ne->ComputeDeltaCost(old_sv.GetSolution(), mv, i);
                val.first = true;
            }
            else
            {
                if (!new_sol)
                {
                    // make a copy of the solution
                    new_sol = std::make_shared<Solution>(*(old_sv.GetSolution()));
                    // apply the move
                    ne->MakeMove(new_sol, mv);
                }
                // compute the new cost directly from solution
                val.second = cs->ComputeCost(new_sol, i);
                val.first = true;
            }
        }
        return val.second;
    }
    
    template <SolutionValueT<Input, Solution, T, CostStructure> SV>
    auto operator<=>(const SV& other) const
    {
        return cs->spaceship(*this, other);
    }
    
    template <SolutionValueT<Input, Solution, T, CostStructure> SV2>
    auto operator==(const SV2& other) const
    {
        return cs->equality(*this, other);
    }
    
    std::shared_ptr<const Solution> GetSolution() const
    {
        // the new solution has not been determined yet
        if (!new_sol)
        {
            new_sol = std::make_shared<Solution>(*(old_sv.GetSolution())); // make a copy of the solution
            ne->MakeMove(new_sol, mv);
        }
        return new_sol;
    }
    
    SolutionValue GetSolutionValue() const
    {
        return cs->CreateSolutionValue(this->GetSolution());
    }
    
    using std::vector<std::pair<bool, T>>::size;
    
    MoveValue(const MoveValue<Input, Solution, T, _CostStructure, NeighborhoodExplorer>& m) : cs(m.cs), ne(m.ne), new_sol(m.new_sol), old_sv(m.old_sv), std::vector<std::pair<bool, T>>(m)
    {}
    
protected:
    MoveValue(std::shared_ptr<const _NeighborhoodExplorer> ne, const SolutionValue& sv, const Move& mv, size_t size) : mv(mv), old_sv(sv), cs(sv.cs), ne(ne), std::vector<std::pair<bool, T>>(size, { false, 0 })
    {
        assert(sv.cs && ne);
    }
    
    std::shared_ptr<const CostStructure> cs;
    std::shared_ptr<const _NeighborhoodExplorer> ne;
    Move mv;
    SolutionValue old_sv;
    mutable std::shared_ptr<Solution> new_sol;
};

template <InputT _Input, SolutionT<_Input> _Solution, Number _T>
class AggregatedCostStructure : public std::enable_shared_from_this<AggregatedCostStructure<_Input, _Solution, _T>>
{
public:
    using Input = _Input;
    using Solution = _Solution;
    using T = _T;
    friend class SolutionValue<Input, Solution, T, AggregatedCostStructure<Input, Solution, T>>;
    using SolutionValue = SolutionValue<Input, Solution, T, AggregatedCostStructure>;
protected:
    using SelfClass = AggregatedCostStructure<Input, Solution, T>;
    
public:
    template <CostComponentT<Input, Solution, T> CostComponent>
    void AddCostComponent(std::shared_ptr<CostComponent> cc, bool hard, double weight = 1.0)
    {
        // make a copy of the cost component
        cost_components.emplace_back(std::make_unique<CostComponent>(*cc));
        hard_components.emplace_back(hard);
        weight_components.emplace_back(weight);
    }
    
    SolutionValue CreateSolutionValue(std::shared_ptr<const Solution> sol) const
    {
        return { this->shared_from_this(), sol, cost_components.size() };
    }
    
    T ComputeCost(std::shared_ptr<const Solution> sol, size_t i) const
    {
        return this->cost_components[i]->ComputeCost(sol);
    }
    
    template <SolutionValueT<Input, Solution, T, SelfClass> SV1, SolutionValueT<Input, Solution, T, SelfClass> SV2>
    bool equality(const SV1& sc1, const SV2& sc2) const
    {
        assert(this->cost_components.size() == sc1.size() && this->cost_components.size() == sc2.size());
        T total_cost_1 = this->ComputeAggregatedCost(sc1), total_cost_2 = this->ComputeAggregatedCost(sc2);
        // TODO: consider floating point approximated equality at some point, use SFINAE
        return (total_cost_1 == total_cost_2);
    }
    
    template <SolutionValueT<Input, Solution, T, SelfClass> SV1, SolutionValueT<Input, Solution, T, SelfClass> SV2>
    std::strong_ordering spaceship(const SV1& sc1, const SV2& sc2) const
    {
        assert(this->cost_components.size() == sc1.size() && this->cost_components.size() == sc2.size());
        T total_cost_1 = this->ComputeAggregatedCost(sc1), total_cost_2 = this->ComputeAggregatedCost(sc2);
        // TODO: consider floating point approximated equality at some point, use SFINAE
        if (total_cost_1 < total_cost_2)
            return std::strong_ordering::less;
        else if (total_cost_1 == total_cost_2)
            return std::strong_ordering::equal;
        else
            return std::strong_ordering::greater;
    }
    
    size_t Components() const
    {
        return this->cost_components.size();
    }
    
protected:
    template <SolutionValueT<Input, Solution, T, SelfClass> SV>
    T ComputeAggregatedCost(const SV& sv) const
    {
        T cost_H = 0, cost_S = 0;
        for (size_t i = 0; i < this->cost_components.size(); ++i)
        {
            if (this->hard_components[i])
                cost_H += this->weight_components[i] * sv[i];
            else
                cost_S += this->weight_components[i] * sv[i];
        }
        return this->HARD_WEIGHT * cost_H + cost_S;
    }
    
    std::vector<std::unique_ptr<CostComponent<Input, Solution, T>>> cost_components;
    std::vector<bool> hard_components;
    std::vector<double> weight_components;
    T HARD_WEIGHT = 1000;
};

template <InputT _Input, SolutionT<_Input> _Solution, Number _T>
class MultiObjectiveCostStructure : public std::enable_shared_from_this<MultiObjectiveCostStructure<_Input, _Solution, _T>>
{
public:
    using Input = _Input;
    using Solution = _Solution;
    using T = _T;
    friend class SolutionValue<Input, Solution, T, MultiObjectiveCostStructure<Input, Solution, T>>;
    using SolutionValue = SolutionValue<Input, Solution, T, MultiObjectiveCostStructure>;
    using SelfClass = MultiObjectiveCostStructure<_Input, _Solution, _T>;
    
    template <CostComponentT<Input, Solution, T> CostComponent>
    void AddCostComponent(std::shared_ptr<CostComponent> cc)
    {
        // make a copy of the cost component
        cost_components.emplace_back(std::make_unique<CostComponent>(*cc));
    }
    
    SolutionValue CreateSolutionValue(std::shared_ptr<const Solution> sol) const
    {
        return { this->shared_from_this(), sol, cost_components.size() };
    }
    
    T ComputeCost(std::shared_ptr<const Solution> sol, size_t i) const
    {
        return this->cost_components[i]->ComputeCost(sol);
    }
    
    template <SolutionValueT<Input, Solution, T, SelfClass> SV1, SolutionValueT<Input, Solution, T, SelfClass> SV2>
    bool equality(const SV1& sc1, const SV2& sc2) const
    {
        assert(this->cost_components.size() == sc1.size() && this->cost_components.size() == sc2.size());
        
        // TODO: consider floating point approximated equality at some point, use SFINAE
        for (size_t i = 0; i < this->cost_components.size(); ++i)
        {
            if (sc1[i] != sc2[i])
                return false;
        }
        return true;
    }
    
    template <SolutionValueT<Input, Solution, T, SelfClass> SV1, SolutionValueT<Input, Solution, T, SelfClass> SV2>
    std::partial_ordering spaceship(const SV1& sc1, const SV2& sc2) const
    {
        assert(this->cost_components.size() == sc1.size() && this->cost_components.size() == sc2.size());
        size_t sc1_less_than_sc2 = 0, sc1_greater_than_sc2 = 0;
        for (size_t i = 0; i < this->cost_components.size(); ++i)
        {
            if (sc1[i] < sc2[i])
            {
                if (sc1_greater_than_sc2 > 0)
                    return std::partial_ordering::unordered;
                sc1_less_than_sc2++;
            }
            else if (sc1[i] > sc2[i])
            {
                sc1_greater_than_sc2++;
                if (sc1_less_than_sc2 > 0)
                    return std::partial_ordering::unordered;
            }
        }
        if (sc1_less_than_sc2 > 0 && sc1_greater_than_sc2 == 0)
            return std::partial_ordering::less;
        if (sc1_greater_than_sc2 > 0 && sc1_less_than_sc2 == 0)
            return std::partial_ordering::greater;
        if (sc1_less_than_sc2 == 0 && sc1_greater_than_sc2 == 0)
            return std::partial_ordering::equivalent;
        return std::partial_ordering::unordered;
    }
    
    size_t Components() const
    {
        return this->cost_components.size();
    }
    
protected:
    std::vector<std::unique_ptr<CostComponent<Input, Solution, T>>> cost_components;
};
}
