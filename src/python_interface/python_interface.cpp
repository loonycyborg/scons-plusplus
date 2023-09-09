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

#include <pybind11/pybind11.h>
#include <pybind11/embed.h>
#include <pybind11/eval.h>

#include <iostream>
#include <boost/algorithm/string/join.hpp>
#include <boost/typeof/typeof.hpp>

#include "util.hpp"
#include "python_interface/python_interface.hpp"
#include "fs_node.hpp"
#include "scan_cpp.hpp"
#include "taskmaster.hpp"

#include "python_interface/python_interface_internal.hpp"
#include "python_interface/sconscript.hpp"
#include "python_interface/node_wrapper.hpp"
#include "python_interface/builder_wrapper.hpp"
#include "python_interface/environment_wrappers.hpp"
#include "python_interface/directives.hpp"

#include "config.hpp"

using std::string;

py::dict& main_namespace()
{
	static py::dict main_namespace;
	return main_namespace;
}

namespace sconspp
{
namespace python_interface
{

py::list flatten(py::object obj)
{
	py::list result;

	if(obj.is_none())
		return result;

	if(py::isinstance<py::list>(obj) || py::isinstance<py::tuple>(obj)) {
		for(auto item : obj)
			for(auto subitem : flatten(py::reinterpret_borrow<py::object>(item)))
				result.append(subitem);
		return result;
	}

	result.append(obj);
	return result;
}

py::list split(py::object obj)
{
	if(obj.is_none())
		return py::list();
	if(py::isinstance<py::list>(obj))
		return obj;
	if(py::isinstance<py::str>(obj)) {
		return obj.attr("split")();
	}
	py::list result;
	result.append(obj);
	return result;
}

py::dict dictify(py::object obj)
{
	if(py::isinstance<py::dict>(obj)) return obj;

	obj = flatten(obj);
	py::dict result;
	for(auto item : obj)
		result[item] = py::none();
	return result;
}

void AddOption(std::string name, py::kwargs args);
py::object GetOption(std::string name);

using namespace pybind11::literals;

PYBIND11_EMBEDDED_MODULE(SCons, m_scons)
{
	py::module m_node = m_scons.def_submodule("Node");
	py::class_<NodeWrapper>(m_node, "Node")
		.def("__str__", &NodeWrapper::to_string)
		.def("__repr__", &NodeWrapper::to_string)
		.def_property_readonly("sources", &NodeWrapper::sources)
		.def_property_readonly("path", &NodeWrapper::path)
		.def_property_readonly("abspath", &NodeWrapper::abspath)
		.def_property_readonly("name", &NodeWrapper::name)
		.def_property_readonly("dir", &NodeWrapper::dir)
		.def("get_contents", &NodeWrapper::get_contents)
		.def("scanner_key", &NodeWrapper::scanner_key)
		.def("exists", &NodeWrapper::exists)
		;
	py::implicitly_convertible<Node, NodeWrapper>();

	py::module m_value = m_node.def_submodule("Python");
	m_value.def("Value", wrap_add<add_dummy_node>);

	py::module m_fs = m_node.def_submodule("FS");
	m_fs.def("find_file", FindFile, "filename"_a, "paths"_a);
	m_fs.def("Entry", wrap_add<add_entry>);
	m_fs.def("Base", wrap_add<add_entry>);
	m_fs.def("File", wrap_add<add_file>);
	m_fs.def("Dir", wrap_add<add_directory>);
	m_fs.def("_my_normcase", scons_norm_case);

	py::module m_action = m_scons.def_submodule("Action");
	m_action.def("Action", &make_actions, "action"_a, "strfunction"_a=py::str(), "varlist"_a=py::list());
	py::class_<Action, Action::pointer>(m_action, "ActionWrapper")
		.def("__call__", &Action::execute)
		;
	py::class_<ActionFactory>(m_action, "ActionFactory")
		.def(py::init<py::object, py::object, py::object>(),  "actfunc"_a, "strfunc"_a, "convert"_a = py::none())
		.def("__call__", &call_action_factory)
		;

	py::module m_builder = m_scons.def_submodule("Builder");
	def_builder(m_builder);

	py::module m_environment = m_scons.def_submodule("Environment");
	py::class_<Environment, Environment::pointer> env{m_environment, "Environment", py::dynamic_attr()};
	env
		.def(py::init(&make_environment), "platform"_a = py::none(), "tools"_a = py::none(), "toolpath"_a = py::none(), "variables"_a = py::none(), "parse_flags"_a = py::none())
		.def("subst", &Environment::subst, "input"_a, "for_signature"_a = false)
		.def("backtick", backtick, "command"_a)
		.def("Command", &Command)
		.def("Value", &Value) //
		.def("Execute", &Execute)
		.def("__getitem__", &get_item_from_env)
		.def("__delitem__", &del_item_in_env)
		.def("__setitem__", &set_item_in_env)
		.def("__contains__", &has_key)
		.def("Tool", &Tool)
		.def("Platform", &Platform)
		.def("Append", &Update<Append, NonUnique>)
		.def("Prepend", &Update<Prepend, NonUnique>)
		.def("AppendUnique", &Update<Append, Unique>)
		.def("PrependUnique", &Update<Prepend, Unique>)
		.def("Replace", &Replace)
		.def("Detect", &Detect)
		.def("ParseFlags", &ParseFlags)
		.def("MergeFlags", &MergeFlags, "args"_a, "unique"_a = true, "dict"_a = py::none())
		.def("ParseConfig", &ParseConfig, "command"_a, "function"_a = py::none(), "unique"_a = true)
		.def("has_key", &has_key)
		.def("get", &get_item_default, "key"_a, "default"_a = py::none())
		.def("__getattr__", &get_env_attr)
		.def("AddMethod", &AddMethod, "function"_a, "name"_a = ""_s)
		.def("SetDefault", &SetDefault)
		.def("Dump", &Dump)
		.def("Clone", &Environment::clone)
		.def("SConscript", static_cast<py::object(*)(const Environment&, const std::string&)>(SConscript))
		;

	py::module m_script = m_scons.def_submodule("Script");
	m_script.attr("ARGUMENTS") = py::dict();
	m_script.attr("ARGLIST") = py::list();
	m_script.attr("BUILD_TARGETS") = py::list();
	m_script.attr("COMMAND_LINE_TARGETS") = py::list();
	m_script.attr("DEFAULT_TARGETS") = py::list();

	m_script.attr("Action") = m_action.attr("Action");
	m_script.attr("Builder") = m_builder.attr("Builder");
	m_script.attr("Environment") = m_environment.attr("Environment");

	m_script.def("DefaultEnvironment", &DefaultEnvironment);
	m_script.def("SConscript", static_cast<py::object(*)(const std::string&)>(SConscript));
	def_directive(m_script, env, "Entry", &wrap_add<add_entry>);
	def_directive(m_script, env, "File", &wrap_add<add_file>);
	def_directive(m_script, env, "Dir", &wrap_add<add_directory>);
	m_script.def("Export", &Export);
	m_script.def("Import", &Import);
	m_script.def("Return", &Return);
	def_directive(m_script, env, "EnsureSConsVersion", &EnsureSConsVersion, "major"_a, "minor"_a, "revision"_a = 0);
	def_directive(m_script, env, "EnsurePythonVersion", &EnsurePythonVersion, "major"_a, "minor"_a);
	m_script.def("AddOption", &AddOption);
	def_directive(m_script, env, "GetOption", &GetOption, "name"_a);
	def_directive(m_script, env, "Default", &Default, "targets"_a);
	def_directive(m_script, env, "WhereIs", &WhereIs, "program"_a);
	def_directive<boost::mpl::set_c<int, 2>>(m_script, env, "Alias", &Alias, "alias"_a, "targets"_a = py::none(), "action"_a = py::none());
	def_directive<boost::mpl::set_c<int, 1>>(m_script, env, "AddPreAction", &AddPreAction, "target"_a, "action"_a);
	def_directive<boost::mpl::set_c<int, 1>>(m_script, env, "AddPostAction", &AddPostAction, "target"_a, "action"_a);
	def_directive(m_script, env, "Split", &split, "arg"_a);
	def_directive(m_script, env, "Flatten", &flatten, "arg"_a);
	def_directive(m_script, env, "Depends", &Depends, "target"_a, "dependency"_a);
	def_directive(m_script, env, "AlwaysBuild", &AlwaysBuild);
	def_directive(m_script, env, "Glob", &glob, "pattern"_a, "ondisk"_a = true);
	def_directive(m_script, env, "FindFile", &FindFile, "file"_a, "dirs"_a);
	def_directive(m_script, env, "Precious", &Precious);

	py::module m_script_main = m_script.def_submodule("Main");

	py::module m_sconspp_ext = m_scons.def_submodule("SConsppExt");
	py::class_<Task::Scanner>(m_sconspp_ext, "Scanner");
	m_sconspp_ext.attr("CPPScanner") = Task::Scanner(scan_cpp);
	m_sconspp_ext.def("execute", &sconspp::exec, "argv"_a, "capture_output"_a = false);

	py::list path;
	path.append(py::str(PYTHON_MODULES_PATH));
	path.append((readlink("/proc/self/exe").parent_path() / "python_modules").string());
	m_scons.attr("__path__") = path;

	m_scons.attr("__version__") = "3.1.2";
}

