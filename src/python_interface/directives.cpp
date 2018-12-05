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
#include "python_interface/directives.hpp"

#include "util.hpp"
#include "environment.hpp"
#include "python_interface/action_wrapper.hpp"
#include "python_interface/node_wrapper.hpp"
#include "python_interface/subst.hpp"

namespace sconspp
{
namespace python_interface
{

py::object WhereIs(const std::string& name)
{
	std::string path = where_is(name).native();
	if(path.empty())
		return py::none();
	else
		return py::str(path);
}

/*
class DirectiveWrapper
{
	const char* name_;

	public:
	DirectiveWrapper(const char* name) : name_(name) {}

	py::object operator()(py::tuple args, py::dict kw)
	{
		static py::object directive = import("SCons").attr("Script").attr(name_);
		const Environment& env = extract<const Environment&>(args[0]);
		return call_extended(directive, tuple(subst(env, args.slice(1, _))), kw);
	}
};

py::object subst_directive_args(const char* name)
{
	return raw_function(DirectiveWrapper(name));
}
*/

template<template<class Container> class insert_iterator>
inline void AddFooAction(const py::object& target, const py::object& action)
{
	ActionList actions = make_actions(flatten(action));
	NodeList targets = extract_file_nodes(flatten(target));
	std::set<Task::pointer> tasks;
	for(Node node : targets) {
		Task::pointer task = graph[node]->task();;
		if(task)
			tasks.insert(task);
	}
	for(Task::pointer task : tasks)
		std::copy(actions.begin(), actions.end(), insert_iterator<ActionList>(task->actions()));
}

void AddPreAction(py::object target, py::object action)
{
	AddFooAction<std::front_insert_iterator>(target, action);
}

void AddPostAction(py::object target, py::object action)
{
	AddFooAction<std::back_insert_iterator>(target, action);
}

void Depends(py::object target, py::object dependency)
{
	NodeList
		targets = extract_file_nodes(flatten(target)),
		dependencies = extract_file_nodes(flatten(dependency));
	for(Node t : targets) {
		for(Node d : dependencies) {
			add_edge(t, d, graph);
		}
	}
}

void AlwaysBuild(py::args args)
{
	NodeList nodes = extract_file_nodes(flatten(args));
	for(Node node : nodes)
		graph[node]->always_build();
}

py::object FindFile(const std::string& name, py::object dir_objs)
{
	std::vector<std::string> directories;
	for(auto dir : flatten(dir_objs))
		directories.push_back(dir.cast<std::string>());
	boost::optional<Node> file = find_file(name, directories);
	if(file)
		return py::cast(NodeWrapper(file.get()));
	return py::none();
}

void Precious(py::args args)
{
	NodeList nodes = extract_file_nodes(flatten(args));
	for(Node node : nodes) {
		try {
			properties<FSEntry>(node).precious();
		} catch (const std::bad_cast&) { }
	}
}

}
}
