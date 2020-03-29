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

#include "python_interface.hpp"
#include "builder_wrapper.hpp"
#include "environment_wrappers.hpp"
#include "python_interface/directives.hpp"
#include "fs_node.hpp"
#include "util.hpp"

using boost::polymorphic_cast;

namespace sconspp
{
namespace python_interface
{

NodeList Command(const Environment& env, py::object target, py::object source, py::object action)
{
	NodeList targets { extract_file_nodes(env, flatten(target)) };
	Task::add_task(env, targets, extract_file_nodes(flatten(source)), make_actions(action));
	return targets;
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
	ActionList action_list { make_actions(flatten(actions)) };
	NodeList alias_nodes { extract_alias_nodes(flatten(aliases)) };
	NodeList source_nodes { extract_file_nodes(flatten(sources)) };

	if(!source_nodes.empty()) {
		for(auto alias : alias_nodes) {
			Task::add_task(*default_environment(), NodeList { alias }, extract_file_nodes(flatten(sources)), action_list);
		}
	}

	return alias_nodes;
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

void Tool(Environment::pointer env, py::object obj)
{
	py::eval<py::eval_single_statement>("import SCons.Tool", main_namespace(), main_namespace());
	py::object tool = py::eval("SCons.Tool.Tool", main_namespace(), main_namespace());
	tool(obj)(env);
}

void Platform(Environment::pointer env, const std::string& name)
{
	py::eval<py::eval_single_statement>("import SCons.Platform", main_namespace(), main_namespace());
	py::object platform = py::eval("SCons.Platform.Platform", main_namespace(), main_namespace());
	if(name.empty())
		platform()(env);
	else
		platform(name)(env);
	(*env)["PLATFORM"] = extract_variable(py::eval("'FOO'"));
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

std::unordered_map<std::string, py::object> ParseFlags(const Environment& env, py::args flags)
{
	py::object CLVar = py::module::import("SCons.Util").attr("CLVar");
	std::unordered_map<std::string, py::object> result {
		{ "ASFLAGS", CLVar("") },
		{ "CFLAGS", CLVar("") },
		{ "CCFLAGS", CLVar("") },
		{ "CXXFLAGS", CLVar("") },
		{ "CPPDEFINES", py::list() },
		{ "CPPFLAGS", CLVar("") },
		{ "CPPPATH", py::list() },
		{ "FRAMEWORKPATH", CLVar("") },
		{ "FRAMEWORKS", CLVar("") },
		{ "LIBPATH", py::list() },
		{ "LIBS", py::list() },
		{ "LINKFLAGS", CLVar("") },
		{ "RPATH", py::list() },
	};

	auto command_lines = flatten(flags);
	for(auto command_obj : command_lines) {
		std::string command { command_obj.cast<std::string>() };
		if(command.empty()) continue;
		if(command[0] == '!') {
			command = backtick(env, py::str(std::string(++command.begin(), command.end())));
		}

		const char* append_next_arg_to = nullptr;
		for(auto param_obj : py::module::import("shlex").attr("split")(py::str(command))) {
			std::string param { param_obj.cast<std::string>() };

			if(append_next_arg_to) {
				result[append_next_arg_to].attr("append")(py::str(param));
				append_next_arg_to = nullptr;
				continue;
			}

			static const std::unordered_map<std::string, const char*> letter_params_with_arg {
				{ "-I", "CPPPATH" },
				{ "-L", "LIBPATH" },
				{ "-l", "LIBS" },
				{ "-D", "CPPDEFINES" },
				{ "-F", "FRAMEWORKPATH" },
			};
			if(param.size() >= 2) {
				auto param_iter { letter_params_with_arg.find(param.substr(0, 2)) };
				if(param_iter != letter_params_with_arg.end()) {
					auto arg { param.substr(2) };
					if(!arg.empty()) {
						result[param_iter->second].attr("append")(py::str(arg));
					} else {
						append_next_arg_to = param_iter->second;
					}
					continue;
				}
			}

			static const std::unordered_map<std::string, std::vector<const char*>> word_params {
				{ "-mno-cygwin", { "CCFLAGS", "LINKFLAGS" } },
				{ "-pthread", { "CCFLAGS", "LINKFLAGS" } },
				{ "-openmp", { "CCFLAGS", "LINKFLAGS" } },
				{ "-fmerge-all-constants", { "CCFLAGS", "LINKFLAGS" } },
				{ "-fopenmp", { "CCFLAGS", "LINKFLAGS" } },
				{ "-mwindows", { "LINKFLAGS" } },
			};
			auto param_iter { word_params.find(param) };
			if(param_iter != word_params.end() ) {
				for(auto var : param_iter->second) {
					result[var].attr("append")(py::str(param));
				}
				continue;
			}

			static const std::vector<std::string> rpath_params { "-Wl,-rpath=", "-Wl,-R", "-Wl,-R," };
			bool is_rpath = false;
			for(auto p : rpath_params) {
				if(param.substr(0, p.size()) == p) {
					result["RPATH"].attr("append")(py::str(param.substr(p.size())));
					is_rpath = true;
				}
			}
			if(is_rpath) continue;

			if(param.substr(0, 5) == "-std=") {
				if(param.find("++") != std::string::npos) {
					result["CXXFLAGS"].attr("append")(py::str(param));
				} else {
					result["CFLAGS"].attr("append")(py::str(param));
				}
				continue;
			}

			std::unordered_map<std::string, const char*> stage_params {
				{ "-Wl,", "LINKFLAGS" },
				{ "-Wa,", "CCFLAGS" },
				{ "-Wp,", "CPPFLAGS" },
			};
			auto stage_param_iter { stage_params.find(param) };
			if(stage_param_iter != stage_params.end()) {
				result[stage_params[param.substr(0,4)]].attr("append")(py::str(param));
				continue;
			}

			result["CCFLAGS"].attr("append")(py::str(param));
		}
	}

	return result;
}

void MergeFlags(Environment& env, py::object args, bool unique, py::object dict)
{
	std::unordered_map<std::string, py::object> source_dict;
	if(py::module::import("SCons.Util").attr("is_Dict")(args)) {
		source_dict = args.cast<decltype(source_dict)>();
	} else {
		source_dict = ParseFlags(env, args);
	}

	if(unique) {
		Update<Append, Unique>(env, py::cast(source_dict));
	} else {
		Update<Append, NonUnique>(env, py::cast(source_dict));
	}
}

void ParseConfig(Environment& env, py::object command, py::object function, bool unique)
{
	py::list command_arg;
	command_arg.append(py::str(backtick(env, subst(env, command))));
	if(function.is_none()) {
		return MergeFlags(env, py::cast(ParseFlags(env, command_arg)), unique, py::none());
	} else {
		return MergeFlags(env, function(env, command_arg), unique, py::none());
	}
}

bool has_key(const Environment& env, const string& key)
{
	return env.count(key);
}

py::object get_item_default(const Environment& env, const string& key, py::object default_value)
{
	return env[key] ? variable_to_python(env[key]) : default_value;
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

void setup_scons_task_context(Environment& env, const Task& task)
{
	auto targets = task.targets();
	auto sources = task.sources();
	if(targets.size()) {
		env["TARGETS"] = sconspp::make_variable(targets.begin(), targets.end());
		env["TARGET"] = sconspp::make_variable(targets[0]);
	}
	if(sources.size()) {
		env["SOURCES"] = sconspp::make_variable(sources.begin(), sources.end());
		env["SOURCE"] = sconspp::make_variable(sources[0]);
	}
}

Environment::pointer make_environment(py::args args, py::kwargs kw)
{
	Environment::pointer env_ptr{ new Environment(subst_to_string, setup_scons_task_context) };
	Environment& env = *env_ptr;
	env["BUILDERS"] = extract_variable(py::dict());

	if(!kw.contains("_no_platform")) {
		Platform(env_ptr, kw.contains("platform") ? kw["platform"].cast<string>() : "");
		Tool(env_ptr, py::str("default"));

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
	return env_ptr;
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

std::string backtick(const Environment& env, py::object command)
{
	if(py::isinstance<py::str>(command)) {
		command = split(command);
	}

	std::vector<std::string> command_strings;
	for(auto obj : command) {
		command_strings.push_back(obj.cast<std::string>());
	}

	int status;
	std::vector<std::string> output;
	std::tie(status, output) = exec(command_strings, true);

	if(status != 0)
		throw std::runtime_error(command.cast<std::string>() + " exited with error");

	return output[0];
}

}
}
