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
#include <boost/thread.hpp>
#include <map>
#include <iostream>

#include "taskmaster.hpp"
#include "task.hpp"
#include "node_properties.hpp"
#include "log.hpp"

#define foreach BOOST_FOREACH

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
	boost::optional<unsigned int> num_jobs;
	bool always_build;

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

	bool is_task_up_to_date(const TaskListItem& item, db::PersistentData& db)
	{
		bool up_to_date = !always_build;
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
				bool unchanged = dependency_graph::graph[build_source]->unchanged(item.targets, source_data);
				if(!unchanged) {
					up_to_date = false;
					logging::debug(logging::Taskmaster) <<
						dependency_graph::graph[build_source]->name() << " has changed\n";
				}
			}
			if(prev_sources.size() != source_ids.size() || !std::equal(source_ids.begin(), source_ids.end(), prev_sources.begin())) {
				logging::debug(logging::Taskmaster) << "Dependency relations have changed\n";
				up_to_date = false;
			}
			if(db[build_target].task_signature() != item.task->signature()) {
				up_to_date = false;
				db[build_target].task_signature() = item.task->signature();
				logging::debug(logging::Taskmaster) << "Task signature has changed\n";
			}
		}
		return up_to_date;
	}

	void serial_build(TaskList& tasks, db::PersistentData& db)
	{
		const int num_tasks = tasks.size();
		int current_task = 1;
		foreach(const TaskListItem& item, tasks.get<sequence_index>()) {
			std::cout << "[" << current_task++ << "/" << num_tasks << "] " << item.task->targets() << " <- " << item.task->sources() << "\n";

			if(is_task_up_to_date(item, db))
				logging::debug(logging::Taskmaster) << "Task is up-to-date.\n";
			else
				item.task->execute();
		}
	}

	class JobServer
	{
		typedef std::map<Node, boost::shared_future<void> > Futures;
		Futures futures;
		typedef std::vector<boost::shared_future<void> > FutureVec;
		
		FutureVec future_vec() const
		{
			FutureVec vec;
			foreach(const Futures::value_type& value, futures)
				vec.push_back(value.second);
			return vec;
		}

		public:
		void schedule(Node node)
		{
			boost::packaged_task<void> ptask(boost::bind(
				&Task::execute, dependency_graph::graph[node]->task().get()
				));
			futures[node] = boost::shared_future<void>(ptask.get_future());
			boost::thread thread(boost::move(ptask));
		}
		NodeList wait_for_any()
		{
			FutureVec vec = future_vec();
			boost::wait_for_any(vec.begin(), vec.end());
			Futures new_futures;
			NodeList result;
			for(Futures::iterator iter = futures.begin(); iter != futures.end(); ++iter) {
				if(iter->second.is_ready()) {
					try {
						iter->second.get();
					} catch(...) {
						wait_for_all();
						throw;
					}
					result.push_back(iter->first);
				} else {
					new_futures.insert(*iter);
				}
			}
			std::swap(futures, new_futures);
			return result;
		}
		void wait_for_all()
		{
			if(futures.size() > 0) {
				logging::warning(logging::Taskmaster) << "Caught exception. Waiting for the rest of tasks to finish...\n";
				FutureVec vec = future_vec();
				boost::wait_for_all(vec.begin(), vec.end());
			}
		}
		std::size_t num_scheduled_jobs() const { return futures.size(); }
	};

	enum TaskState { SCHEDULED, BLOCKED, TO_BUILD, BUILT };

	void parallel_build(TaskList& tasks, std::vector<Node>& nodes, db::PersistentData& db)
	{
		if(num_jobs) {
			if(num_jobs.get() == 0)
				num_jobs = boost::thread::hardware_concurrency();
			if(num_jobs.get() == 0) {
				logging::warning(logging::Taskmaster) << "Unknown degree of hardware concurrency."
					"Setting the number of parallel jobs to 1\n";
				num_jobs = 1;
			}
			logging::debug(logging::Taskmaster) << "Will execute up to " << num_jobs.get() << " jobs in parallel.\n";
		} else {
			logging::debug(logging::Taskmaster) << "Will execute unlimited number of parallel jobs.\n";
		}

		std::map<Node, TaskState> states;
		JobServer job_server;

		Node end_node = *(--nodes.end());
		while(!states.count(end_node) || states[end_node] != BUILT) {
			foreach(Node node, job_server.wait_for_any())
				states[node] = BUILT;

			foreach(Node node, nodes) {
				if(states.count(node) && states[node] != BLOCKED) {
					continue;
				}
				states[node] = TO_BUILD;
				foreach(Edge e, out_edges(node, dependency_graph::graph)) {
					if(states[target(e, dependency_graph::graph)] == SCHEDULED ||
					   states[target(e, dependency_graph::graph)] == BLOCKED)
						states[node] = BLOCKED;
				}
				if(states[node] == TO_BUILD) {
					if(!dependency_graph::graph[node]->task() ||
						is_task_up_to_date(*(tasks.get<task_index>().find(
							dependency_graph::graph[node]->task())), db)
					) {
						states[node] = BUILT;
					} else {
						job_server.schedule(node);
						logging::debug(logging::Taskmaster)
							<< "Scheduled building target " << dependency_graph::properties(node).name() << ".\n";
						states[node] = SCHEDULED;
						if(num_jobs && job_server.num_scheduled_jobs() == *num_jobs) {
							logging::debug(logging::Taskmaster)
								<< "All slots taken. Waiting...\n";
							break;
						}
					}
				}
			}
		}
	}

	void build(dependency_graph::Node end_goal)
	{
		TaskList tasks;
		std::vector<Node> nodes;
		build_order(end_goal, tasks, nodes);
		db::PersistentData& db = db::get_global_db();

		parallel_build(tasks, nodes, db);
	}

}
