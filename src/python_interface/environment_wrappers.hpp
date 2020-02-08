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

#ifndef ENVIRONMENT_WRAPPERS_HPP
#define ENVIRONMENT_WRAPPERS_HPP

#include <boost/cast.hpp>

#include "environment.hpp"
#include "dependency_graph.hpp"
#include "alias_node.hpp"

#include "python_interface/node_wrapper.hpp"
#include "python_interface/action_wrapper.hpp"

using std::string;

namespace sconspp
{
namespace python_interface
{

class PythonVariable : public Variable
{
	py::object obj_;
	public:
	PythonVariable(py::object obj) : obj_(obj) {}
	string to_string() const { py::gil_scoped_acquire lock {}; return py::str(obj_).cast<string>(); }
	std::list<std::string> to_string_list() const
	{
		py::gil_scoped_acquire lock {};
		std::list<std::string> result;
		for(auto item : flatten(obj_))
			result.push_back(py::str(item).cast<string>());
		return result;
	}
	py::object get() const { return obj_; }
	py::object& get() { return obj_; }
	pointer clone() const
	{
		py::gil_scoped_acquire lock {};
		py::object result = deepcopy(obj_);
		return std::make_shared<PythonVariable>(result);
	}
};

inline Variable::pointer extract_variable(py::object obj)
{
	/*
	if(is_dict(obj))
		return make_variable(obj);
	try {
		boost::shared_ptr<environment::CompositeVariable> vars(new environment::CompositeVariable);
		foreach(const object& item, make_object_iterator_range(obj))
			vars->push_back(extract_variable(item));
		return vars;
	} catch(const error_already_set&) {
		PyErr_Clear();
	}
	*/

	return std::make_shared<PythonVariable>(obj);
}

inline py::object variable_to_python(Variable::const_pointer var)
{
	try {
		const CompositeVariable* variables = boost::polymorphic_cast<const CompositeVariable*>(var.get());
		py::list result;
		for(const Variable::pointer& item : *variables)
			result.append(variable_to_python(item));
		return result;
	} catch(const std::bad_cast&) {
	}
	try {
		const PythonVariable* variable = boost::polymorphic_cast<const PythonVariable*>(var.get());
		return variable->get();
	} catch(const std::bad_cast&) {
	}
	try {
		const SimpleVariable<Node>* variable = boost::polymorphic_cast<const SimpleVariable<Node>*>(var.get());
		return py::cast(NodeWrapper(variable->get()));
	} catch(const std::bad_cast&) {
	}
	return py::str(var->to_string());
}

NodeList Command(const Environment& env, py::object target, py::object source, py::object action);
void Default(const Environment::pointer& env, py::object obj);

NodeWrapper Entry(Environment::pointer, std::string name);
NodeWrapper File(Environment::pointer, std::string name);
NodeWrapper Dir(Environment::pointer, std::string name);
NodeWrapper Value(Environment::pointer, std::string name);
NodeList Alias(py::object aliases, py::object sources, py::object actions);
void Execute(Environment::pointer, py::object obj);
py::object get_item_from_env(const Environment& env, const std::string& key);
void del_item_in_env(Environment& env, const std::string& key);
void set_item_in_env(Environment& env, const std::string& key, py::object val);
void Tool(Environment::pointer, py::object obj);
void Platform(Environment::pointer, const std::string& name);

enum UpdateType { Append, Prepend };
enum UniqueType { Unique, NonUnique };

inline py::object update_dict(py::object old_val, py::object new_val)
{
	py::dict result = dictify(old_val);
	PyDict_Update(result.ptr(), dictify(new_val).ptr());
	return result;
}

template<UpdateType update, UniqueType unique> inline void update_item(py::list&, const py::object&);

template<> inline void update_item<Append, NonUnique>(py::list& the_list, const py::object& item)
{
	the_list.append(item);
}
template<> inline void update_item<Prepend, NonUnique>(py::list& the_list, const py::object& item)
{
	PyList_Insert(the_list.ptr(), 0, item.ptr());
}
template<> inline void update_item<Append, Unique>(py::list& the_list, const py::object& item)
{
	for(auto i : the_list)
		if(i.is(item))
			return;
	the_list.append(item);
}
template<> inline void update_item<Prepend, Unique>(py::list& the_list, const py::object& item)
{
	for(auto i : the_list)
		if(i.is(item))
			return;
	PyList_Insert(the_list.ptr(), 0, item.ptr());
}

template<UpdateType update, UniqueType unique> py::object update_list(py::object old_val, py::object new_val)
{
	py::list result(flatten(old_val));
	new_val = flatten(new_val);
	if(update == Prepend)
		new_val = py::list(reversed(new_val));
	for(auto item : new_val)
		update_item<update, unique>(result, py::reinterpret_borrow<py::object>(item));
	return result;
}

template<UpdateType update_type, UniqueType unique_type> void Update(Environment& env, py::kwargs kw)
{
	for(auto item : kw) {
		std::string key = py::reinterpret_borrow<py::object>(item.first).cast<std::string>();
		py::object old_val = env[key] ? variable_to_python(env[key]) : py::none();
		py::object new_val {py::reinterpret_borrow<py::object>(item.second)};
		if(py::isinstance<py::dict>(old_val) || py::isinstance<py::dict>(new_val))
			env[key] = extract_variable(update_dict(old_val, new_val));
		else
			env[key] = extract_variable(update_list<update_type, unique_type>(old_val, new_val));
	}
}

void Replace(Environment&, py::kwargs kw);
py::object Detect(const Environment&, py::object progs);
bool has_key(const Environment&, const string& key);
py::object get_item_default(const Environment&, const string& key, py::object);
py::object get_env_attr(py::object, const string& key);
void AddMethod(py::object, py::object, const std::string& name);
void SetDefault(Environment&, py::kwargs);
std::string Dump(const Environment& env);
Environment::pointer make_environment(py::args, py::kwargs);
Environment::pointer default_environment(py::tuple, py::dict);
Environment::pointer default_environment();
py::object DefaultEnvironment(py::tuple args, py::dict kw);

}
}

#endif
