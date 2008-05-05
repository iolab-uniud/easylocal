/*
 *  Utils.hh
 *  EasyLocalpp
 *
 *  Created by Luca Di Gaspero on 25/04/08.
 *  Copyright 2008 Università degli Studi di Udine. All rights reserved.
 *
 */

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
