#pragma once

#include "helpers/neighborhoodexplorer.hh"

namespace EasyLocal
{
  namespace Core
  {
    template <class State, class Move, class CostStructure>
    struct Kick : public std::vector<std::pair<EvaluatedMove<Move, CostStructure>, State>>
    {
    public:
      static Kick empty;
    };
    
    template <class State, class Move, class CostStructure>
    Kick<State, Move, CostStructure> Kick<State, Move, CostStructure>::empty;
    
    template <class State, class Move, class CostStructure>
    std::ostream &operator<<(std::ostream &os, const Kick<State, Move, CostStructure> &k)
    {
      os << "{";
      for (size_t i = 0; i < k.size(); i++)
      {
        if (i > 0)
          os << ", ";
        os << k[i].first.move;
      }
      os << "}";
      return os;
    }
    
    template <class NeighborhoodExplorer>
    class Kicker;
    
    
    template <class Kicker>
    class KickerIterator : public std::iterator<std::input_iterator_tag, Kick<typename Kicker::State, typename Kicker::Move, typename Kicker::CostStructure>>
    {
    protected:
      typedef typename Kicker::Input Input;
      typedef typename Kicker::State State;
      typedef typename Kicker::Move Move;
      typedef typename Kicker::CostStructure CostStructure;
      typedef typename Kicker::RelatedMovesFunc RelatedMovesFunc;
      typedef typename Kicker::RelatedStateFuncType RelatedStateFuncType;
      typedef typename Kicker::RelatedFuncType RelatedFuncType;
      
      KickerIterator(size_t length, const typename Kicker::NeighborhoodExplorer&ne, const Input& in, const State &state, const RelatedMovesFunc* related_moves, bool end = false)
      : length(length), ne(ne), in(in), start_state(state), kick_count(0), end(end), related_moves(related_moves)
      {}
    public:
      
      const Kick<State, Move, CostStructure> &operator*() const
      {
        return kick;
      }
      
      Kick<State, Move, CostStructure> &operator*()
      {
        return kick;
      }
      
      const Kick<State, Move, CostStructure> *operator->() const
      {
        return &kick;
      }
      
      Kick<State, Move, CostStructure> *operator->()
      {
        return &kick;
      }
      
    protected:
      bool RelatedMoves(const State& state, const Move& mv1, const Move& mv2) const
      {
        if (related_moves != nullptr)
        {
          if (related_moves->first == std::type_index(typeid(RelatedStateFuncType)))
            return boost::any_cast<RelatedStateFuncType>(related_moves->second)(state, mv1, mv2);
          else
          {
            assert(related_moves->first == std::type_index(typeid(RelatedFuncType)));
            return boost::any_cast<RelatedFuncType>(related_moves->second)(mv1, mv2);
          }
        }
        // when no related moves relation is provided, all moves are considered as related
        return true;
      }
      
      const size_t length;
      const typename Kicker::NeighborhoodExplorer &ne;
      const Input& in;
      const State &start_state;
      Kick<State, Move, CostStructure> kick;
      size_t kick_count;
      bool end;
      const RelatedMovesFunc* related_moves;
    };
    
    template <class _Kicker>
    class FullKickerIterator : public KickerIterator<_Kicker>
    {
      friend class Kicker<typename _Kicker::NeighborhoodExplorer>;
    protected:
      typedef _Kicker Kicker;
      typedef typename Kicker::Input Input;
      typedef typename Kicker::State State;
      typedef typename Kicker::Move Move;
      typedef typename Kicker::CostStructure CostStructure;
      typedef typename Kicker::RelatedMovesFunc RelatedMovesFunc;
    public:
      
      FullKickerIterator operator++(int) // postfix
      {
        FullKickerIterator pi = *this;
        if (this->end)
          throw std::logic_error("Attempting to go after last kick");
        this->end = !NextKick();
        this->kick_count++;
        return pi;
      }
      
      FullKickerIterator &operator++() // prefix
      {
        if (this->end)
          throw std::logic_error("Attempting to go after last kick");
        this->end = !NextKick();
        this->kick_count++;
        return *this;
      }
      