	void init_python(std::vector<std::pair<std::string, std::string>> overrides, int argc, char** argv)
	{
		py::initialize_interpreter(true, argc, argv);

		main_namespace() = py::dict(py::module::import("__main__").attr("__dict__"));

		auto m_script { py::module::import("SCons.Script") };

		m_script.attr("ARGLIST") = py::cast(overrides);
		m_script.attr("ARGUMENTS") = py::dict();
		for(auto override : overrides) m_script.attr("ARGUMENTS")[py::str(override.first)] = override.second;

		auto m_variables { py::module::import("SCons.Variables") };
		m_script.def("Variables",
			[m_variables](py::object files, py::object args) -> py::object
				{ return m_variables.attr("Variables")(files, args); },
			"files"_a = py::none(), "args"_a = m_script.attr("ARGUMENTS")
		);
		m_script.attr("BoolVariable") = m_variables.attr("BoolVariable");
		m_script.attr("EnumVariable") = m_variables.attr("EnumVariable");
		m_script.attr("ListVariable") = m_variables.attr("ListVariable");
		m_script.attr("PackageVariable") = m_variables.attr("PackageVariable");
		m_script.attr("PathVariable") = m_variables.attr("PathVariable");

		PyDict_Update(main_namespace().ptr(), m_script.attr("__dict__").ptr());

		sconspp::pre_build_hook = []() -> void* { return new py::gil_scoped_release; };
		sconspp::post_build_hook = [](void* release) { delete reinterpret_cast<py::gil_scoped_release*>(release); };
	}

