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

#include <boost/cast.hpp>
#include <boost/variant/variant.hpp>

#include "builder_wrapper.hpp"
#include "environment_wrappers.hpp"
#include "python_interface/directives.hpp"
#include "fs_node.hpp"
#include "builder.hpp"
#include "util.hpp"

using boost::polymorphic_cast;

using dependency_graph::add_entry;

namespace python_interface
{

NodeList Command(const environment::Environment& env, object target, object source, object action)
{
	builder::SimpleBuilder simple_builder(make_actions(action));
	return call_builder(simple_builder, env, target, source);
}

void Default(const environment::Environment::pointer& env, object obj)
{
	obj = flatten(obj);
	NodeList nodes = extract_file_nodes(*env, obj);
	foreach(Node node, nodes)
		dependency_graph::default_targets.insert(node);
}
NodeWrapper Entry(environment::Environment::pointer, std::string name)
{
	return NodeWrapper(add_entry(name, boost::logic::indeterminate));
}

NodeWrapper File(environment::Environment::pointer, std::string name)
{
	return NodeWrapper(add_entry(name, true));
}

NodeWrapper Dir(environment::Environment::pointer, std::string name)
{
	return NodeWrapper(add_entry(name, false));
}

NodeList Alias(object aliases, object sources, object actions)
{
	static environment::Environment::pointer env = environment::Environment::create();
	action::ActionList action_list = make_actions(flatten(actions));
	return builder::AliasBuilder(action_list)(*env, extract_nodes(flatten(aliases)), extract_nodes(flatten(sources)));
}

void Execute(environment::Environment::pointer env, object obj)
{
	foreach(const action::Action::pointer& action, make_actions(obj))
		action::execute(action, *env);
}

object get_item_from_env(const Environment& env, const std::string& key)
{
	environment::Variable::const_pointer var = env[key];
	if(!var) {
		PyErr_SetString(PyExc_KeyError, (string("No construction variable named '") + key + "'").c_str());
		throw_error_already_set();
	}

	return variable_to_python(var);
}

void del_item_in_env(Environment& env, const std::string& key)
{
	env[key] = environment::Variable::pointer();
}

void set_item_in_env(Environment& env, const std::string& key, object val)
{
	if(env.count(key)) {
		try {
			object& obj = polymorphic_cast<PythonVariable*>(env[key].get())->get();
			obj = val;
		} catch(const std::bad_cast&) {}
	}
	env[key] = extract_variable(val);
}

void Tool(environment::Environment::pointer env, object obj)
{
	exec("import SCons.Tool", main_namespace, main_namespace);
	object tool = eval("SCons.Tool.Tool", main_namespace, main_namespace);
	tool(obj)(object(env));
}

void Platform(environment::Environment::pointer env, const std::string& name)
{
	exec("import SCons.Platform", main_namespace, main_namespace);
	object platform = eval("SCons.Platform.Platform", main_namespace, main_namespace);
	if(name.empty())
		platform()(object(env));
	else
		platform(name)(object(env));
	(*env)["PLATFORM"] = extract_variable(str(platform));
}

object Replace(const tuple& args, const dict& kw)
{
	environment::Environment& env = extract<environment::Environment&>(args[0]);
	foreach(const object& item, make_object_iterator_range(kw.items())) {
		std::string key = extract<std::string>(item[0]);
		env[key] = extract_variable(item[1]);
	}
	return object();
}

object Detect(const Environment& env, object progs)
{
	progs = flatten(progs);
	foreach(const object& prog, make_object_iterator_range(progs))
	{
		object path = WhereIs(extract<string>(prog));
		if(path)
			return path;
	}
	return object();
}

bool has_key(const Environment& env, const string& key)
{
	return env.count(key);
}

object get_env_attr(object env, const string& name)
{
	try {
		return partial(env["BUILDERS"][name], env);
	} catch(const error_already_set&) {
		if(PyErr_ExceptionMatches(PyExc_KeyError)) {
			PyErr_SetString(PyExc_AttributeError, ("No builder or custom method named '" + name + "'").c_str());
		}
		throw;
	}
}

object AddMethod(object env, object method, object name)
{
	if(!name)
		name = method.attr("__name__");
	env.attr(name) = partial(method, env);
	return object();
}

object SetDefault(tuple args, dict kw)
{
	environment::Environment& env = extract<environment::Environment&>(args[0]);
	foreach(object item, make_object_iterator_range(kw.items())) {
		std::string key = extract<std::string>(item[0]);
		if(!has_key(env, key))
			env[key] = extract_variable(item[1]);
	}
	return object();
}

std::string Dump(const Environment& env)
{
	std::ostringstream os;
	os << "{\n";
	foreach(const Environment::value_type& var, env) {
		os << "'" << var.first << "' : " << var.second->to_string() << "\n";
	}
	os << "}\n";
	return os.str();
}

object make_environment(tuple args, dict kw)
{
	args[0].attr("_true__init__")();
	environment::Environment::pointer env = extract<environment::Environment::pointer>(args[0]);
	(*env)["BUILDERS"] = extract_variable(dict());

	if(!kw.get("_no_platform")) {
		Platform(env, kw.has_key("platform") ? extract<string>(kw["platform"])() : "");
		Tool(env, str("default"));

		(*env)["_concat"] = extract_variable(import("SCons.Defaults").attr("_concat"));
		(*env)["_stripixes"] = extract_variable(import("SCons.Defaults").attr("_stripixes"));
		(*env)["RPATH"] = extract_variable(list());
		(*env)["LIBS"] = extract_variable(list());
		(*env)["__env__"] = extract_variable(args[0]);
	}

	return object();
}

}
