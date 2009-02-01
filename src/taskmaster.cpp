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

#include <boost/graph/topological_sort.hpp>
#include <boost/foreach.hpp>
#include <map>
#define foreach BOOST_FOREACH

#include "taskmaster.hpp"
#include "task.hpp"

using std::vector;
using boost::depth_first_visit;
using boost::associative_property_map;

using dependency_graph::Node;

class BuildVisitor : public boost::default_dfs_visitor
{
	std::vector<Node>& node_list_;
	public:
	BuildVisitor(std::vector<Node>& node_list) : node_list_(node_list) {}

	void discover_vertex(Node node, const dependency_graph::Graph& graph) const
	{
		node_list_.push_back(node);
	}
};

namespace
{
	std::ostream& operator<<(std::ostream& os, const dependency_graph::NodeList& node_list)
	{
		os << "(";
		if(node_list.size()) {
			dependency_graph::NodeList::const_iterator iter = node_list.begin();
			os << dependency_graph::graph[*iter++]->name();
			for(;iter != node_list.end(); iter++) {
				os << "," << dependency_graph::graph[*iter]->name();
			}
		}
		os << ")";
		return os;
	}
}

namespace taskmaster
{
	void build(dependency_graph::Node node)
	{
		vector<dependency_graph::Node> nodes;
		std::map<Node, boost::default_color_type> colors;
		associative_property_map<std::map<Node, boost::default_color_type> > color_map(colors);
		depth_first_visit(dependency_graph::graph, node, BuildVisitor(nodes), color_map);

		std::vector<boost::shared_ptr<taskmaster::Task> > tasks;
		std::set<boost::shared_ptr<taskmaster::Task> > added_tasks;
		for(vector<dependency_graph::Node>::const_reverse_iterator iter = nodes.rbegin(); iter != nodes.rend(); iter++)
		{
			boost::shared_ptr<taskmaster::Task> task = dependency_graph::graph[*iter]->task();
			if(task && !added_tasks.count(task)) {
				tasks.push_back(task);
				added_tasks.insert(task);
			}
		}

		const int num_tasks = tasks.size();
		int current_task = 1;
		foreach(boost::shared_ptr<taskmaster::Task> task, tasks) {
			std::cout << "[" << current_task++ << "/" << num_tasks << "] " << task->targets() << " <- " << task->sources() << "\n";
			task->execute();
		}
	}
}
