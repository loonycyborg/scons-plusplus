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

#include "python_interface_internal.hpp"

#include <iostream>
#include <boost/range/iterator_range.hpp>
#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

#include "action_wrapper.hpp"

using std::string;

namespace python_interface
{

void PythonAction::execute(const environment::Environment& env) const
{
	try {
		action_obj(env["TARGETS"], env["SOURCES"], env);
	} catch(const error_already_set&) {
		PyErr_Print();
		PyErr_Clear();
		throw std::runtime_error("A python exception was raised when executing action.");
	}
}

std::string PythonAction::to_string(const environment::Environment& env) const
{
	return string(extract<std::string>(action_obj.attr("__name__"))) + "()";
}

object call_action_factory(tuple args, dict kw)
{
	ActionFactory factory = extract<ActionFactory>(args[0]);
	return object(action::Action::pointer(new ActionCaller(factory.actfunc_, factory.strfunc_, factory.convert_, tuple(args.slice(1, _)), kw)));
}

void ActionCaller::execute(const environment::Environment& env) const
{
	object format_exc = import("traceback").attr("format_exc");
	try {
		list args;
		foreach(const object& arg, make_object_iterator_range(args_))
			args.append(convert_(object(env.subst(extract<string>(str(arg))))));

		call_extended(actfunc_, tuple(args), kw_);
	} catch(const error_already_set&) {
		PyErr_Print();
		PyErr_Clear();
		throw std::runtime_error("A python exception was raised when executing action.");
	}
}

std::string ActionCaller::to_string(const environment::Environment& env) const
{
	list args;
	foreach(const object& arg, make_object_iterator_range(args_))
		args.append(convert_(object(env.subst(extract<string>(str(arg))))));

	return extract<std::string>(call_extended(strfunc_, tuple(args), kw_));
}

}
