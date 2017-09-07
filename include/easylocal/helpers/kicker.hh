#pragma once

#include "easylocal/helpers/neighborhoodexplorer.hh"

namespace EasyLocal {
  namespace Core {
    template <class State, class Move, class CostStructure = DefaultCostStructure<int>>
    struct Kick : public std::vector<std::pair<EvaluatedMove<Move, CostStructure>, State>>
    {
    public:
      static Kick empty;
    };
    
    template <class State, class Move, class CostStructure>
    Kick<State, Move, CostStructure> Kick<State, Move, CostStructure>::empty;
    
    template <class State, class Move, class CostStructure>
    std::ostream& operator<<(std::ostream& os, const Kick<State, Move, CostStructure>& k)
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
    
    template <class Input, class State, class Move, class CostStructure = DefaultCostStructure<int>>
    class Kicker;
    
    template <class Input, class State, class Move, class CostStructure>
    class FullKickerIterator : public std::iterator<std::input_iterator_tag, Kick<State, Move, CostStructure>>
    {
      friend class Kicker<Input, State, Move, CostStructure>;
    public:
      typedef typename Kicker<Input, State, Move, CostStructure>::MoveRelatedness MoveRelatedness;
      
      FullKickerIterator operator++(int) // postfix
      {
        FullKickerIterator pi = *this;
        if (end)
          throw std::logic_error("Attempting to go after last kick");
        end = !NextKick();
        kick_count++;
        return pi;
      }
      FullKickerIterator& operator++() // prefix
      {
        if (end)
          throw std::logic_error("Attempting to go after last kick");
        end = !NextKick();
        kick_count++;
        return *this;
      }
      const Kick<State, Move, CostStructure>& operator*() const
      {
        return kick;
      }
      Kick<State, Move, CostStructure>& operator*()
      {
        return kick;
      }
      const Kick<State, Move, CostStructure>* operator->() const
      {
        return &kick;
      }
      Kick<State, Move, CostStructure>* operator->()
      {
        return &kick;
      }
      bool operator==(const FullKickerIterator<Input, State, Move, CostStructure>& it2) const
      {
        if (end && it2.end)
          return true;
        return (end == it2.end && length == it2.length && kick_count == it2.kick_count && &start_state == &it2.start_state);
      }
      bool operator!=(const FullKickerIterator<Input, State, Move, CostStructure>& it2)
      {
        if (end && it2.end)
          return false;
        return (end != it2.end || length != it2.length || kick_count != it2.kick_count || &start_state != &it2.start_state);
      }
    protected:
      void FirstKick() throw (EmptyNeighborhood)
      {
        kick.assign(length, std::make_pair(EvaluatedMove<Move, CostStructure>(false), start_state));
        
        int cur = 0;
        bool backtracking = false;
        
        // stop only when a complete kicker has been generated, or throw an @ref EmptyNeighborhood
      loop:
        while (cur < (int)length)
        {
          if (cur == -1)
            throw EmptyNeighborhood();
            
          // reset state before generating each move
          kick[cur].second = cur > 0 ? kick[cur - 1].second : start_state;
          
          if (!backtracking)
            {
              try
                {
                  ne.FirstMove(kick[cur].second, kick[cur].first.move);
                  while (cur > 0 && !RelatedMoves(kick[cur - 1].first.move, kick[cur].first.move))
                    {
                      if (!ne.NextMove(kick[cur].second, kick[cur].first.move))
                        {
                          backtracking = true;
                          cur--;
                          goto loop;
                        }
                    }
                  backtracking = false;
                  ne.MakeMove(kick[cur].second, kick[cur].first.move);
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
                  if (!ne.NextMove(kick[cur].second, kick[cur].first.move))
                    {
                      backtracking = true;
                      cur--;
                      goto loop;
                    }
                }
              while (cur > 0 && !RelatedMoves(kick[cur - 1].first.move, kick[cur].first.move));
              backtracking = false;
              ne.MakeMove(kick[cur].second, kick[cur].first.move);
              cur++;
              goto loop;
            }
        }
      }
      
