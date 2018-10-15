#pragma once

#include <iostream>

namespace EasyLocal
{
  
  namespace Core
  {
    
    /** The Output Manager is responsible for translating between elements of the search space and output solutions. It also delivers other output information of the search, and stores and retrieves solutions from files. This is the only helper that deals with the @c Output class. All other helpers work only on the @c State class, which represents the elements of the search space used by the algorithms.
     @ingroup Helpers
     */
    template <class _Input, class _Output, class _State>
    class OutputManager 
    {
    public:
      typedef _Input Input;
      typedef _State State;
      typedef _Output Output;
      
      /** Old-style deprecated method
       @deprecated
       */
      [[deprecated("Input object has been moved outside the CostComponent class")]]
      void OutputState(const State &st, Output &out) const
      {
        throw std::runtime_error("You should update your OutputManager by adding a const Input& reference to the method");
      }
      
      /** Transforms the given state in an output object.
       @param in the input object
       @param st the state to transform
       @param out the corresponding output object.
       */
      virtual void OutputState(const Input& in, const State &st, Output &out) const = 0;
      
      /** Old-style deprecated method
       @deprecated
       */
      [[deprecated("Input object has been moved outside the CostComponent class")]]
      void InputState(State &st, const Output &out) const
      {
        throw std::runtime_error("You should update your OutputManager by adding a const Input& reference to the method");
      }
      
      /** Transforms an output object in a state object.
       @param in the input object
       @param st the resulting state
       @param out the output object to transform
       */
      virtual void InputState(const Input& in, State &st, const Output &out) const = 0;
      
      /** Old-style deprecated method
       @deprecated
       */
      [[deprecated("Input object has been moved outside the CostComponent class")]]
      void ReadState(State &st, std::istream &is) const
      {
        throw std::runtime_error("You should update your OutputManager by adding a const Input& reference to the method");
      }
      
      /** Reads a state from an input stream.
       @param in the input object
       @param st the state to be read
       @param is the input stream
       */
      virtual void ReadState(const Input& in, State &st, std::istream &is) const;
      
      /** Old-style deprecated method
       @deprecated
       */
      [[deprecated("Input object has been moved outside the CostComponent class")]]
      void WriteState(const State &st, std::ostream &os) const
      {
        throw std::runtime_error("You should update your OutputManager by adding a const Input& reference to the method");
      }
      
      /** Writes a state on an output stream.
       @param in the input object
       @param st the state to be written,
       @param os the output stream
       */
      virtual void WriteState(const Input& in, const State &st, std::ostream &os) const;
      
      /** Old-style deprecated method
       @deprecated
       */
      [[deprecated("Input object has been moved outside the CostComponent class")]]
      void PrettyPrintOutput(const State &st, const std::string &file_name) const
      {
        throw std::runtime_error("You should update your OutputManager by adding a const Input& reference to the method");
      }
      
      virtual void PrettyPrintOutput(const Input& in, const State &st, const std::string &file_name) const
      {
        std::cout << "Sorry, not implemented yet" << std::endl;
      }
      
    protected:
      
      /** Constructs an output managert.
       @param name the name of the output manager
       */
      OutputManager(std::string name)
      : name(name)
      {}
      
      /** Old-style constructor
       @deprecated
       */
      [[deprecated("Input object has been moved outside the CostComponent class")]]
      OutputManager(const Input &in, std::string name)
      : name(name)
      {
        throw std::runtime_error("You should update your OutputManager, this constructor cannot be used anymore");
      }
      
      virtual ~OutputManager() {}
      
      /** Name of the output manager. */
      const std::string name;
    };
    
    /** IMPLEMENTATION */
    
    template <class Input, class Output, class State>
    void OutputManager<Input, Output, State>::ReadState(const Input& in, State &st, std::istream &is) const
    {
      Output out(in);
      is >> out;
      InputState(in, st, out);
    }
    
    template <class Input, class Output, class State>
    void OutputManager<Input, Output, State>::WriteState(const Input& in, const State &st, std::ostream &os) const
    {
      Output out(in);
      OutputState(in, st, out);
      os << out;
    }
  } // namespace Core
} // namespace EasyLocal
