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

#include "task.hpp"
#include "util.hpp"
#include "log.hpp"
#include "node_properties.hpp"
#include "fs_node.hpp"

namespace sconspp
{

void Task::add_sources(const NodeList& sources)
{
	std::copy(sources.begin(), sources.end(), std::back_inserter(sources_));
	for(const Node& target : targets_)
		for(const Node& source : sources)
			add_edge(target, source, graph);
}

Environment::const_pointer Task::env() const
{
	Environment::pointer task_env = env_->override();
	task_env->setup_task_context(*this);
	return task_env;
}

bool Task::database_decider(NodeList targets)
{
	bool up_to_date = true;
	auto& db = get_global_db();
	for(Node build_target : targets) {

		// let node check if it has an inherent need to be rebuilt, such as a file not existing
		if(graph[build_target]->needs_rebuild()) {
			up_to_date = false;
		}
		// check if building node failed last run, or if it never was built yet
		auto& target_data = db.record_current_data(build_target);
		if(!target_data.task_status() || target_data.task_status().get() != 0) {
			logging::debug(logging::Taskmaster) << graph[build_target]->name() << " wasn't built before or failed\n";
			up_to_date = false;
		}
		// check if dependencies have changed
		std::set<int>
			prev_sources{ target_data.dependencies() };
		for(Edge dependency : boost::make_iterator_range(out_edges(build_target, graph))) {
			Node build_source = target(dependency, graph);
			auto& source_data = db.record_current_data(build_source);
			int source_id = source_data.id();

			bool unchanged;
			if(prev_sources.count(source_id)) {
				prev_sources.erase(source_id);
				unchanged = graph[build_source]->unchanged(targets, source_data);
			} else {
				auto prev_id = source_data.prev_id();
				if(prev_id && prev_sources.count(prev_id.get())) {
					unchanged = graph[build_source]->unchanged(targets, source_data);
					prev_sources.erase(prev_id.get());
				} else {
					auto archive_record = target_data.map_to_archive_dep(source_id);
					if(archive_record) {
						prev_sources.erase(archive_record.get());
						auto& archive_data = db.get_archive_data(archive_record.get());
						unchanged = graph[build_source]->unchanged(targets, archive_data);
						logging::debug(logging::Taskmaster) << graph[build_source]->name() << " is an archived dependency\n";
					} else {
						logging::debug(logging::Taskmaster) << graph[build_source]->name() << " is a new dependency\n";
						unchanged = false;
					}
				}
			}
			if(!unchanged) {
				logging::debug(logging::Taskmaster) << graph[build_source]->name() << " has changed or is a new dependency\n";
				up_to_date = false;
				db.schedule_clean_db();
			}
		}

		if(!prev_sources.empty()) {
			logging::debug(logging::Taskmaster) << "Dependency relations have changed\n";
			up_to_date = false;
		}

		if(target_data.task_signature() != signature()) {
			up_to_date = false;
			target_data.task_signature() = signature();
			logging::debug(logging::Taskmaster) << "Task signature has changed\n";
		}

		// mark node as unbuilt, in case build is interrupted
		if(!up_to_date)
			target_data.task_status().reset();
	}

	return up_to_date;
}

bool Task::timestamp_pure_decider(NodeList targets)
{
	for(auto target : targets) {
		for(auto source : sources_) {
			try {
				auto target_timestamp = properties<FSEntry>(target).timestamp();
				auto source_timestamp = properties<FSEntry>(source).timestamp();
				if(source_timestamp > target_timestamp) {
					logging::debug(logging::Taskmaster) <<
						"Source '" << properties(source).name() << "' is older than target '" << properties(target).name() << "'\n";
					return false;
				}
			} catch (const std::bad_cast&) {
				return false;
			}
		}
	}
	return true;
}

boost::optional<boost::array<unsigned char, 16> > Task::signature() const
{
	boost::optional<boost::array<unsigned char, 16> > result;
	if(actions_.empty())
		return result;
	MD5 md5_sum;
	for(const Action::pointer& action : actions_)
		md5_sum.append(action->to_string(*env(), true));
	return md5_sum.finish();
}

int Task::execute() const
{
	Environment::const_pointer task_env = env();
	int status = 0;
	for(const Action::pointer& action : actions_) {
		status = sconspp::execute(action, *task_env);
		if(status != 0)
			break;
	}

	for(const Node& target : targets_)
		properties(target).was_rebuilt(status);
	return status;
}

}