      bool NextKick()
      {
        // go to last move, then start generating with backtracking
        int cur = length - 1;
        bool backtracking = true;
        
        // stop only when a complete kicker has been generated, or throw an @ref EmptyNeighborhood
      loop:
        while (cur < (int)length)
        {
          if (cur == -1)
            return false;
          
          // reset state before generating each move
          kick[cur].second = cur > 0 ? kick[cur - 1].second : start_state;
          
          if (!backtracking)
          {
            try
            {
              ne.FirstMove(kick[cur].second, kick[cur].first.move);
              while (cur > 0 && !RelatedMoves(kick[cur - 1].first.move, kick[cur].first.move))
              {
                if (!ne.NextMove(kick[cur].second, kick[cur].first.move))
                {
                  backtracking = true;
                  kick[cur].first.is_valid = false;
                  cur--;
                  goto loop;
                }
              }
              backtracking = false;
              ne.MakeMove(kick[cur].second, kick[cur].first.move);
              kick[cur].first.is_valid = false;
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
              if (!ne.NextMove(kick[cur].second, kick[cur].first.move))
              {
                backtracking = true;
                kick[cur].first.is_valid = false;
                cur--;
                goto loop;
              }
            }
            while (cur > 0 && !RelatedMoves(kick[cur - 1].first.move, kick[cur].first.move));
            backtracking = false;
            ne.MakeMove(kick[cur].second, kick[cur].first.move);
            kick[cur].first.is_valid = false;
            cur++;
            goto loop;
          }
        }
        return true;
      }
    protected:
      FullKickerIterator(size_t length, const NeighborhoodExplorer<Input, State, Move, CostStructure>& ne, const State& state, const MoveRelatedness& RelatedMoves, bool end = false)
      : length(length), ne(ne), start_state(state), kick_count(0), end(end), RelatedMoves(RelatedMoves)
      {
        if (end)
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
      const size_t length;
      const NeighborhoodExplorer<Input, State, Move, CostStructure>& ne;
      const State& start_state;
      Kick<State, Move, CostStructure> kick;
      size_t kick_count;
      bool end;
      const MoveRelatedness& RelatedMoves;
    };
    
    
    template <class Input, class State, class Move, class CostStructure>
    class SampleKickerIterator : public std::iterator<std::input_iterator_tag, Kick<State, Move, CostStructure>>
    {
      friend class Kicker<Input, State, Move, CostStructure>;
    public:
      typedef typename Kicker<Input, State, Move, CostStructure>::MoveRelatedness MoveRelatedness;
      
