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

#include <boost/variant/variant.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>

#include "builder.hpp"
#include "fs_node.hpp"
#include "alias_node.hpp"

using boost::add_edge;

namespace sconspp
{

NodeList Builder::add_task(
		const Environment& env,
		const NodeList& targets,
		const NodeList& sources,
		const ActionList& actions
		)
{
	Task::pointer task(new Task(env, targets, sources, actions));
	for(const Node& node : targets)
		graph[node]->set_task(task);
	return targets;
}

void Builder::create_task(
        const Environment& env,
		const NodeList& targets,
		const NodeList& sources,
        const ActionList& actions,
		Task::Scanner scanner
		) const
{
	add_task(env, targets, sources, actions);
	properties(targets[0]).task()->set_scanner(scanner);
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
inline NodeList make_nodes(const NodeStringList& list)
{
	NodeList result;
	make_node<factory> visitor;
	transform(list.begin(), list.end(), back_inserter(result), boost::apply_visitor(visitor));
	return result;
}

NodeList make_file_nodes(NodeStringList list)
{
	return make_nodes<add_entry>(list);
}

NodeList add_command(const Environment& env,
				 const NodeStringList& targets,
				 const NodeStringList& sources,
				 const ActionList& actions)
{
	NodeList
		target_nodes = make_nodes<add_entry>(targets),
		source_nodes = make_nodes<add_entry>(sources);
	Builder::add_task(env, target_nodes, source_nodes, actions);
	return target_nodes;
}

NodeList add_alias(const Environment& env,
				 const NodeStringList& targets,
				 const NodeStringList& sources,
				 const ActionList& actions)
{
	NodeList
		target_nodes = make_nodes<add_alias>(targets),
		source_nodes = make_nodes<add_entry>(sources);
	Builder::add_task(env, target_nodes, source_nodes, actions);
	return target_nodes;
}

NodeList SimpleBuilder::operator()(
        const Environment& env,
		const NodeStringList& targets,
		const NodeStringList& sources
		) const
{
	NodeList
		target_nodes = make_nodes<add_entry>(targets),
		source_nodes = make_nodes<add_entry>(sources);
	create_task(env, target_nodes, source_nodes, actions_);
	return target_nodes;
}

}
