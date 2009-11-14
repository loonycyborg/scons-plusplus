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

#include <boost/python.hpp>
#include <boost/python/raw_function.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/foreach.hpp>
#include <boost/typeof/typeof.hpp>
#define foreach BOOST_FOREACH

#include "util.hpp"
#include "python_interface/python_interface.hpp"
#include "fs_node.hpp"
#include "builder.hpp"

#include "python_interface/sconscript.hpp"
#include "python_interface/node_wrapper.hpp"
#include "python_interface/builder_wrapper.hpp"
#include "python_interface/environment_wrappers.hpp"
#include "python_interface/directives.hpp"

#include "config.hpp"

using std::string;

using namespace boost::python;

dict main_namespace;

namespace python_interface
{

object flatten(object obj)
{
	list result;

	if(is_none(obj))
		return result;

	if(is_list(obj) || is_tuple(obj)) {
		foreach(const object& item, make_object_iterator_range(obj))
			result.extend(flatten(item));
		return result;
	}

	result.append(obj);
	return result;
}

object split(object obj)
{
	if(is_none(obj))
		return list();
	if(is_list(obj))
		return obj;
	if(is_string(obj)) {
		static object string_split = import("string").attr("split");
		return string_split(obj);
	}
	list result;
	result.append(obj);
	return result;
}

object dictify(object obj)
{
	if(is_dict(obj)) return obj;

	obj = flatten(obj);
	dict result;
	foreach(object item, make_object_iterator_range(obj))
		result[item] = object();
	return result;
}

struct node_list_to_python
{
	static PyObject* convert(const dependency_graph::NodeList& node_list)
	{
		list result;
		foreach(const Node& node, node_list)
			result.append(NodeWrapper(node));
		return incref(result.ptr());
	}
};

struct action_list_to_python
{
	static PyObject* convert(const action::ActionList& action_list)
	{
		list result;
		foreach(const action::Action::pointer& action, action_list)
			result.append(object(action));
		return incref(result.ptr());
	}
};

#define NESTED_MODULE(parent, name) \
handle<PyObject> h(borrowed(Py_InitModule((std::string(parent) + "." + name).c_str(), NULL))); \
scope parent_scope = import(parent); \
scope().attr(name) = h; \
scope s = object(h);

BOOST_PYTHON_MODULE(SCons)
{
	{
	NESTED_MODULE("SCons", "Node")
		class_<NodeWrapper>("Node", init<NodeWrapper>())
			.def("__str__", &NodeWrapper::to_string)
			.def("__repr__", &NodeWrapper::to_string)
			.def_readonly("sources", &NodeWrapper::sources)
			.def_readonly("path", &NodeWrapper::path)
			.def_readonly("abspath", &NodeWrapper::abspath)
		;
	}
	{
	NESTED_MODULE("SCons", "Action")
		def("Action", &make_actions, (arg("action"), arg("strfunction")=object(), arg("varlist")=list()));
		class_<action::Action, action::Action::pointer, boost::noncopyable>("ActionWrapper", no_init)
			.def("__call__", &action::Action::execute)
		;
		class_<ActionFactory>("ActionFactory", init<object, object, object>((arg("actfunc"), arg("strfunc"), arg("convert") = eval("lambda x:x"))))
			.def("__call__", raw_function(&call_action_factory))
		;
	}
	{
	NESTED_MODULE("SCons", "Builder")
		def("Builder", raw_function(&make_builder));
		class_<builder::Builder, builder::Builder::pointer, boost::noncopyable>("BuilderWrapper", no_init)
			.def("__call__", raw_function(&call_builder_interface, 2))
			.def("add_action", &add_action)
			.def("add_emitter", &add_emitter)
			.def("get_suffix", &get_builder_suffix)
			.def("get_prefix", &get_builder_prefix)
			.def("get_src_suffix", &get_builder_src_suffix)
			.add_property("suffix", make_function(&get_builder_suffix))
		;
	}
	object env;
	{
	NESTED_MODULE("SCons", "Environment")
		env = class_<Environment, environment::Environment::pointer>("Environment", no_init)
			.def("_true__init__", make_constructor(&environment::Environment::create))
			.def("__init__", raw_function(&make_environment))
			.def("subst", &Environment::subst)
			.def("Default", &Default)
			.def("Command", &Command)
			.def("Entry", &Entry)
			.def("File", &File)
			.def("Dir", &Dir)
			.def("Execute", &Execute)
			.def("__getitem__", &get_item_from_env)
			.def("__delitem__", &del_item_in_env)
			.def("__setitem__", &set_item_in_env)
			.def("Tool", &Tool)
			.def("Platform", &Platform)
			.def("Append", raw_function(&Update<Append, NonUnique>))
			.def("Prepend", raw_function(&Update<Prepend, NonUnique>))
			.def("AppendUnique", raw_function(&Update<Append, Unique>))
			.def("PrependUnique", raw_function(&Update<Prepend, Unique>))
			.def("Replace", raw_function(&Replace))
			.def("Detect", &Detect)
			.def("has_key", &has_key)
			.def("__getattr__", &get_env_attr)
			.def("AddMethod", &AddMethod, (arg("function"), arg("name") = object()))
			.def("SetDefault", raw_function(&SetDefault))
			.def("Dump", &Dump)
			.def("Clone", &Environment::clone)
			.def("SConscript", (void(*)(const environment::Environment&, const std::string&))SConscript)
		;
	}
	to_python_converter<dependency_graph::NodeList, node_list_to_python>();
	to_python_converter<action::ActionList, action_list_to_python>();

	{
	NESTED_MODULE("SCons", "Script")
		s.attr("ARGUMENTS") = dict();
		s.attr("ARGLIST") = list();
		s.attr("BUILD_TARGETS") = list();
		s.attr("COMMAND_LINE_TARGETS") = list();
		s.attr("DEFAULT_TARGETS") = list();

		s.attr("Action") = import("SCons.Action").attr("Action");
		s.attr("Builder") = import("SCons.Builder").attr("Builder");
		s.attr("Environment") = import("SCons.Environment").attr("Environment");

		def("DefaultEnvironment", raw_function(&DefaultEnvironment));
		def("SConscript", (void(*)(const std::string&))SConscript);
		def("Export", raw_function(&Export));
		def("Import", raw_function(&Import));
		def_directive<BOOST_TYPEOF(WhereIs), WhereIs>(env, "WhereIs", (arg("program")));
		def_directive<BOOST_TYPEOF(Alias), Alias, boost::mpl::set_c<int, 3> >(env, "Alias", (arg("alias"), arg("targets") = object(), arg("action") = object()));
		def_directive<BOOST_TYPEOF(AddPreAction), AddPreAction, boost::mpl::set_c<int, 2> >(env, "AddPreAction", (arg("target"), arg("action")));
		def_directive<BOOST_TYPEOF(AddPostAction), AddPostAction, boost::mpl::set_c<int, 2> >(env, "AddPostAction", (arg("target"), arg("action")));
		def_directive<BOOST_TYPEOF(split), split>(env, "Split", (arg("arg")));
		def_directive<BOOST_TYPEOF(flatten), flatten>(env, "Flatten", (arg("arg")));
		def_directive<BOOST_TYPEOF(Depends), Depends>(env, "Depends", (arg("target"), arg("dependency")));
		def_directive_raw<AlwaysBuild>(env, "AlwaysBuild");
	}
}