      SampleKickerIterator operator++(int) // postfix
      {
        SampleKickerIterator pi = *this;
        if (end)
          throw std::logic_error("Attempting to go after last kick");
        kick_count++;
        end = kick_count >= samples;
        if (!end)
        {
          RandomKick();
        }
        return pi;
      }
      SampleKickerIterator& operator++() // prefix
      {
        if (end)
          throw std::logic_error("Attempting to go after last kick");
        kick_count++;
        end = kick_count >= samples;
        if (!end)
        {
          RandomKick();
        }
        return *this;
      }
      const Kick<State, Move, CostStructure>& operator*() const
      {
        return kick;
      }
      Kick<State, Move, CostStructure>& operator*()
      {
        return kick;
      }
      const Kick<State, Move, CostStructure>* operator->() const
      {
        return &kick;
      }
      Kick<State, Move, CostStructure>* operator->()
      {
        return &kick;
      }
      bool operator==(const SampleKickerIterator<Input, State, Move, CostStructure>& it2) const
      {
        if (end && it2.end)
          return true;
        return (end == it2.end && length == it2.length && kick_count == it2.kick_count && &start_state == &it2.start_state);
      }
      bool operator!=(const SampleKickerIterator<Input, State, Move, CostStructure>& it2)
      {
        if (end && it2.end)
          return false;
        return (end != it2.end || length != it2.length || kick_count != it2.kick_count || &start_state != &it2.start_state);
      }
    protected:
      void RandomKick()
      {
        kick.assign(length, std::make_pair(EvaluatedMove<Move, CostStructure>(false), start_state));
        std::vector<Move> initial_kick_moves(length, Move());
        std::vector<bool> initial_set(length, false);
        
        int cur = 0;
        bool backtracking = false;
        
        // stop only when a complete kicker has been generated, or throw an @ref EmptyNeighborhood
      loop:
        while (cur < (int)length)
        {
          if (cur == -1)
            throw EmptyNeighborhood();
          
          // reset state before generating each move
          kick[cur].second = cur > 0 ? kick[cur - 1].second : start_state;
          
          if (!backtracking)
          {
            try
            {
              ne.RandomMove(kick[cur].second, kick[cur].first.move);
              kick[cur].first.is_valid = false;
              
              if (!initial_set[cur])
              {
                initial_kick_moves[cur] = kick[cur].first.move;
                initial_set[cur] = true;
              }
              
              while (cur > 0 && !RelatedMoves(kick[cur - 1].first.move, kick[cur].first.move))
              {
                if (!ne.NextMove(kick[cur].second, kick[cur].first.move))
                  ne.FirstMove(kick[cur].second, kick[cur].first.move);
                if (kick[cur].first.move == initial_kick_moves[cur])
                {
                  backtracking = true;
                  cur--;
                  goto loop;
                }
              }
              backtracking = false;
              ne.MakeMove(kick[cur].second, kick[cur].first.move);
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
              if (!ne.NextMove(kick[cur].second, kick[cur].first.move))
                ne.FirstMove(kick[cur].second, kick[cur].first.move);
              if (kick[cur].first.move == initial_kick_moves[cur])
              {
                backtracking = true;
                cur--;
                goto loop;
              }
            }
            while (cur > 0 && !RelatedMoves(kick[cur - 1].first.move, kick[cur].first.move));
            backtracking = false;
            ne.MakeMove(kick[cur].second, kick[cur].first.move);
            kick[cur].first.is_valid = false;
            cur++;
            goto loop;
          }
        }
      }
      
      SampleKickerIterator(size_t length, const NeighborhoodExplorer<Input, State, Move, CostStructure>& ne, const State& state, size_t samples, const MoveRelatedness& RelatedMoves, bool end = false)
      : length(length), ne(ne), start_state(state), kick_count(0), samples(samples), end(end), RelatedMoves(RelatedMoves)
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
      const size_t length;
      const NeighborhoodExplorer<Input, State, Move, CostStructure>& ne;
      const State& start_state;
      Kick<State, Move, CostStructure> kick;
      size_t kick_count, samples;
      bool end;
      const MoveRelatedness& RelatedMoves;
    };
    
    /** A kicker is a special kind of neighborhood explorer, which can generate sequences of moves of arbitrary length. It is used to provide diversification or intensification strategies.
     */
    template <class Input, class State, class Move, class CostStructure>
    class Kicker
    {
    public:
      typedef Move MoveType;
      typedef typename CostStructure::CFtype CFtype;
      typedef CostStructure CostStructureType;

      typedef typename std::function<bool(const Move& m1, const Move& m2)> MoveRelatedness;
      
      /** Constructor.
       @param ne the @ref NeighborhoodExplorer used to generate the @ref Move
       */
      Kicker(NeighborhoodExplorer<Input, State, Move, CostStructure>& ne, const MoveRelatedness& RelatedMoves = AllMovesRelated) : ne(ne), RelatedMoves(RelatedMoves) {}
      
      /** The modality of the @ref Move (warning: not the length of the @ref Move sequences) */
      virtual size_t Modality() const
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
      virtual std::pair<Kick<State, Move, CostStructure>, CostStructure> SelectFirst(size_t length, const State &st) const throw (EmptyNeighborhood)
      {
        for (FullKickerIterator<Input, State, Move, CostStructure> it = begin(length, st); it != end(length, st); ++it)
        {
          CostStructure cost(0, 0, 0, std::vector<CFtype>(CostComponent<Input, State, typename CostStructure::CFtype>::CostComponents(), 0));
          for (int i = 0; i < it->size(); i++)
          {
            if (!(*it)[i].first.is_valid)
            {
              (*it)[i].first.cost = this->ne.DeltaCostFunctionComponents((*it)[i].second, (*it)[i].first.move);
              (*it)[i].first.is_valid = true;
            }
            cost += (*it)[i].first.cost;
          }
          if (cost < 0)
            return std::make_pair(*it, cost);
        }
        return std::make_pair(Kick<State, Move, CostStructure>::empty, CostStructure(std::numeric_limits<CFtype>::infinity(), std::numeric_limits<CFtype>::infinity(), std::numeric_limits<CFtype>::infinity(), std::vector<CFtype>(CostComponent<Input, State, typename CostStructure::CFtype>::CostComponents(), std::numeric_limits<CFtype>::infinity())));
      }
      
