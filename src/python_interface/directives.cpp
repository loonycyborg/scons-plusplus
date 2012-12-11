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

#include <boost/python/raw_function.hpp>

#include "util.hpp"
#include "environment.hpp"
#include "python_interface/action_wrapper.hpp"
#include "python_interface/node_wrapper.hpp"
#include "python_interface/subst.hpp"

namespace python_interface
{

object WhereIs(const std::string& name)
{
	std::string path = util::where_is(name).native();
	if(path.empty())
		return object();
	else
		return str(path);
}

class DirectiveWrapper
{
	const char* name_;

	public:
	DirectiveWrapper(const char* name) : name_(name) {}

	object operator()(tuple args, dict kw)
	{
		static object directive = import("SCons").attr("Script").attr(name_);
		const environment::Environment& env = extract<const environment::Environment&>(args[0]);
		return call_extended(directive, tuple(subst(env, args.slice(1, _))), kw);
	}
};

object subst_directive_args(const char* name)
{
	return raw_function(DirectiveWrapper(name));
}

template<template<class Container> class insert_iterator>
inline void AddFooAction(const object& target, const object& action)
{
	action::ActionList actions = make_actions(flatten(action));
	NodeList targets = extract_file_nodes(flatten(target));
	std::set<taskmaster::Task::pointer> tasks;
	foreach(Node node, targets) {
		taskmaster::Task::pointer task = graph[node]->task();;
		if(task)
			tasks.insert(task);
	}
	foreach(taskmaster::Task::pointer task, tasks)
		std::copy(actions.begin(), actions.end(), insert_iterator<action::ActionList>(task->actions()));
}

void AddPreAction(object target, object action)
{
	AddFooAction<std::front_insert_iterator>(target, action);
}

void AddPostAction(object target, object action)
{
	AddFooAction<std::back_insert_iterator>(target, action);
}

void Depends(object target, object dependency)
{
	NodeList
		targets = extract_file_nodes(flatten(target)),
		dependencies = extract_file_nodes(flatten(dependency));
	foreach(Node t, targets) {
		foreach(Node d, dependencies) {
			add_edge(t, d, dependency_graph::graph);
		}
	}
}

object AlwaysBuild(tuple args, dict keywords)
{
	NodeList nodes = extract_file_nodes(flatten(args));
	foreach(Node node, nodes)
		dependency_graph::graph[node]->always_build();
	return object();
}

object FindFile(const std::string& name, object dir_objs)
{
	std::vector<std::string> directories;
	foreach(object dir, make_object_iterator_range(flatten(dir_objs)))
		directories.push_back(extract<std::string>(dir)());
	boost::optional<Node> file = dependency_graph::find_file(name, directories);
	if(file)
		return object(NodeWrapper(file.get()));
	return object();
}

}
