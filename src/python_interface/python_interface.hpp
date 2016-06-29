/***************************************************************************
 *   Copyright (C) 2009 by Sergey Popov                                    *
 *   loonycyborg@gmail.com                                                 *
 *                                                                         *
 *  This file is part of SCons++.                                          *
 *                                                                         *
 *  SCons++ is free software; you can redistribute it and/or modify        *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation; either version 3 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  SCons++ is distributed in the hope that it will be useful,             *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#ifndef PYTHON_INTERFACE_HPP
#define PYTHON_INTERFACE_HPP
#include "dependency_graph.hpp"

namespace sconspp
{
class Environment;

namespace python_interface
{
void init_python();
void run_script(const std::string&, int argc, char** argv);
std::string eval_string(const std::string&, const Environment&);
std::string expand_variable(const std::string&, const Environment&);
std::string subst_to_string(const Environment& env, const std::string& input, bool for_signature = false);
}

}

#endif