      /** Generates the best kick.
       @param st current @ref State
       @param kick the sequence of @ref Move to generate
       @param length length of the kick
       @throws @ref EmptyNeighborhood if no kick can be found
       @return the cost of applying the kick to the @ref State
       */
      virtual std::pair<Kick<State, Move, CostStructure>, CostStructure> SelectBest(size_t length, const State &st) const throw (EmptyNeighborhood)
      {
        Kick<State, Move, CostStructure> best_kick;
        CostStructure best_cost;
        unsigned int number_of_bests = 0;
        for (FullKickerIterator<Input, State, Move, CostStructure> it = begin(length, st); it != end(length, st); ++it)
        {
          CostStructure cost(0, 0, 0, std::vector<CFtype>(CostComponent<Input, State, typename CostStructure::CFtype>::CostComponents(), 0));
          for (int i = 0; i < it->size(); i++)
          {
            if (!(*it)[i].first.is_valid)
            {
              (*it)[i].first.cost = ne.DeltaCostFunctionComponents((*it)[i].second, (*it)[i].first.move);
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
            if (Random::Int(0, number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
              best_kick = *it;
            number_of_bests++;
          }
        }
        return std::make_pair(best_kick, best_cost);
      }
      
      virtual std::pair<Kick<State, Move, CostStructure>, CostStructure> SelectRandom(size_t length, const State &st) const throw (EmptyNeighborhood)
      {
        SampleKickerIterator<Input, State, Move, CostStructure> random_it = sample_begin(length, st, 1);
        CostStructure cost(0, 0, 0, std::vector<CFtype>(CostComponent<Input, State, typename CostStructure::CFtype>::CostComponents(), 0));
        for (int i = 0; i < random_it->size(); i++)
        {
          if (!(*random_it)[i].first.is_valid)
          {
            (*random_it)[i].first.cost = ne.DeltaCostFunctionComponents((*random_it)[i].second, (*random_it)[i].first.move);
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
      virtual void MakeKick(State &st, const Kick<State, Move, CostStructure>& kick) const
      {
        st = kick[kick.size() - 1].second;
      }
      
      
      FullKickerIterator<Input, State, Move, CostStructure> begin(size_t length, const State& st) const
      {
        return FullKickerIterator<Input, State, Move, CostStructure>(length, ne, st, RelatedMoves);
      }
      
      FullKickerIterator<Input, State, Move, CostStructure> end(size_t length, const State& st) const
      {
        return FullKickerIterator<Input, State, Move, CostStructure>(length, ne, st, RelatedMoves, true);
      }
      
      SampleKickerIterator<Input, State, Move, CostStructure> sample_begin(size_t length, const State& st, size_t samples) const
      {
        return SampleKickerIterator<Input, State, Move, CostStructure>(length, ne, st, samples, RelatedMoves);
      }
      
      SampleKickerIterator<Input, State, Move, CostStructure> sample_end(size_t length, const State& st, size_t samples) const
      {
        return SampleKickerIterator<Input, State, Move, CostStructure>(length, ne, st, samples, RelatedMoves, true);
      }
      
    protected:
      
      /** The @ref NeighborhoodExplorer used */
      NeighborhoodExplorer<Input, State, Move, CostStructure>& ne;
      
      
      /** The functor for checking for move relatedness */
      const MoveRelatedness& RelatedMoves;
      
      static MoveRelatedness AllMovesRelated;
    };
    
    template <class Input, class State, class Move, class CostStructure>
    typename Kicker<Input, State, Move, CostStructure>::MoveRelatedness Kicker<Input, State, Move, CostStructure>::AllMovesRelated = [](const Move&, const Move&) { return true; };
  }
}

