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

#ifndef SCONSCRIPT_HPP
#define SCONSCRIPT_HPP

#include "environment.hpp"
#include "python_interface/python_interface_internal.hpp"

namespace sconspp
{
namespace python_interface
{

py::object SConscript(const std::string& script);
py::object SConscript(const Environment&, const std::string& script);

void Export(py::args args);
void Import(py::args args);
void Return(py::args args, py::kwargs kw);

void Default(py::object obj);

}
}

#endif