      bool operator==(const FullKickerIterator<Kicker> &it2) const
      {
        if (this->end && it2.end)
          return true;
        return (this->end == it2.end && this->length == it2.length && this->kick_count == it2.kick_count && &this->start_state == &it2.start_state);
      }
      bool operator!=(const FullKickerIterator<Kicker> &it2)
      {
        if (this->end && it2.end)
          return false;
        return (this->end != it2.end || this->length != it2.length || this->kick_count != it2.kick_count || &this->start_state != &it2.start_state);
      }
      
    protected:
      void FirstKick()
      {
        this->kick.assign(this->length, std::make_pair(EvaluatedMove<Move, CostStructure>(false), this->start_state));
        
        int cur = 0;
        bool backtracking = false;
        
        // stop only when a complete kicker has been generated, or throw an @ref EmptyNeighborhood
      loop:
        while (cur < (int)this->length)
        {
          if (cur == -1)
            throw EmptyNeighborhood();
          
          // reset state before generating each move
          this->kick[cur].second = cur > 0 ? this->kick[cur - 1].second : this->start_state;
          
          if (!backtracking)
          {
            try
            {
              this->ne.FirstMove(this->in, this->kick[cur].second, this->kick[cur].first.move);
              while (cur > 0 && !this->RelatedMoves(this->kick[cur - 1].second, this->kick[cur - 1].first.move, this->kick[cur].first.move))
              {
                if (!this->ne.NextMove(this->in, this->kick[cur].second, this->kick[cur].first.move))
                {
                  backtracking = true;
                  cur--;
                  goto loop;
                }
              }
              backtracking = false;
              this->ne.MakeMove(this->in, this->kick[cur].second, this->kick[cur].first.move);
              cur++;
              goto loop;
            }
            catch (EmptyNeighborhood e)
            {
              backtracking = true;
              cur--;
              goto loop;
            }
          }
          else // backtracking (we only need to generate NextMoves)
          {
            do
            {
              if (!this->ne.NextMove(this->in, this->kick[cur].second, this->kick[cur].first.move))
              {
                backtracking = true;
                cur--;
                goto loop;
              }
            } while (cur > 0 && !this->RelatedMoves(this->kick[cur - 1].second, this->kick[cur - 1].first.move, this->kick[cur].first.move));
            backtracking = false;
            this->ne.MakeMove(this->in, this->kick[cur].second, this->kick[cur].first.move);
            cur++;
            goto loop;
          }
        }
      }
      
      bool NextKick()
      {
        // go to last move, then start generating with backtracking
        int cur = this->length - 1;
        bool backtracking = true;
        
        // stop only when a complete kicker has been generated, or throw an @ref EmptyNeighborhood
      loop:
        while (cur < (int)this->length)
        {
          if (cur == -1)
            return false;
          
          // reset state before generating each move
          this->kick[cur].second = cur > 0 ? this->kick[cur - 1].second : this->start_state;
          
          if (!backtracking)
          {
            try
            {
              this->ne.FirstMove(this->in, this->kick[cur].second, this->kick[cur].first.move);
              while (cur > 0 && !this->RelatedMoves(this->kick[cur - 1].second, this->kick[cur - 1].first.move, this->kick[cur].first.move))
              {
                if (!this->ne.NextMove(this->in, this->kick[cur].second, this->kick[cur].first.move))
                {
                  backtracking = true;
                  this->kick[cur].first.is_valid = false;
                  cur--;
                  goto loop;
                }
              }
              backtracking = false;
              this->ne.MakeMove(this->in, this->kick[cur].second, this->kick[cur].first.move);
              this->kick[cur].first.is_valid = false;
              cur++;
              goto loop;
            }
            catch (EmptyNeighborhood e)
            {
              backtracking = true;
              cur--;
              goto loop;
            }
          }
          else // backtracking (we only need to generate NextMoves)
          {
            do
            {
              if (!this->ne.NextMove(this->in, this->kick[cur].second, this->kick[cur].first.move))
              {
                backtracking = true;
                this->kick[cur].first.is_valid = false;
                cur--;
                goto loop;
              }
            } while (cur > 0 && !this->RelatedMoves(this->kick[cur - 1].second, this->kick[cur - 1].first.move, this->kick[cur].first.move));
            backtracking = false;
            this->ne.MakeMove(this->in, this->kick[cur].second, this->kick[cur].first.move);
            this->kick[cur].first.is_valid = false;
            cur++;
            goto loop;
          }
        }
        return true;
      }
      
