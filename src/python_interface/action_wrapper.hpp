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

#include "action.hpp"

using namespace boost::python;

namespace python_interface
{

class PythonAction : public action::Action
{
	const object action_obj;
	const object action_str;
	public:

	PythonAction(object callable, object action_str_) : action_obj(callable), action_str(action_str_) {}
	void execute(const environment::Environment&) const;
	std::string to_string(const environment::Environment&, bool for_signature = false) const;
};

inline action::Action::pointer make_action(object obj, object action_str = object())
{
	static object action_type = import("SCons.Action").attr("ActionWrapper");
	if(is_string(obj)) {
		if(action_str)
			return action::Action::pointer(new action::Command(extract<std::string>(obj), extract<std::string>(action_str)));
		else
			return action::Action::pointer(new action::Command(extract<std::string>(obj)));
	} else if(is_instance(obj, action_type)) {
		return extract<action::Action::pointer>(obj);
	} else if(is_callable(obj)) {
		return action::Action::pointer(new PythonAction(obj, action_str));
	} else
		throw std::runtime_error("Cannot make an action from object of type '" + type(obj) + "'");
}

inline action::ActionList make_actions(object act, object action_str = object(), list varlist = list())
{
	action::ActionList result;
	foreach(const object& obj, make_object_iterator_range(flatten(act)))
		result.push_back(make_action(obj, action_str));
	return result;
}

class ActionFactory
{
	object actfunc_;
	object strfunc_;
	object convert_;
	friend object call_action_factory(tuple args, dict kw);
	public:
	
	ActionFactory(object actfunc, object strfunc, object convert) : actfunc_(actfunc), strfunc_(strfunc), convert_(convert) {}
};

object call_action_factory(tuple args, dict kw);

class ActionCaller : public action::Action
{
	object actfunc_;
	object strfunc_;
	object convert_;
	tuple args_;
	dict kw_;

	public:
	ActionCaller(object actfunc, object strfunc, object convert, tuple args, dict kw) : actfunc_(actfunc), strfunc_(strfunc), convert_(convert), args_(args), kw_(kw) {}

	void execute(const environment::Environment&) const;
	std::string to_string(const environment::Environment&, bool for_signature = false) const;
};

}

#endif
