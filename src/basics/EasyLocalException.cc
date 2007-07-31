#include "EasyLocalException.hh"

#include <sstream>

EasyLocalException::EasyLocalException()
        : message("Anonymous Exception"), file(""), line(0)
{}

EasyLocalException::EasyLocalException(const string& msg)
        : message(msg)
{}

EasyLocalException::EasyLocalException(const string& msg, const string& f, unsigned int l)
        : message(msg), file(f), line(l)
{
    ostringstream tmp;
    tmp << message << " [" << file << ':' << line << ']';
    message = tmp.str();
}

EasyLocalException::~EasyLocalException() throw()
{}

const char* EasyLocalException::what() const throw()
{
    return message.c_str();
}

const string& EasyLocalException::toString() const throw()
{
    return message;
}
