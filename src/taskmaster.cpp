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
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/spirit/include/phoenix_bind.hpp>
#include <map>
#define foreach BOOST_FOREACH

#include "taskmaster.hpp"
#include "task.hpp"

using std::vector;
using boost::depth_first_visit;
using boost::associative_property_map;

using boost::multi_index_container;
using boost::multi_index::indexed_by;
using boost::multi_index::tag;
using boost::multi_index::sequenced;
using boost::multi_index::ordered_unique;

using boost::tie;

using boost::phoenix::arg_names::arg1;
using boost::phoenix::push_back;
using boost::phoenix::bind;

using dependency_graph::Node;
using dependency_graph::NodeList;

struct TaskListItem
{
	boost::shared_ptr<taskmaster::Task> task;
	NodeList targets;

	TaskListItem(boost::shared_ptr<taskmaster::Task> task, Node target) : task(task) { targets.push_back(target); }
};

struct sequence_index;
struct task_index;

typedef multi_index_container<
	TaskListItem,
	indexed_by<
		sequenced<tag<sequence_index> >,
		ordered_unique<tag<task_index>, BOOST_MULTI_INDEX_MEMBER(TaskListItem, boost::shared_ptr<taskmaster::Task>, task)>
		>
	> TaskList;

class BuildVisitor : public boost::default_dfs_visitor
{
	TaskList& task_list_;
	std::vector<Node>& build_order_;

	public:
	BuildVisitor(TaskList& task_list, std::vector<Node>& build_order) : task_list_(task_list), build_order_(build_order) {}

	template <typename Edge>
	void back_edge(const Edge&, const dependency_graph::Graph& graph) const { throw boost::not_a_dag(); }
	void finish_vertex(Node node, const dependency_graph::Graph& graph) const
	{
		boost::shared_ptr<taskmaster::Task> task = graph[node]->task();
		if(task) {
			bool inserted;
			TaskList::iterator iter;
			tie(iter, inserted) = task_list_.push_back(TaskListItem(task, node));
			if(!inserted)
				task_list_.modify(iter, push_back(bind(&TaskListItem::targets, arg1), node));
		}
		build_order_.push_back(node);
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
	void build_order(dependency_graph::Node end_goal, TaskList& tasks, std::vector<Node>& output)
	{
		std::map<Node, boost::default_color_type> colors;
		associative_property_map<std::map<Node, boost::default_color_type> > color_map(colors);
		depth_first_visit(dependency_graph::graph, end_goal, BuildVisitor(tasks, output), color_map);
	}

	void build_order(dependency_graph::Node end_goal, std::vector<Node>& output)
	{
		TaskList tasks;
		build_order(end_goal, tasks, output);
	}

	void build(dependency_graph::Node end_goal)
	{
		TaskList tasks;
		std::vector<Node> nodes;
		build_order(end_goal, tasks, nodes);

		const int num_tasks = tasks.size();
		int current_task = 1;
		foreach(const TaskListItem& item, tasks.get<sequence_index>()) {
			std::cout << "[" << current_task++ << "/" << num_tasks << "] " << item.task->targets() << " <- " << item.task->sources() << "\n";

			bool up_to_date = true;
			foreach(Node build_target, item.targets) {
				if(dependency_graph::graph[build_target]->needs_rebuild()) {
					up_to_date = false;
					break;
				}
				typedef boost::graph_traits<dependency_graph::Graph>::edge_descriptor Edge;
				foreach(Edge dependency, out_edges(build_target, dependency_graph::graph)) {
					Node build_source = target(dependency, dependency_graph::graph);
					if(!dependency_graph::graph[build_source]->unchanged(item.targets)) {
						up_to_date = false;
						break;
					}
				}
			}
			if(up_to_date)
				std::cout << "Task is up-to-date.\n";
			else
				item.task->execute();
		}
	}

}
