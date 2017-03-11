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

#include "action_wrapper.hpp"

#include <iostream>
#include <boost/range/iterator_range.hpp>

#include "environment_wrappers.hpp"

using std::string;

namespace sconspp
{
namespace python_interface
{

int PythonAction::execute(const Environment& env) const
{
	ScopedGIL lock;
	try {
		action_obj(
			env["TARGETS"] ? variable_to_python(env["TARGETS"]) : py::none(),
			env["SOURCES"] ? variable_to_python(env["SOURCES"]) : py::none(),
		env);
	} catch(const py::error_already_set&) {
		//throw_python_exc("A python exception was raised when executing action.");
		return -1;
	}
	return 0;
}

std::string PythonAction::to_string(const Environment& env, bool) const
{
	ScopedGIL lock;
	return string(action_obj.attr("__name__").cast<std::string>()) + "()";
}

py::object call_action_factory(ActionFactory& factory, py::args args, py::kwargs kw)
{
	return py::cast(Action::pointer(new ActionCaller(factory.actfunc_, factory.strfunc_, factory.convert_, args, kw)));
}

int ActionCaller::execute(const Environment& env) const
{
	ScopedGIL lock;
	try {
		py::list args;
		for(auto arg : args_)
			args.append(convert_(env.subst(py::str(arg).cast<std::string>())));

		actfunc_(*args, **kw_);
	} catch(const py::error_already_set&) {
		//throw_python_exc("A python exception was raised when executing action.");
		return -1;
	}
	return 0;
}

std::string ActionCaller::to_string(const Environment& env, bool) const
{
	ScopedGIL lock;
	py::list args;
	for(auto arg : args_)
		args.append(convert_(env.subst(py::str(arg).cast<string>())));

	return strfunc_(*args, **kw_).cast<string>();
}

}
}
