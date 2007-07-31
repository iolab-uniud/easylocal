#ifndef EASYLOCALEXCEPTION_HH_
#define EASYLOCALEXCEPTION_HH_

#include <string>

using namespace std;

/** This is the most general exception that can happen in the
    EasyLocal++ framework.
*/
class EasyLocalException
            : public exception
{
public:
    EasyLocalException(const string& msg);
    EasyLocalException(const string& msg, const string& f, unsigned int line);
    ~EasyLocalException() throw();
    const char* what() const throw();
    const string& toString() const throw();
protected:
    EasyLocalException();
    string message;
    string file;
    unsigned int line;
};

#endif /*EASYLOCALEXCEPTION_HH_*/
