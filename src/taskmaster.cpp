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
#include <boost/multi_index/hashed_index.hpp>
#include <thread>
#include <future>
#include <condition_variable>
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
using boost::multi_index::ordered_non_unique;
using boost::multi_index::hashed_unique;
using boost::multi_index::member;

using boost::tie;

namespace {

using namespace sconspp;

enum TaskState { SCHEDULED, BLOCKED, TO_BUILD, BUILT, FAILED };

struct BuildOrderEntry
{
	Node node;
	Task::pointer task;
	mutable TaskState state { TO_BUILD };
};

struct task_tag {};
struct node_tag {};

typedef multi_index_container<
	BuildOrderEntry,
	indexed_by<
		sequenced<>,
		ordered_non_unique<tag<task_tag>, member<BuildOrderEntry, Task::pointer, &BuildOrderEntry::task>>,
		hashed_unique<tag<node_tag>, member<BuildOrderEntry, Node, &BuildOrderEntry::node>>
	>
> BuildOrder;

class BuildVisitor : public boost::default_dfs_visitor
{
	BuildOrder& build_order_;

	public:
	BuildVisitor(BuildOrder& build_order) : build_order_(build_order) {}

	template <typename Edge>
	void back_edge(const Edge&, const Graph& graph) const { throw boost::not_a_dag(); }
	void discover_vertex(Node node, const Graph& graph) const
	{
		Task::pointer task = graph[node]->task();
		if(!task) return;

		for(auto edge : make_iterator_range(out_edges(node, graph))) {
			task->scan(source(edge, graph), target(edge, graph));
		}
	}
	void finish_vertex(Node node, const Graph& graph) const
	{
		Task::pointer task = graph[node]->task();
		if(task && !task->actions().empty()) {
			task->add_requested_target(node);
		} else task = {};
		build_order_.push_back({ node, task });
	}
};

}

/*
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
*/

namespace sconspp
{
	boost::optional<unsigned int> num_jobs;
	bool always_build;
	bool keep_going;

	void build_order(Node end_goal, BuildOrder& output)
	{
		std::map<Node, boost::default_color_type> colors;
		associative_property_map<std::map<Node, boost::default_color_type> > color_map(colors);
		depth_first_visit(graph, end_goal, BuildVisitor(output), color_map);
	}

	void build_order(Node end_goal, std::vector<Node>& output)
	{
		BuildOrder nodes;
		build_order(end_goal, nodes);

		std::transform(nodes.begin(), nodes.end(), std::back_inserter(output), [](const BuildOrderEntry& node) -> Node { return node.node; } );
	}

	class JobServer
	{
		typedef std::vector<std::pair<Node,int>> ResultVec;
		ResultVec result_vec;
		std::size_t num_scheduled_jobs = 0;
		std::condition_variable num_scheduled_jobs_cv;
		std::mutex num_scheduled_jobs_mutex;

		public:
		void schedule(Node node)
		{
			std::lock_guard<std::mutex> lock { num_scheduled_jobs_mutex };
			std::packaged_task<void()> ptask{ [this, node]() {
				int result;
				try {
					result = graph[node]->task()->execute();
				} catch(std::exception& e) {
					result = -1;
					logging::error(logging::Taskmaster) <<
						"Exception during execution of task: " << e.what() << std::endl;
				}
				std::lock_guard<std::mutex> lock { num_scheduled_jobs_mutex };
				num_scheduled_jobs--;
				num_scheduled_jobs_cv.notify_one();
				result_vec.emplace_back(node, result);
			} };
			num_scheduled_jobs++;
			std::thread thread(std::move(ptask));
			thread.detach();
		}
		bool have_free_slots()
		{
			return !num_jobs || num_scheduled_jobs < num_jobs.get();
		}
		ResultVec wait_for_results()
		{
			std::unique_lock<std::mutex> lock(num_scheduled_jobs_mutex);
			num_scheduled_jobs_cv.wait(lock, [this]{ return have_free_slots(); });
			return std::move(result_vec);
		}
		void wait_for_all()
		{
			std::unique_lock<std::mutex> lock(num_scheduled_jobs_mutex);
			num_scheduled_jobs_cv.wait(lock, [this]{ return num_scheduled_jobs == 0; });
		}
	};

	int parallel_build(BuildOrder& nodes, PersistentData& db)
	{
		int job_counter = 0;
		if(num_jobs) {
			if(num_jobs.get() == 0)
				num_jobs = std::thread::hardware_concurrency();
			if(num_jobs.get() == 0) {
				logging::warning(logging::Taskmaster) << "Unknown degree of hardware concurrency."
					"Setting the number of parallel jobs to 1\n";
				num_jobs = 1;
			}
			logging::debug(logging::Taskmaster) << "Will execute up to " << num_jobs.get() << " jobs in parallel.\n";
		} else {
			logging::debug(logging::Taskmaster) << "Will execute unlimited number of parallel jobs.\n";
		}

		JobServer job_server;

		auto get_state {
			[&nodes](Node node) -> TaskState& {
				return nodes.get<node_tag>().find(node)->state;
			}
		};

		auto last_node = --nodes.end();
		while(last_node->state != BUILT && last_node->state != FAILED) {
			for(const auto& result : job_server.wait_for_results()) {
				auto& node_data { db.record_current_data(result.first) };
				properties(result.first).unchanged(node_data);
				node_data.task_status() = result.second;
				if(result.second == 0) {
					job_counter++;
					get_state(result.first) = BUILT;
				} else {
					get_state(result.first) = FAILED;
					if(!keep_going) {
						logging::warning(logging::Taskmaster) << "Task failed. Waiting for the rest of active tasks to finish...\n";
						job_server.wait_for_all();
						throw std::runtime_error("Task failed");
					}
				}
			}

			for(auto node { nodes.begin() }; node != nodes.end(); node++) {
				if(node->state == BLOCKED || node->state == TO_BUILD) {
					node->state = TO_BUILD;
				} else continue;

				for(Edge e : boost::make_iterator_range(out_edges(node->node, graph))) {
					if(get_state(target(e, graph)) == SCHEDULED ||
					   get_state(target(e, graph)) == BLOCKED) {
						node->state = BLOCKED;
					}
					if(get_state(target(e, graph)) == FAILED) {
						node->state = FAILED;
						break;
					}
				}
				if(node->state == TO_BUILD) {
					auto t = graph[node->node]->task();
					if(!t || t->is_up_to_date()) {
						node->state = BUILT;
					} else {
						job_server.schedule(node->node);
						logging::debug(logging::Taskmaster)
							<< "Scheduled building target " << properties(node->node).name() << ".\n";
						node->state = SCHEDULED;
						if(!job_server.have_free_slots()) {
							logging::debug(logging::Taskmaster)
								<< "All slots taken. Waiting...\n";
							break;
						}
					}
				}
			}
		}

		if(nodes.get<task_tag>().begin()->task == (--nodes.get<task_tag>().end())->task && nodes.get<task_tag>().begin()->task == Task::pointer{}) {
			logging::info(logging::Taskmaster) << "celebration of laziness: no actions assigned to target(s).\n";
		} else {
			if(job_counter == 0) logging::info(logging::Taskmaster) << "celebration of laziness: all targets up-to-date.\n";
		}
		return job_counter;
	}

	int build(Node end_goal)
	{
		BuildOrder nodes;
		build_order(end_goal, nodes);
		PersistentData& db = get_global_db();

		return parallel_build(nodes, db);
	}
}
