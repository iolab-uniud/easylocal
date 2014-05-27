#if !defined(_TEST_UTILS_HH_)
#define _TEST_UTILS_HH_

#include <string>
#include <sstream>

// For letting unit tests access also to private and protected members
#define protected public
#define private public

inline std::string stringify(const char* message, const char* file, unsigned int line)
{
	std::ostringstream s;
	s << message << " at " << file << ":" << line;
	
	return s.str();
}

#endif //_TESTUTILS_HH_

