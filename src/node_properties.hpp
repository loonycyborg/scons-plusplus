/***************************************************************************
 *   Copyright (C) 2010 by Sergey Popov                                    *
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

#ifndef NODE_PROPERTIES_HPP
#define NODE_PROPERTIES_HPP

#include "dependency_graph.hpp"
#include "task.hpp"
#include "db.hpp"

#include <boost/cast.hpp>

namespace dependency_graph
{

class node_properties
{
	boost::shared_ptr<taskmaster::Task> task_;
	friend class builder::Builder;
	void set_task(taskmaster::Task::pointer task) { task_ = task; }

	protected:
	bool always_build_;

	public:
	node_properties() : always_build_(false) {}
	virtual ~node_properties() {}
	virtual std::string name() const = 0;
	virtual const char* type() const = 0;

	virtual bool unchanged(const NodeList& targets, const db::PersistentNodeData&) const = 0;
	virtual bool needs_rebuild() const { return always_build_; }

	void always_build() { always_build_ = true; }
	taskmaster::Task::pointer task() const { return task_; }

	virtual void record_persistent_data(db::PersistentNodeData&) {}
};

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
	const char* type() const { return "dummy"; }
	bool unchanged(const NodeList& targets, const db::PersistentNodeData&) const { return true; }
};

inline Node add_dummy_node(const std::string& name)
{
	Node node = add_vertex(graph);
	graph[node].reset(new dummy_node(name));
	return node;
}

}

#endif
