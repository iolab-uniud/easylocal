// $Id$
// This file is part of EasyLocalpp: a C++ Object-Oriented framework
// aimed at easing the development of Local Search algorithms.
// Copyright (C) 2001--2008 Andrea Schaerf, Luca Di Gaspero. 
//
// EasyLocalpp is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// EasyLocalpp is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with EasyLocalpp. If not, see <http://www.gnu.org/licenses/>.

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
  enum ClockTypes {
#if defined(_MSC_VER)
    MSWindows
#else
    CpuTime, ClockTime, TimeOfDay
#endif
  };
  static void SetClockType(ClockTypes ct) { clock_type = ct; }
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
  static ClockTypes clock_type;
};

#endif /*_CHRONOMETER_HH_*/
