#ifndef OUTPUTMANAGER_HH_
#define OUTPUTMANAGER_HH_

#include "StateManager.hh"
#include <iostream>

/** The Output Manager is responsible for translating between
     elements of the search space and output solutions.  It also
     delivers other output information of the search, and stores and
     retrieves solutions from files.  This is the only helper that
     deals with the @c Output class.  All other helpers work only on
     the @c State class, which represents the elements of the search
     space used by the algorithms.  
     @ingroup Helpers
 */	

template <class Input, class Output, class State, typename CFtype = int>
class OutputManager
{
public:
    void Print(std::ostream& os = std::cout) const;
    /** Transforms the given state in an output object.
    @param st the state to transform 
    @param out the corresponding output object. */
    virtual void OutputState(const State &st, Output& out) const = 0;
    /** Transforms an output object in a state object.
    @param st the resulting state
    @param out the output object to transform */
    virtual void InputState(State &st, const Output& out) const = 0;
    virtual void ReadState(State &st, std::istream &is) const;
    virtual void WriteState(const State &st, std::ostream &os) const;
    virtual void Check() const;
    virtual void PrettyPrintOutput(const Output &st, const std::string& string_name) const
        { std::cout << "Sorry, not implemented yet" << std::endl; }
protected:
    /** Constructs an output manager by providing it a state manager
     and an input object.
     @param sm a pointer to a state manager
     @param in a pointer to an input object */
    OutputManager(const Input& i, StateManager<Input, State,CFtype>& e_sm, std::string e_name)
            :  in(i), sm(e_sm), name(e_name) {}
    virtual ~OutputManager() {}
    const Input& in; /**< A reference to the input manager. */
    StateManager<Input, State,CFtype>& sm; /**< A reference to an attached
    	state manager. */
    const std::string name;
};

/*************************************************************************
 * Implementation
 *************************************************************************/

template <class Input, class Output, class State, typename CFtype>
void OutputManager<Input,Output,State,CFtype>::Print(std::ostream& os) const
{
    os  << "Output Manager: " << name << std::endl;
}

/**
   Reads a state from an input stream.

   @param st the state to be read
   @param is the input stream
*/
template <class Input, class Output, class State, typename CFtype>
void OutputManager<Input,Output,State,CFtype>::ReadState(State &st,
        std::istream &is) const
{
	// FIXME: controllare se ha senso
  Output out(in);
  is >> out;
  InputState(st, out);
}

/**
   Writes a state on an output stream.

   @param st the state to be written,
   @param os the output stream
*/
template <class Input, class Output, class State, typename CFtype>
void OutputManager<Input,Output,State,CFtype>::WriteState(const State &st, std::ostream &os) const
{
	// FIXME: controllare se ha senso
    Output out(in);
    OutputState(st, out);
    os << out;
}

/**
   Checks wether the object state is consistent with all the related
   objects.
*/
template <class Input, class Output, class State, typename CFtype>
void OutputManager<Input,Output,State,CFtype>::Check() const
{}

#endif