    protected:
      FullKickerIterator(size_t length, const typename Kicker::NeighborhoodExplorer &ne, const Input& in, const State &state, const RelatedMovesFunc* related_moves, bool end = false)
      : KickerIterator<Kicker>(length, ne, in, state, related_moves, end)
      {
        if (this->end)
          return;
        try
        {
          FirstKick();
        }
        catch (EmptyNeighborhood)
        {
          end = true;
        }
      }
    };
    

    template <class _Kicker>
    class SampleKickerIterator : public KickerIterator<_Kicker>
    {
      friend class Kicker<typename _Kicker::NeighborhoodExplorer>;
    protected:
      typedef _Kicker Kicker;
    protected:
      typedef typename Kicker::Input Input;
      typedef typename Kicker::State State;
      typedef typename Kicker::Move Move;
      typedef typename Kicker::CostStructure CostStructure;
      typedef typename Kicker::RelatedMovesFunc RelatedMovesFunc;
    public:
      
      SampleKickerIterator operator++(int) // postfix
      {
        SampleKickerIterator pi = *this;
        if (this->end)
          throw std::logic_error("Attempting to go after last kick");
        this->kick_count++;
        this->end = this->kick_count >= samples;
        if (!this->end)
        {
          RandomKick();
        }
        return pi;
      }
      
      SampleKickerIterator &operator++() // prefix
      {
        if (this->end)
          throw std::logic_error("Attempting to go after last kick");
        this->kick_count++;
        this->end = this->kick_count >= samples;
        if (!this->end)
        {
          RandomKick();
        }
        return *this;
      }
      
      
      bool operator==(const SampleKickerIterator<Kicker> &it2) const
      {
        if (this->end && it2.end)
          return true;
        return (this->end == it2.end && this->length == it2.length && this->kick_count == it2.kick_count && &this->start_state == &it2.start_state);
      }
      
      bool operator!=(const SampleKickerIterator<Kicker> &it2)
      {
        if (this->end && it2.end)
          return false;
        return (this->end != it2.end || this->length != it2.length || this->kick_count != it2.kick_count || &this->start_state != &it2.start_state);
      }
      
    protected:
      void RandomKick()
      {
        this->kick.assign(this->length, std::make_pair(EvaluatedMove<Move, CostStructure>(false), this->start_state));
        std::vector<Move> initial_kick_moves(this->length, Move());
        std::vector<bool> initial_set(this->length, false);
        
        int cur = 0;
        bool backtracking = false;
        
        // stop only when a complete kicker has been generated, or throw an @ref EmptyNeighborhood
      loop:
        while (cur < (int)this->length)
        {
          if (cur == -1)
            throw EmptyNeighborhood();
          
          // reset state before generating each move
          this->kick[cur].second = cur > 0 ? this->kick[cur - 1].second : this->start_state;
          
          if (!backtracking)
          {
            try
            {
              this->ne.RandomMove(this->in, this->kick[cur].second, this->kick[cur].first.move);
              this->kick[cur].first.is_valid = false;
              
              if (!initial_set[cur])
              {
                initial_kick_moves[cur] = this->kick[cur].first.move;
                initial_set[cur] = true;
              }
              
              while (cur > 0 && !this->RelatedMoves(this->kick[cur - 1].second, this->kick[cur - 1].first.move, this->kick[cur].first.move))
              {
                if (!this->ne.NextMove(this->in, this->kick[cur].second, this->kick[cur].first.move))
                  this->ne.FirstMove(this->in, this->kick[cur].second, this->kick[cur].first.move);
                if (this->kick[cur].first.move == initial_kick_moves[cur])
                {
                  backtracking = true;
                  cur--;
                  goto loop;
                }
              }
              backtracking = false;
              this->ne.MakeMove(this->in, this->kick[cur].second, this->kick[cur].first.move);
              cur++;
              goto loop;
            }
            catch (EmptyNeighborhood e)
            {
              backtracking = true;
              cur--;
              goto loop;
            }
          }
          else // backtracking (we only need to generate moves following the first)
          {
            do
            {
              if (!this->ne.NextMove(this->in, this->kick[cur].second, this->kick[cur].first.move))
                this->ne.FirstMove(this->in, this->kick[cur].second, this->kick[cur].first.move);
              if (this->kick[cur].first.move == initial_kick_moves[cur])
              {
                backtracking = true;
                cur--;
                goto loop;
              }
            } while (cur > 0 && !this->RelatedMoves(this->kick[cur - 1].second, this->kick[cur - 1].first.move, this->kick[cur].first.move));
            backtracking = false;
            this->ne.MakeMove(this->in, this->kick[cur].second, this->kick[cur].first.move);
            this->kick[cur].first.is_valid = false;
            cur++;
            goto loop;
          }
        }
      }
      
