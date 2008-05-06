#ifndef _TABU_LIST_ITEM_HH_
#define _TABU_LIST_ITEM_HH_

template <class State, class Move, typename CFtype> class TabuListManager;
template <class State, class Move, typename CFtype> class FrequencyTabuListManager;

/** The class for a @c Move item in the Tabu List.
    It is simply a compound data made up of the @c Move itself and the 
    iteration at which the element shall leave the list.
*/
template <class State, class Move, typename CFtype = int>
class TabuListItem
{
  friend class TabuListManager<State, Move,CFtype>;
	friend class FrequencyTabuListManager<State, Move,CFtype>;
public:
	/** Creates a tabu list item constituted by a move
		 and the leaving iteration passed as parameters.
		 @param mv the move to insert into the list
		 @param out the iteration at which the move leaves the list.
		 */
	TabuListItem(Move mv, unsigned long out)
	: elem(mv), out_iter(out)
	{}
	virtual ~TabuListItem() {}
protected:
	Move elem;              /**< The move stored in the list item. */
	unsigned long out_iter; /**< iteration at which the element
	 leaves the list */
};

#endif // define _TABU_LIST_ITEM_HH_
