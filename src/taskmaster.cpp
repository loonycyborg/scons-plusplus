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

namespace {

using namespace sconspp;

struct TaskListItem
{
	Task::pointer task;
	NodeList targets;

	TaskListItem(Task::pointer task, Node target) : task(task) { targets.push_back(target); }
};

struct sequence_index;
struct task_index;

typedef multi_index_container<
	TaskListItem,
	indexed_by<
		sequenced<tag<sequence_index> >,
        ordered_unique<tag<task_index>, BOOST_MULTI_INDEX_MEMBER(TaskListItem, Task::pointer, task)>
		>
	> TaskList;

class BuildVisitor : public boost::default_dfs_visitor
{
	TaskList& task_list_;
	std::vector<Node>& build_order_;

	public:
	BuildVisitor(TaskList& task_list, std::vector<Node>& build_order) : task_list_(task_list), build_order_(build_order) {}

	template <typename Edge>
	void back_edge(const Edge&, const Graph& graph) const { throw boost::not_a_dag(); }
	void discover_vertex(Node node, const Graph& graph) const
	{
		const std::pair<Graph::out_edge_iterator, Graph::out_edge_iterator>&
			iterators = out_edges(node, graph);
		std::vector<Edge> edges(iterators.first, iterators.second);
		for(Edge edge : edges) {
			Task::pointer task = graph[source(edge, graph)]->task();
			if(task)
				task->scan(source(edge, graph), target(edge, graph));
		}
	}
	void finish_vertex(Node node, const Graph& graph) const
	{
		Task::pointer task = graph[node]->task();
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

}

namespace
{
    std::ostream& operator<<(std::ostream& os, const sconspp::NodeList& node_list)
	{
		os << "(";
		if(node_list.size()) {
			sconspp::NodeList::const_iterator iter = node_list.begin();
			os << sconspp::graph[*iter++]->name();
			for(;iter != node_list.end(); iter++) {
				os << "," << sconspp::graph[*iter]->name();
			}
		}
		os << ")";
		return os;
	}
}

namespace sconspp
{
	boost::optional<unsigned int> num_jobs;
	bool always_build;
	bool keep_going;

	void build_order(Node end_goal, TaskList& tasks, std::vector<Node>& output)
	{
		std::map<Node, boost::default_color_type> colors;
		associative_property_map<std::map<Node, boost::default_color_type> > color_map(colors);
		depth_first_visit(graph, end_goal, BuildVisitor(tasks, output), color_map);
	}

	void build_order(Node end_goal, std::vector<Node>& output)
	{
		TaskList tasks;
		build_order(end_goal, tasks, output);
	}

	bool is_task_up_to_date(const TaskListItem& item, PersistentData& db)
	{
		bool up_to_date = !always_build;
		for(Node build_target : item.targets) {

			// let node check if it has an inherent need to be rebuilt, such as a file not existing
			if(graph[build_target]->needs_rebuild()) {
				up_to_date = false;
			}
			// check if building node failed last run, or if it never was built yet
			if(!db[build_target].task_status() || db[build_target].task_status().get() != 0) {
				logging::debug(logging::Taskmaster) << graph[build_target]->name() << " wasn't built before or failed\n";
				up_to_date = false;
			}
			std::set<int> 
				source_ids,
				prev_sources(db[build_target].dependencies());
			for(Edge dependency : boost::make_iterator_range(out_edges(build_target, graph))) {
				Node build_source = target(dependency, graph);
				PersistentNodeData& source_data = db[build_source];
				source_ids.insert(source_data.id());
				bool unchanged = graph[build_source]->unchanged(item.targets, source_data);
				if(!unchanged) {
					up_to_date = false;
					logging::debug(logging::Taskmaster) <<
					    graph[build_source]->name() << " has changed\n";
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

			// mark node as unbuilt, in case build is interrupted
			if(!up_to_date)
				db[build_target].task_status().reset();
		}

		return up_to_date;
	}

	class JobServer
	{
		typedef std::map<Node, boost::shared_future<int> > Futures;
		Futures futures;
		typedef std::vector<boost::shared_future<int> > FutureVec;
		typedef std::vector<std::pair<Node,int>> ResultVec;
		
		FutureVec future_vec() const
		{
			FutureVec vec;
			for(const Futures::value_type& value : futures)
				vec.push_back(value.second);
			return vec;
		}

		public:
		void schedule(Node node)
		{
			boost::packaged_task<int> ptask(boost::bind(
			    &Task::execute, graph[node]->task().get()
				));
			futures[node] = boost::shared_future<int>(ptask.get_future());
			boost::thread thread(boost::move(ptask));
		}
		ResultVec wait_for_any()
		{
			FutureVec vec = future_vec();
			boost::wait_for_any(vec.begin(), vec.end());
			Futures new_futures;
			ResultVec result;
			for(const auto& fut : futures) {
				if(fut.second.is_ready()) {
					int status = fut.second.get();
					result.push_back(std::make_pair(fut.first, status));
				} else {
					new_futures.insert(fut);
				}
			}
			std::swap(futures, new_futures);
			return result;
		}
		void wait_for_all()
		{
			if(futures.size() > 0) {
				FutureVec vec = future_vec();
				boost::wait_for_all(vec.begin(), vec.end());
			}
		}
		std::size_t num_scheduled_jobs() const { return futures.size(); }
	};

	enum TaskState { SCHEDULED, BLOCKED, TO_BUILD, BUILT, FAILED };

	void parallel_build(TaskList& tasks, std::vector<Node>& nodes, PersistentData& db)
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
		while(!states.count(end_node) || (states[end_node] != BUILT && states[end_node] != FAILED)) {
			for(const auto& result : job_server.wait_for_any()) {
				db[result.first].task_status() = result.second;
				if(result.second == 0)
					states[result.first] = BUILT;
				else {
					states[result.first] = FAILED;
					if(!keep_going) {
						logging::warning(logging::Taskmaster) << "Task failed. Waiting for the rest of active tasks to finish...\n";
						job_server.wait_for_all();
						throw std::runtime_error("Task failed");
					}
				}
			}

			for(Node node : nodes) {
				if(states.count(node) && states[node] != BLOCKED) {
					continue;
				}
				states[node] = TO_BUILD;
				for(Edge e : boost::make_iterator_range(out_edges(node, graph))) {
					if(states[target(e, graph)] == SCHEDULED ||
					   states[target(e, graph)] == BLOCKED)
						states[node] = BLOCKED;
					if(states[target(e, graph)] == FAILED)
						states[node] = FAILED;
				}
				if(states[node] == TO_BUILD) {
					if(!graph[node]->task() ||
						is_task_up_to_date(*(tasks.get<task_index>().find(
					        graph[node]->task())), db)
					) {
						states[node] = BUILT;
					} else {
						job_server.schedule(node);
						logging::debug(logging::Taskmaster)
						    << "Scheduled building target " << properties(node).name() << ".\n";
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

	void build(Node end_goal)
	{
		TaskList tasks;
		std::vector<Node> nodes;
		build_order(end_goal, tasks, nodes);
		PersistentData& db = get_global_db();

		parallel_build(tasks, nodes, db);
	}

}
