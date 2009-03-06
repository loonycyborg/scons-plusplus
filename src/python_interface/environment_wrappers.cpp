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

#include "builder_wrapper.hpp"
#include "environment_wrappers.hpp"
#include "fs_node.hpp"
#include "builder.hpp"
#include "util.hpp"

using boost::polymorphic_cast;

using dependency_graph::add_entry;

namespace python_interface
{

NodeList Command(const environment::Environment& env, object target, object source, object action)
{
	std::cout << "Command(" << extract<char*>(str(target)) << ", " << extract<char*>(str(source)) << ")\n";

	std::deque<action::Action::pointer> actions;
	foreach(const object& act, make_object_iterator_range(make_actions(action)))
		actions.push_back(extract<action::Action::pointer>(act));
	builder::SimpleBuilder simple_builder(actions);
	return call_builder(simple_builder, env, target, source);
}

void Default(const environment::Environment::pointer& env, object obj)
{
	obj = flatten(obj);
	NodeList nodes;
	extract_nodes(obj, back_inserter(nodes), builder::default_factory, *env);
	foreach(Node node, nodes)
		dependency_graph::default_targets.insert(node);
}
NodeWrapper Entry(environment::Environment::pointer, std::string name)
{
	return NodeWrapper(add_entry(transform_node_name(name), boost::logic::indeterminate));
}

NodeWrapper File(environment::Environment::pointer, std::string name)
{
	return NodeWrapper(add_entry(transform_node_name(name), true));
}

NodeWrapper Dir(environment::Environment::pointer, std::string name)
{
	return NodeWrapper(add_entry(transform_node_name(name), false));
}

void Execute(environment::Environment::pointer env, object obj)
{
	object actions = make_actions(obj);
	foreach(const object& action, make_object_iterator_range(actions))
		action::execute(extract<action::Action::pointer>(action), *env);
}

object get_item_from_env(const Environment& env, object obj)
{
	string key = extract<string>(str(obj));
	environment::Variable::const_pointer var = env[key];
	if(!var) {
		PyErr_SetString(PyExc_KeyError, (string("No construction variable named '") + key + "'").c_str());
		throw_error_already_set();
	}

	return variable_to_python(var);
}

object set_item_in_env(Environment& env, object key_obj, object val)
{
	string key = extract<string>(str(key_obj));
	if(env.count(key)) {
		try {
			object& obj = polymorphic_cast<PythonVariable*>(env[key].get())->get();
			obj = val;
		} catch(const std::bad_cast&) {}
	}
	env[key] = extract_variable(val);
	return object();
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

object WhereIs(const Environment&, const string& name)
{
	string path = util::where_is(name).external_file_string();
	if(path.empty())
		return object();
	else
		return str(path);
}

object Detect(const Environment& env, object progs)
{
	progs = flatten(progs);
	foreach(const object& prog, make_object_iterator_range(progs))
	{
		object path = WhereIs(env, extract<string>(prog));
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
