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

#include <functional>

#include <boost/cast.hpp>
#include <boost/variant/variant.hpp>

#include <pybind11/eval.h>
#include <pybind11/functional.h>

#include "builder_wrapper.hpp"
#include "environment_wrappers.hpp"
#include "python_interface/directives.hpp"
#include "fs_node.hpp"
#include "builder.hpp"
#include "util.hpp"

using boost::polymorphic_cast;

namespace sconspp
{
namespace python_interface
{

NodeList Command(const Environment& env, py::object target, py::object source, py::object action)
{
	SimpleBuilder simple_builder(make_actions(action));
	return call_builder(simple_builder, env, target, source);
}

void Default(const Environment::pointer& env, py::object obj)
{
	obj = flatten(obj);
	NodeList nodes = extract_file_nodes(*env, obj);
	for(Node node : nodes)
		default_targets.insert(node);
}
NodeWrapper Entry(Environment::pointer, std::string name)
{
	return NodeWrapper(add_entry(name, boost::logic::indeterminate));
}

NodeWrapper File(Environment::pointer, std::string name)
{
	return NodeWrapper(add_entry(name, true));
}

NodeWrapper Dir(Environment::pointer, std::string name)
{
	return NodeWrapper(add_entry(name, false));
}

NodeWrapper Value(Environment::pointer, std::string name)
{
	return NodeWrapper(add_dummy_node(name));
}

NodeList Alias(py::object aliases, py::object sources, py::object actions)
{
	ActionList action_list = make_actions(flatten(actions));
	return add_alias(*default_environment(), extract_nodes(flatten(aliases)), extract_nodes(flatten(sources)), action_list);
}

void Execute(Environment::pointer env, py::object obj)
{
	for(const Action::pointer& action : make_actions(obj))
		execute(action, *env);
}

py::object get_item_from_env(const Environment& env, const std::string& key)
{
	Variable::const_pointer var = env[key];
	if(!var) {
		PyErr_SetString(PyExc_KeyError, (string("No construction variable named '") + key + "'").c_str());
		throw py::error_already_set();
	}

	return variable_to_python(var);
}

void del_item_in_env(Environment& env, const std::string& key)
{
	env[key] = Variable::pointer();
}

void set_item_in_env(Environment& env, const std::string& key, py::object val)
{
	if(env.count(key)) {
		try {
			py::object& obj = polymorphic_cast<PythonVariable*>(env[key].get())->get();
			obj = val;
		} catch(const std::bad_cast&) {}
	}
	env[key] = extract_variable(val);
}

void Tool(Environment& env, py::object obj)
{
	py::eval<py::eval_single_statement>("import SCons.Tool", main_namespace, main_namespace);
	py::object tool = py::eval("SCons.Tool.Tool", main_namespace, main_namespace);
	tool(obj)(env);
}

void Platform(Environment& env, const std::string& name)
{
	py::eval<py::eval_single_statement>("import SCons.Platform", main_namespace, main_namespace);
	py::object platform = py::eval("SCons.Platform.Platform", main_namespace, main_namespace);
	if(name.empty())
		platform()(env);
	else
		platform(name)(env);
	env["PLATFORM"] = extract_variable(py::eval("'FOO'"));
}

void Replace(Environment& env, py::kwargs kw)
{
	for(auto item : kw) {
		std::string key = item.first.cast<std::string>();
		env[key] = extract_variable(py::reinterpret_borrow<py::object>(item.second));
	}
}

py::object Detect(const Environment& env, py::object progs)
{
	progs = flatten(progs);
	for(auto prog : progs)
	{
		py::object path = WhereIs(prog.cast<string>());
		if(path)
			return path;
	}
	return py::none();
}

bool has_key(const Environment& env, const string& key)
{
	return env.count(key);
}

py::object get_item_or_none(const Environment& env, const string& key)
{
	return env[key] ? variable_to_python(env[key]) : py::none();
}

py::object get_env_attr(py::object env, const string& name)
{
	try {
		return partial(env["BUILDERS"][name.c_str()], env);
	} catch(const py::error_already_set&) {
		if(PyErr_ExceptionMatches(PyExc_KeyError)) {
			PyErr_SetString(PyExc_AttributeError, ("No builder or custom method named '" + name + "'").c_str());
		}
		throw;
	}
}

void AddMethod(py::object env, py::object method, const std::string& name)
{
	std::string method_name{ name.empty() ? method.attr("__name__").cast<std::string>() : name };
	env.attr(method_name.c_str()) = partial(method, env);
}

void SetDefault(Environment& env, py::kwargs kw)
{
	for(auto item : kw) {
		std::string key = item.first.cast<std::string>();
		if(!has_key(env, key))
			env[key] = extract_variable(py::reinterpret_borrow<py::object>(item.second));
	}
}

std::string Dump(const Environment& env)
{
	std::ostringstream os;
	os << "{\n";
	for(const Environment::value_type& var : env) {
		os << "'" << var.first << "' : " << var.second->to_string() << "\n";
	}
	os << "}\n";
	return os.str();
}

py::object concat(const std::string& prefix, py::object objs, const std::string& suffix, const Environment& env)
{
	objs = flatten(subst(env, objs));
	py::list result;
	for(auto obj : objs)
		result.append(py::str(prefix + expand_python(env, py::reinterpret_borrow<py::object>(obj)) + suffix));
	return result;
}

void make_environment(Environment& env, py::args args, py::kwargs kw)
{
	new (&env) Environment();
	env["BUILDERS"] = extract_variable(py::dict());

	if(!kw.contains("_no_platform")) {
		Platform(env, kw.contains("platform") ? kw["platform"].cast<string>() : "");
		Tool(env, py::str("default"));

		env["_concat"] = extract_variable(py::cast(std::function<decltype(concat)>(concat)));
		env["_defines"] = extract_variable(py::module::import("SCons.Defaults").attr("_defines"));
		env["_stripixes"] = extract_variable(py::module::import("SCons.Defaults").attr("_stripixes"));
		env["RDirs"] = extract_variable(py::eval("lambda x : x"));
		env["_LIBFLAGS"] = extract_variable(py::str("${_concat(LIBLINKPREFIX, LIBS, LIBLINKSUFFIX, __env__)}"));
		env["_LIBDIRFLAGS"] = extract_variable(py::str("$( ${_concat(LIBDIRPREFIX, LIBPATH, LIBDIRSUFFIX, __env__)} $)"));
		env["_CPPINCFLAGS"] = extract_variable(py::str("$( ${_concat(INCPREFIX, CPPPATH, INCSUFFIX, __env__)} $)"));
		env["_CPPDEFFLAGS"] = extract_variable(py::str("${_concat(CPPDEFPREFIX, CPPDEFINES, CPPDEFSUFFIX, __env__)}"));
		env["CPPDEFINES"] = extract_variable(py::list());
		env["CPPPATH"] = extract_variable(py::list());
		env["RPATH"] = extract_variable(py::list());
		env["LIBS"] = extract_variable(py::list());
		env["LIBPATH"] = extract_variable(py::list());
		env["__env__"] = extract_variable(py::cast(env));
	}
}

py::object DefaultEnvironment(py::tuple args, py::dict kw)
{
	return py::cast(default_environment(args, kw));
}

Environment::pointer default_environment()
{
	return default_environment(py::tuple(), py::dict());
}

Environment::pointer default_environment(py::tuple args, py::dict kw)
{
	Environment::pointer default_env
		= py::module::import("SCons.Environment").attr("Environment")(*args, **kw).cast<Environment::pointer>();
	return default_env;
}

}
}
