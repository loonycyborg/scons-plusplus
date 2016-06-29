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
using namespace boost::python;

namespace sconspp
{
namespace python_interface
{

class PythonVariable : public Variable
{
	object obj_;
	public:
	PythonVariable(object obj) : obj_(obj) {}
	string to_string() const { return extract<string>(str(obj_)); }
	std::list<std::string> to_string_list() const
	{
		std::list<std::string> result;
		foreach(const object& item, make_object_iterator_range(flatten(obj_)))
			result.push_back(extract<string>(str(item)));
		return result;
	}
	object get() const { return obj_; }
	object& get() { return obj_; }
	pointer clone() const
	{
		object result = deepcopy(obj_);
		return pointer(new PythonVariable(result));
	}
};
inline Variable::pointer make_variable(object obj)
{
	return Variable::pointer(new PythonVariable(obj));
}

inline Variable::pointer extract_variable(object obj)
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

	return make_variable(obj);
}

inline object variable_to_python(Variable::const_pointer var)
{
	try {
		const CompositeVariable* variables = boost::polymorphic_cast<const CompositeVariable*>(var.get());
		list result;
		foreach(const Variable::pointer& item, *variables)
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
		return object(NodeWrapper(variable->get()));
	} catch(const std::bad_cast&) {
	}
	return str(var->to_string());
}

NodeList Command(const Environment& env, object target, object source, object action);
void Default(const Environment::pointer& env, object obj);

NodeWrapper Entry(Environment::pointer, std::string name);
NodeWrapper File(Environment::pointer, std::string name);
NodeWrapper Dir(Environment::pointer, std::string name);
NodeWrapper Value(Environment::pointer, std::string name);
NodeList Alias(object aliases, object sources, object actions);
void Execute(Environment::pointer, object obj);
object get_item_from_env(const Environment& env, const std::string& key);
void del_item_in_env(Environment& env, const std::string& key);
void set_item_in_env(Environment& env, const std::string& key, object val);
void Tool(Environment::pointer, object obj);
void Platform(Environment::pointer, const std::string& name);

enum UpdateType { Append, Prepend };
enum UniqueType { Unique, NonUnique };

inline object update_dict(object old_val, object new_val)
{
	dict result = dict(dictify(old_val));
	result.update(dictify(new_val));
	return result;
}

template<UpdateType update, UniqueType unique> inline void update_item(list&, const object&);

template<> inline void update_item<Append, NonUnique>(list& the_list, const object& item)
{
	the_list.append(item);
}
template<> inline void update_item<Prepend, NonUnique>(list& the_list, const object& item)
{
	the_list.insert(0, item);
}
template<> inline void update_item<Append, Unique>(list& the_list, const object& item)
{
	if(!the_list.count(item))
		the_list.append(item);
}
template<> inline void update_item<Prepend, Unique>(list& the_list, const object& item)
{
	if(!the_list.count(item))
		the_list.insert(0, item);
}

template<UpdateType update, UniqueType unique> object update_list(object old_val, object new_val)
{
	list result(flatten(old_val));
	new_val = flatten(new_val);
	if(update == Prepend)
		new_val = reversed(new_val);
	foreach(const object& item, make_object_iterator_range(new_val))
		update_item<update, unique>(result, item);
	return result;
}

template<UpdateType update_type, UniqueType unique_type>object Update(const tuple& args, const dict& kw)
{
	Environment::pointer env_ptr = extract<Environment::pointer>(args[0]);
	Environment& env = *env_ptr;
	foreach(const object& item, make_object_iterator_range(kw.items())) {
		std::string key = extract<std::string>(item[0]);
		object old_val = env[key] ? variable_to_python(env[key]) : object();
		object new_val(item[1]);
		if(is_dict(old_val) || is_dict(new_val))
			env[key] = extract_variable(update_dict(old_val, new_val));
		else
			env[key] = extract_variable(update_list<update_type, unique_type>(old_val, new_val));
	}
	return object();
}

object Replace(const tuple& args, const dict& kw);
object Detect(const Environment&, object progs);
bool has_key(const Environment&, const string& key);
object get_item_or_none(const Environment&, const string& key);
object get_env_attr(object, const string& key);
object AddMethod(object, object, object);
object SetDefault(tuple, dict);
std::string Dump(const Environment& env);
object make_environment(tuple, dict);
Environment::pointer default_environment(tuple, dict);
Environment::pointer default_environment();
object DefaultEnvironment(tuple args, dict kw);

}
}

#endif
