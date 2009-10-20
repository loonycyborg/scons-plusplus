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

#ifndef DEPENDENCY_GRAPH_HPP
#define DEPENDENCY_GRAPH_HPP

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_utility.hpp>
#include <boost/property_map.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/cast.hpp>

namespace taskmaster
{
class Task;
}

namespace builder
{
class Builder;
}

namespace dependency_graph
{

using boost::adjacency_list;
using boost::vecS;
using boost::directedS;
using boost::graph_traits;
using boost::add_vertex;
using boost::add_edge;

class node_properties;
typedef boost::adjacency_list<vecS, vecS, directedS, boost::shared_ptr<node_properties> > Graph;
typedef graph_traits<Graph>::vertex_descriptor Node;
typedef std::vector<Node> NodeList;

class node_properties
{
	boost::shared_ptr<taskmaster::Task> task_;
	friend class builder::Builder;
	void set_task(boost::shared_ptr<taskmaster::Task> task) { task_ = task; }

	public:
	virtual ~node_properties() {}
	virtual std::string name() const = 0;
	virtual bool unchanged(const NodeList& targets) const = 0;

	boost::shared_ptr<taskmaster::Task> task() const { return task_; }
};

extern Graph graph;
extern std::set<Node> default_targets;

template<class NodeClass> inline NodeClass& properties(Node node)
{
	return *boost::polymorphic_cast<NodeClass*>(graph[node].get());
}

class dummy_node : public node_properties
{
	std::string name_;
	public:
	dummy_node(const std::string& name) : name_(name) {}
	std::string name() const { return name_; }
	bool unchanged(const NodeList& targets) const { return true; }
};

inline Node add_dummy_node(const std::string& name)
{
	Node node = add_vertex(graph);
	graph[node].reset(new dummy_node(name));
	return node;
}

}

#endif
