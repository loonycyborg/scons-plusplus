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

#ifndef PYTHON_INTERFACE_INTERNAL_HPP
#define PYTHON_INTERFACE_INTERNAL_HPP

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <boost/range/iterator_range.hpp>
#include <boost/filesystem/path.hpp>

namespace py = pybind11;

extern py::dict main_namespace;

namespace sconspp
{
namespace python_interface
{

py::list flatten(py::object obj);
py::dict dictify(py::object obj);

inline py::object partial(py::object func, py::object arg)
{
	py::object partial_func = py::module::import("functools").attr("partial");
	return partial_func(func, arg);
}

inline py::object copy(py::object obj)
{
	py::object copy_func = py::module::import("copy").attr("copy");
	return copy_func(obj);
}

inline py::object deepcopy(py::object obj)
{
	py::object deepcopy_func = py::module::import("SCons").attr("Util").attr("semi_deepcopy");
	return deepcopy_func(obj);
}

inline py::object reversed(py::object iter)
{
	py::object reversed_func = py::module::import("__builtin__").attr("reversed");
	return reversed_func(iter);
}

inline void throw_python_exc(const std::string& msg)
{
	PyObject *exc, *val, *tb;
	PyErr_Fetch(&exc, &val, &tb);
	PyErr_NormalizeException(&exc, &val, &tb);
	py::object traceback_lines = py::module::import("traceback").attr("format_exception")(exc, val, tb);
	std::string traceback = py::str("\n").attr("join")(traceback_lines).cast<std::string>();
	throw std::runtime_error(msg + "\n" + traceback);
}

class ScopedGIL
{
	PyGILState_STATE state_;

	public:
	ScopedGIL() { state_ = PyGILState_Ensure(); }
	~ScopedGIL() { PyGILState_Release(state_); }
};

}
}

#endif