	void init_python()
	{
		PyImport_AppendInittab(const_cast<char*>("SCons"), initSCons);
		Py_Initialize();

		main_namespace = dict(import("__main__").attr("__dict__"));

		object sys = import("sys");
		list path(sys.attr("path"));
		path.insert(0, PYTHON_MODULES_PATH);
		path.insert(0, (util::readlink("/proc/self/exe").parent_path() / "python_modules").string());
		sys.attr("path") = path;
		exec("import sconspp_import", main_namespace, main_namespace);

		main_namespace.update(import("SCons.Script").attr("__dict__"));
	}

	void run_script(const std::string& filename, int argc, char** argv)
	{
		argv[0][0] = 0;
		PySys_SetArgv(argc, argv);
		try {
			SConscript(filename);
		}
		catch(error_already_set const &) {
			std::cerr << "scons++: *** Unhandled python exception when parsing SConscript files." << std::endl;
			PyErr_Print();
			PyErr_Clear();
		}
	}

	std::string eval_string(const std::string& expression, const Environment& environment)
	{
		::object env(environment.override());
		::object result;
		result = eval(expression.c_str(), env, env);

		return result ? extract<string>(str(result))() : "";
	}

	std::string expand_variable(const std::string& var, const environment::Environment& env)
	{
		if(!env.count(var))
			return std::string();

		object obj = variable_to_python(env[var]);
		if(is_callable(obj))
			return extract<string>(str(obj(get_item_from_env(env, "SOURCES"), get_item_from_env(env, "TARGETS"), env, false)));

		if(is_list(obj)) {
			std::vector<string> result;
			foreach(const object& item, make_object_iterator_range(obj))
				result.push_back(extract<string>(str(item)));
			return boost::join(result, " ");
		}
		return extract<string>(str(obj));
	}
}