      SampleKickerIterator(size_t length, const typename Kicker::NeighborhoodExplorer &ne, const Input& in, const State &state, size_t samples, const RelatedMovesFunc* related_func, bool end = false)
      : KickerIterator<Kicker>(length, ne, in, state, related_func, end), samples(samples)
      {
        if (end)
          return;
        try
        {
          RandomKick();
        }
        catch (EmptyNeighborhood)
        {
          end = true;
        }
      }
      
      /*const size_t length;
       const NeighborhoodExplorer<Input, State, Move, CostStructure> &ne;
       const Input& in;
       const State &start_state;
       Kick<State, Move, CostStructure> kick;
       size_t kick_count, samples;
       bool end;
       const MoveRelatedness &RelatedMoves; */
      size_t samples;
    };
    
    /** A kicker is a special kind of neighborhood explorer, which can generate sequences of moves of arbitrary length. It is used to provide diversification or intensification strategies.
     */
    template <class _NeighborhoodExplorer>
    class Kicker
    {
    public:
      typedef _NeighborhoodExplorer NeighborhoodExplorer;
      typedef typename NeighborhoodExplorer::Input Input;
      typedef typename NeighborhoodExplorer::State State;
      typedef typename NeighborhoodExplorer::CostStructure CostStructure;
      typedef typename CostStructure::CFtype CFtype;
      typedef typename NeighborhoodExplorer::Move Move;
      typedef typename std::pair<std::type_index, boost::any> RelatedMovesFunc;
      
      typedef typename std::function<bool(const Move&, const Move&)> RelatedFuncType;
      typedef typename std::function<bool(const State&, const Move&, const Move&)> RelatedStateFuncType;
      
    protected:
      std::unique_ptr<RelatedMovesFunc> related_func;

    public:
      
      /** Constructor.
       @param ne the @ref NeighborhoodExplorer used to generate the @ref Move
       */
      Kicker(StateManager<Input, State, CostStructure>& sm, NeighborhoodExplorer& ne) : sm(sm), ne(ne) {}
      
      void AddRelatedFunction(RelatedFuncType&& r)
      {
        related_func = std::make_unique<RelatedMovesFunc>(std::type_index(typeid(RelatedFuncType)), r);
      }
      
      void AddRelatedFunction(RelatedStateFuncType&& r)
      {
        related_func = std::make_unique<RelatedMovesFunc>(std::type_index(typeid(RelatedStateFuncType)), r);
      }
      
      /** The modality of the @ref Move (warning: not the length of the @ref Move sequences) */
      virtual size_t Modality() const final
      {
        return ne.Modality();
      }
      
      /** Virtual destructor. */
      virtual ~Kicker() {}
      
      /** Generates the first improving kick.
       @param st current @ref State
       @param kick the sequence of @ref Move to generate
       @param length length of the kick
       @throws @ref EmptyNeighborhood if no kick can be found
       @return the cost of applying the kick to the @ref State
       */
      virtual std::pair<Kick<State, Move, CostStructure>, CostStructure> SelectFirst(size_t length, const Input& in, const State &st) const
      {
        for (FullKickerIterator<Kicker<NeighborhoodExplorer>> it = begin(length, in, st); it != end(length, in, st); ++it)
        {
          CostStructure cost(0, 0, 0, std::vector<CFtype>(sm.CostComponents(), 0));
          for (int i = 0; i < it->size(); i++)
          {
            if (!(*it)[i].first.is_valid)
            {
              (*it)[i].first.cost = this->ne.DeltaCostFunctionComponents(in, (*it)[i].second, (*it)[i].first.move);
              (*it)[i].first.is_valid = true;
            }
            cost += (*it)[i].first.cost;
          }
          if (cost < 0)
            return std::make_pair(*it, cost);
        }
        return std::make_pair(Kick<State, Move, CostStructure>::empty, CostStructure(std::numeric_limits<CFtype>::infinity(), std::numeric_limits<CFtype>::infinity(), std::numeric_limits<CFtype>::infinity(), std::vector<CFtype>(sm.CostComponents(), std::numeric_limits<CFtype>::infinity())));
      }
      
