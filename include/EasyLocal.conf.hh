#if !defined(_EASYLOCAL_CONF_HH_)
#define _EASYLOCAL_CONF_HH_

/**
 @file EasyLocal.conf.hh
 @brief Basic configuration.
 
 This file contains the configuration of the framework.   
 
 @author Andrea Schaerf (schaerf@uniud.it), Luca Di Gaspero (l.digaspero@uniud.it)
 @version 1.0
 $Revision$
 @date 15 Jun 2001
 @note This version works both with MS Visual C++ and the GNU C++ 
 compiler. Yet, it is extensively tested only with the GNU compiler.
*/

#if defined(_MSC_VER) // for Visual C++ another inclusion file is needed
#include "EasyLocal.conf.vc.hh"
#endif

#endif
