#if !defined(_OUTPUT_MANAGER_HH_)
#define _OUTPUT_MANAGER_HH_

#include <iostream>

#include "easylocal/utils/printable.hh"

namespace EasyLocal {
  
  namespace Core {
    
    /** The Output Manager is responsible for translating between elements of the search space and output solutions. It also delivers other output information of the search, and stores and retrieves solutions from files. This is the only helper that deals with the @c Output class. All other helpers work only on the @c State class, which represents the elements of the search space used by the algorithms.
     @ingroup Helpers
     */
    template <class Input, class Output, class State, typename CFtype = int>
    class OutputManager : public Printable
    {
    public:
      
      /** @copydoc Printable::Print() */
      virtual void Print(std::ostream& os = std::cout) const;
      
      /** Transforms the given state in an output object.
       @param st the state to transform
       @param out the corresponding output object.
       */
      virtual void OutputState(const State &st, Output& out) const = 0;
      
      /** Transforms an output object in a state object.
       @param st the resulting state
       @param out the output object to transform
       */
      virtual void InputState(State &st, const Output& out) const = 0;
      
      /** Reads a state from an input stream.
       @param st the state to be read
       @param is the input stream
       */
      virtual void ReadState(State &st, std::istream &is) const;
      
      /** Writes a state on an output stream.
       @param st the state to be written,
       @param os the output stream
       */
      virtual void WriteState(const State &st, std::ostream &os) const;
      
      virtual void PrettyPrintOutput(const State &st, const std::string& file_name) const { std::cout << "Sorry, not implemented yet" << std::endl; }
      
    protected:
      
      /** Constructs an output manager by providing it an input object.
       @param in a pointer to an input object
       */
      OutputManager(const Input& i, std::string e_name)
      :  in(i), name(e_name)
      {}
      virtual ~OutputManager() {}
      
      /** A reference to the input. */
      const Input& in;
      
      /** Name of the output manager. */
      const std::string name;
    };
    
    /** IMPLEMENTATION */
    
    template <class Input, class Output, class State, typename CFtype>
    void OutputManager<Input, Output, State, CFtype>::Print(std::ostream& os) const
    {
      os  << "Output Manager: " << name << std::endl;
    }
    
    template <class Input, class Output, class State, typename CFtype>
    void OutputManager<Input, Output, State, CFtype>::ReadState(State &st, std::istream &is) const
    {
      Output out(in);
      is >> out;
      InputState(st, out);
    }
    
    template <class Input, class Output, class State, typename CFtype>
    void OutputManager<Input, Output, State, CFtype>::WriteState(const State &st, std::ostream &os) const
    {
      Output out(in);
      OutputState(st, out);
      os << out;
    }
  }
}

#endif // _OUTPUT_MANAGER_HH_
