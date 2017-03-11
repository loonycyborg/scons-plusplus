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

#ifndef ACTION_WRAPPER_HPP
#define ACTION_WRAPPER_HPP

#include "python_interface_internal.hpp"

#include "action.hpp"

namespace sconspp
{

namespace python_interface
{

class PythonAction : public Action
{
	const py::object action_obj;
	const py::object action_str;
	public:

	PythonAction(py::object callable, py::object action_str_) : action_obj(callable), action_str(action_str_) {}
	int execute(const Environment&) const;
	std::string to_string(const Environment&, bool for_signature = false) const;
};

inline Action::pointer make_action(py::object obj, py::object action_str = py::none())
{
	static py::object action_type = py::module::import("SCons.Action").attr("ActionWrapper");
	if(py::isinstance<py::str>(obj)) {
		if(!action_str.is_none())
			return Action::pointer(new ExecCommand(obj.cast<std::string>(), action_str.cast<std::string>()));
		else
			return Action::pointer(new ExecCommand(obj.cast<std::string>()));
	} else if(py::isinstance(obj, action_type)) {
		return obj.cast<Action::pointer>();
	} else if(PyCallable_Check(obj.ptr())) {
		return Action::pointer(new PythonAction(obj, action_str));
	} else
		throw std::runtime_error("Cannot make an action from object of type '" + py::str(obj.get_type()).cast<std::string>() + "'");
}

inline ActionList make_actions(py::object act, py::object action_str = py::none(), py::list varlist = py::list())
{
	ActionList result;
	for(auto obj : flatten(act))
		result.push_back(make_action(py::reinterpret_borrow<py::object>(obj), action_str));
	return result;
}

class ActionFactory
{
	py::object actfunc_;
	py::object strfunc_;
	py::object convert_;
	friend py::object call_action_factory(ActionFactory&, py::args args, py::kwargs kw);
	public:
	
	ActionFactory(py::object actfunc, py::object strfunc, py::object convert) : actfunc_(actfunc), strfunc_(strfunc), convert_(convert) {}
};

py::object call_action_factory(ActionFactory& factory, py::args args, py::kwargs kw);

class ActionCaller : public Action
{
	py::object actfunc_;
	py::object strfunc_;
	py::object convert_;
	py::tuple args_;
	py::dict kw_;

	public:
	ActionCaller(py::object actfunc, py::object strfunc, py::object convert, py::tuple args, py::dict kw) : actfunc_(actfunc), strfunc_(strfunc), convert_(convert), args_(args), kw_(kw) {}

	int execute(const Environment&) const;
	std::string to_string(const Environment&, bool for_signature = false) const;
};

}

}

#endif