      /** Generates the best kick.
       @param st current @ref State
       @param kick the sequence of @ref Move to generate
       @param length length of the kick
       @throws @ref EmptyNeighborhood if no kick can be found
       @return the cost of applying the kick to the @ref State
       */
      virtual std::pair<Kick<State, Move, CostStructure>, CostStructure> SelectBest(size_t length, const Input& in, const State &st) const
      {
        Kick<State, Move, CostStructure> best_kick;
        CostStructure best_cost;
        unsigned int number_of_bests = 0;
        for (FullKickerIterator<Kicker<NeighborhoodExplorer>> it = begin(length, in, st); it != end(length, in, st); ++it)
        {
          CostStructure cost(0, 0, 0, std::vector<CFtype>(sm.CostComponents(), 0));
          for (int i = 0; i < it->size(); i++)
          {
            if (!(*it)[i].first.is_valid)
            {
              (*it)[i].first.cost = ne.DeltaCostFunctionComponents(in, (*it)[i].second, (*it)[i].first.move);
              (*it)[i].first.is_valid = true;
            }
            cost += (*it)[i].first.cost;
          }
          if (number_of_bests == 0)
          {
            best_kick = *it;
            best_cost = cost;
            number_of_bests = 1;
          }
          else if (cost < best_cost)
          {
            best_kick = *it;
            best_cost = cost;
            number_of_bests = 1;
          }
          else if (cost == best_cost)
          {
            if (Random::Uniform<unsigned int>(0, number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
              best_kick = *it;
            number_of_bests++;
          }
        }
        return std::make_pair(best_kick, best_cost);
      }
      
      virtual std::pair<Kick<State, Move, CostStructure>, CostStructure> SelectRandom(size_t length, const Input& in, const State &st) const
      {
        SampleKickerIterator<Kicker<NeighborhoodExplorer>> random_it = sample_begin(length, in, st, 1);
        CostStructure cost(0, 0, 0, std::vector<CFtype>(sm.CostComponents(), 0));
        for (int i = 0; i < random_it->size(); i++)
        {
          if (!(*random_it)[i].first.is_valid)
          {
            (*random_it)[i].first.cost = ne.DeltaCostFunctionComponents(in, (*random_it)[i].second, (*random_it)[i].first.move);
            (*random_it)[i].first.is_valid = true;
          }
          cost += (*random_it)[i].first.cost;
        }
        return std::make_pair(*random_it, cost);
      }
      
      /** Executes a kick on a state.
       @param st the @ref State to modify
       @param kick the sequence of @ref Move to apply
       */
      virtual void MakeKick(const Input& in, State &st, const Kick<State, Move, CostStructure> &kick) const
      {
        st = kick[kick.size() - 1].second;
      }
      
      FullKickerIterator<Kicker<NeighborhoodExplorer>> begin(size_t length, const Input& in, const State &st) const
      {
        return FullKickerIterator<Kicker<NeighborhoodExplorer>>(length, ne, in, st, related_func.get());
      }
      
      FullKickerIterator<Kicker<NeighborhoodExplorer>> end(size_t length, const Input& in, const State &st) const
      {
        return FullKickerIterator<Kicker<NeighborhoodExplorer>>(length, ne, in, st, related_func.get(), true);
      }
      
      SampleKickerIterator<Kicker<NeighborhoodExplorer>> sample_begin(size_t length, const Input& in, const State &st, size_t samples) const
      {
        return SampleKickerIterator<Kicker<NeighborhoodExplorer>>(length, ne, in, st, samples, related_func.get());
      }
      
      SampleKickerIterator<Kicker<NeighborhoodExplorer>> sample_end(size_t length, const Input& in, const State &st, size_t samples) const
      {
        return SampleKickerIterator<Kicker<NeighborhoodExplorer>>(length, ne, in, st, samples, related_func.get(), true);
      }
      
    protected:
      StateManager<Input, State, CostStructure> &sm;
      /** The @ref NeighborhoodExplorer used */
      NeighborhoodExplorer &ne;
      
      /** The functor for checking for move relatedness */
    };
  } // namespace Core
} // namespace EasyLocal
