/////////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2015, Toro Lee. Use, modification and 
// distribution are subject to the CeCILL-B License
// Author(s): Toro Lee <poy49295@163.com>
// This is the header file that declare the function: ProcessInvoke
//
/////////////////////////////////////////////////////////////////////////////////////

#ifndef COMMON_PROCESSINVOKE_H_
#define COMMON_PROCESSINVOKE_H_

#include <vector>
#include <string>
#include <functional>


void ProcessInvoke(const std::string &binDirectory,
                   const std::string &commandName,
                   const std::vector<std::string> &param,
                   std::function<void(const char*)> cmdCallback);

#endif // COMMON_PROCESSINVOKE_H_
