#if !defined(_CHRONOMETER_HH_)
#define _CHRONOMETER_HH_

#if defined(HAVE_CONFIG_H)
#include <config.hh>
#endif
#include <string>

/** A class for measuring CPU execution times: it relies on
      standard C functions / not standard Visual C++ functions 
	  which are encapsulated in this class. */

class Chronometer
{
public:
    Chronometer();
    void Reset();
    void Start();
    void Stop();
    double TotalTime() const;
	static std::string Now();
private:
	class TimeValue
	{
	protected:
	  unsigned long seconds;
	  unsigned long milli_seconds;
	public:
		TimeValue() : seconds(0), milli_seconds(0) {}
	  operator double () const
	  { return seconds + milli_seconds / 1.0E3; }
	  void Reset()
	  { seconds = 0; milli_seconds = 0; }
	  static TimeValue ReadTime();
	};
    bool running;
	TimeValue start, end; 
};

#endif /*_CHRONOMETER_HH_*/
