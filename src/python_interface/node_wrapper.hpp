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

inline Node extract_node(object obj)
{
	return extract<NodeWrapper>(obj)().node;
}

inline Node string2node(const std::string& name, const builder::Builder::NodeFactory node_factory, const environment::Environment& env)
{
	return node_factory(env, transform_node_name(name));
}

inline Node target_string2node(const std::string& name, const builder::Builder* builder, const environment::Environment& env)
{
	return builder->target_factory()(env, transform_node_name(builder->adjust_target_name(env, name)));
}

inline Node source_string2node(const std::string& name, const builder::Builder* builder, const environment::Environment& env)
{
	return builder->source_factory()(env, transform_node_name(builder->adjust_source_name(env, name)));
}

template<class OutputIterator, typename String2Node>
inline void extract_node(object obj, OutputIterator iter, String2Node string2node)
{
	try {
		*iter++ = extract_node(obj);
	} catch(const error_already_set&) {
		PyErr_Clear();
		*iter++ = string2node(extract<std::string>(obj)());
	}
}

template<class OutputIterator> inline void extract_nodes(object obj, OutputIterator iter)
{
	foreach(object node, make_object_iterator_range(obj)) {
		*iter++ = extract_node(node);
	}
}

template<class OutputIterator, typename String2Node>
inline void extract_nodes(object obj, OutputIterator iter, String2Node string2node)
{
	foreach(object node, make_object_iterator_range(obj)) {
		extract_node(node, iter, string2node);
	}
}

template<class OutputIterator>
inline void extract_nodes(object obj, OutputIterator iter, builder::Builder::NodeFactory node_factory, const environment::Environment& env)
{
	extract_nodes(obj, iter, boost::bind(string2node, _1, node_factory, env));
}

template<class OutputIterator> inline void extract_target_nodes(object obj, OutputIterator iter, const builder::Builder& builder, const environment::Environment& env)
{
	extract_nodes(obj, iter, boost::bind(target_string2node, _1, &builder, env));
}

template<class OutputIterator> inline void extract_source_nodes(object obj, OutputIterator iter, const builder::Builder& builder, const environment::Environment& env)
{
	extract_nodes(obj, iter, boost::bind(source_string2node, _1, &builder, env));
}

template<class NodeClass> inline NodeClass* get_properties(Node node)
{
	return boost::polymorphic_cast<NodeClass*>(graph[node].get());
}

}

#endif
