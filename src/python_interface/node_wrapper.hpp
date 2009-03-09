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

#ifndef NODE_WRAPPER_HPP
#define NODE_WRAPPER_HPP

#include <boost/python.hpp>
#include <boost/python/stl_iterator.hpp>
#include <boost/cast.hpp>

#include "dependency_graph.hpp"
#include "task.hpp"
#include "builder.hpp"
#include "sconscript.hpp"

using dependency_graph::Node;
using dependency_graph::NodeList;
using dependency_graph::graph;

using namespace boost::python;

namespace python_interface
{

struct NodeWrapper
{
	Node node;
	explicit NodeWrapper(Node node_) : node(node_) {}
	std::string to_string() const { return graph[node]->name(); }

	dependency_graph::NodeList sources() const { return graph[node]->task()->sources(); }
};

inline std::string extract_string_subst(const environment::Environment& env, object obj)
{
	return env.subst(extract<std::string>(obj));
}

NodeList extract_file_nodes(const environment::Environment& env, object obj);

inline Node extract_node(object obj)
{
	return extract<NodeWrapper>(obj)().node;
}

builder::Builder::NodeStringList::value_type extract_node(const environment::Environment& env, object obj);
builder::Builder::NodeStringList extract_nodes(const environment::Environment& env, object obj);

template<class NodeClass> inline NodeClass* get_properties(Node node)
{
	return boost::polymorphic_cast<NodeClass*>(graph[node].get());
}

}

#endif
