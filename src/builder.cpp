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

#include <boost/foreach.hpp>
#include <boost/variant/variant.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>

#include "builder.hpp"
#include "task.hpp"
#include "fs_node.hpp"
#include "alias_node.hpp"

#define foreach BOOST_FOREACH

using boost::add_edge;

using taskmaster::Task;
using dependency_graph::graph;
using dependency_graph::Node;
using dependency_graph::NodeList;
using dependency_graph::add_entry_indeterminate;

namespace builder
{

void Builder::create_task(
		const environment::Environment& env,
		const NodeList& targets,
		const NodeList& sources,
		const action::ActionList& actions
		) const
{
	boost::shared_ptr<Task> task(new Task(env, targets, sources, actions));
	foreach(const dependency_graph::Node& node, targets)
		graph[node]->set_task(task);
}

template <Node (*factory)(const std::string&)>
struct make_node : public boost::static_visitor<Node>
{
	Node operator()(const Node& node) const
	{
		return node;
	}
	Node operator()(const std::string& name) const
	{
		return factory(name);
	}
};

template <Node (*factory)(const std::string&)>
inline NodeList make_nodes(const builder::Builder::NodeStringList& list)
{
	NodeList result;
	make_node<factory> visitor;
	transform(list.begin(), list.end(), back_inserter(result), boost::apply_visitor(visitor));
	return result;
}

NodeList Command::operator()(
		const environment::Environment& env,
		const NodeStringList& targets,
		const NodeStringList& sources
		) const
{
	action::ActionList action;
	action.push_back(action::Action::pointer(new action::Command(command_)));
	NodeList 
		target_nodes = make_nodes<add_entry_indeterminate>(targets),
		source_nodes = make_nodes<add_entry_indeterminate>(sources);
	create_task(env, target_nodes, source_nodes, action);
	return target_nodes;
}

NodeList SimpleBuilder::operator()(
		const environment::Environment& env,
		const NodeStringList& targets,
		const NodeStringList& sources
		) const
{
	NodeList
		target_nodes = make_nodes<add_entry_indeterminate>(targets),
		source_nodes = make_nodes<add_entry_indeterminate>(sources);
	create_task(env, target_nodes, source_nodes, actions_);
	return target_nodes;
}

NodeList AliasBuilder::operator()(
		const environment::Environment& env,
		const NodeStringList& targets,
		const NodeStringList& sources
		) const
{
	NodeList
		aliases = make_nodes<dependency_graph::add_alias>(targets),
		source_nodes = make_nodes<add_entry_indeterminate>(sources);
	if(!actions_.empty()) {
		create_task(env, aliases, source_nodes, actions_);
	} else {
		foreach(const dependency_graph::Node& alias, aliases)
			foreach(const dependency_graph::Node& source, source_nodes)
				add_edge(alias, source, graph);
	}
	return aliases;
}

}