	void run_script(const std::string& filename, std::vector<std::string> command_line_target_strings)
	{
		try {
			main_namespace()["COMMAND_LINE_TARGETS"] = py::cast(command_line_target_strings);

			py::module::import("SCons.Script.Main").attr("OptionParser") = py::module::import("argparse").attr("ArgumentParser")();

			SConscript(filename);

			for(const auto& target : command_line_target_strings) {
				boost::optional<Node> node = get_alias(target);
				if(!node) node = get_entry(target);

				if(!node) throw std::runtime_error("failed to resolve name '" + target + "' to either alias or filesystem object.");

				command_line_targets.insert(node.get());
			}
		}
		catch(py::error_already_set const & e) {
			std::cerr << "scons++: *** Unhandled python exception when parsing SConscript files." << std::endl;
			throw std::runtime_error(e.what()); // not using pybind's exception type to prevent segfault outside of lock in outer scope when it's destructed
		}
	}

	std::string eval_string(const std::string& expression, const Environment& environment)
	{
		py::object env{py::cast(environment.override())};
		py::object result;
		result = py::eval(expression.c_str(), env, env);

		return result ? py::str(result).cast<std::string>() : "";
	}

	std::string expand_variable(const std::string& var, const Environment& env)
	{
		if(!env.count(var))
			return std::string();

		py::object obj = variable_to_python(env[var]);
		if(PyCallable_Check(obj.ptr()))
			return py::str(obj(get_item_from_env(env, "SOURCES"), get_item_from_env(env, "TARGETS"), env, false)).cast<std::string>();

		if(py::isinstance<py::list>(obj)) {
			std::vector<string> result;
			for(auto item : obj)
				result.push_back(py::str(item).cast<std::string>());
			return boost::join(result, " ");
		}
		return py::str(obj).cast<std::string>();
	}

	std::string subst_to_string(const Environment& env, const std::string& input, bool for_signature)
	{
		py::gil_scoped_acquire acquire;
		return expand_python(env, subst(env, input, for_signature));
	}

	void AddOption(std::string name, py::kwargs args)
	{
		auto type_mapping { std::map<std::string, const char*> { { "string", "str"}, { "int", "int" } } };
		if(args.contains("type")) {
			auto type { args["type"].cast<std::string>() };
			if(type_mapping.count(type) == 0)
				throw std::runtime_error("AddOption: unsupported option type");
			args["type"] = py::module::import("builtins").attr(type_mapping[type]);
		}
		if(args.contains("nargs")) {
			if(args["nargs"].cast<int>() == 1) {
				args["nargs"] = "?";
			} else {
				throw std::runtime_error("AddOption: unsupported value for nargs");
			}
		}
		py::module::import("SCons.Script.Main").attr("OptionParser").attr("add_argument")(name, **args);
	}

	py::object GetOption(std::string name)
	{
		py::object parser { py::module::import("SCons.Script.Main").attr("OptionParser") };
		return py::tuple(parser.attr("parse_known_args")())[0].attr(py::str(name));
	}
}
}
