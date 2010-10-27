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
#include <iostream>
#define foreach BOOST_FOREACH

#include "taskmaster.hpp"
#include "task.hpp"
#include "node_properties.hpp"
#include "log.hpp"

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
using dependency_graph::Edge;
using dependency_graph::NodeList;

struct TaskListItem
{
	taskmaster::Task::pointer task;
	NodeList targets;

	TaskListItem(taskmaster::Task::pointer task, Node target) : task(task) { targets.push_back(target); }
};

struct sequence_index;
struct task_index;

typedef multi_index_container<
	TaskListItem,
	indexed_by<
		sequenced<tag<sequence_index> >,
		ordered_unique<tag<task_index>, BOOST_MULTI_INDEX_MEMBER(TaskListItem, taskmaster::Task::pointer, task)>
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
	void discover_vertex(Node node, const dependency_graph::Graph& graph) const
	{
		const std::pair<dependency_graph::Graph::out_edge_iterator, dependency_graph::Graph::out_edge_iterator>&
			iterators = out_edges(node, graph);
		std::vector<Edge> edges(iterators.first, iterators.second);
		foreach(Edge edge, edges) {
			taskmaster::Task::pointer task = graph[source(edge, graph)]->task();
			if(task)
				task->scan(source(edge, graph), target(edge, graph));
		}
	}
	void finish_vertex(Node node, const dependency_graph::Graph& graph) const
	{
		taskmaster::Task::pointer task = graph[node]->task();
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
		db::PersistentData db("sconsppsign.sqlite");

		const int num_tasks = tasks.size();
		int current_task = 1;
		foreach(const TaskListItem& item, tasks.get<sequence_index>()) {
			std::cout << "[" << current_task++ << "/" << num_tasks << "] " << item.task->targets() << " <- " << item.task->sources() << "\n";

			bool up_to_date = true;
			foreach(Node build_target, item.targets) {
				if(dependency_graph::graph[build_target]->needs_rebuild()) {
					up_to_date = false;
				}
				std::set<int> 
					source_ids,
					prev_sources(db[build_target].dependencies());
				foreach(Edge dependency, out_edges(build_target, dependency_graph::graph)) {
					Node build_source = target(dependency, dependency_graph::graph);
					db::PersistentNodeData& source_data = db[build_source];
					source_ids.insert(source_data.id());
					if(!dependency_graph::graph[build_source]->unchanged(item.targets, source_data)) {
						up_to_date = false;
					} else {
						logging::debug(logging::Taskmaster) << dependency_graph::graph[build_source]->name() << " is unchanged\n";
					}
				}
				if(prev_sources.size() == source_ids.size() && std::equal(source_ids.begin(), source_ids.end(), prev_sources.begin()))
					logging::debug(logging::Taskmaster) << "Dependency relations are unchanged\n";
				else
					up_to_date = false;
				if(db[build_target].task_signature() != item.task->signature()) {
					up_to_date = false;
					db[build_target].task_signature() = item.task->signature();
				} else
					logging::debug(logging::Taskmaster) << "Task signature is unchanged\n";
			}
			if(up_to_date)
				logging::debug(logging::Taskmaster) << "Task is up-to-date.\n";
			else
				item.task->execute();
		}
	}

}
