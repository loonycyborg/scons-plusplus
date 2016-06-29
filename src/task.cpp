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

#include <boost/foreach.hpp>

#include "task.hpp"
#include "util.hpp"
#include "node_properties.hpp"

#define foreach BOOST_FOREACH

namespace sconspp
{

void Task::add_sources(const NodeList& sources)
{
	std::copy(sources.begin(), sources.end(), std::back_inserter(sources_));
	foreach(const Node& target, targets_)
		foreach(const Node& source, sources)
			add_edge(target, source, graph);
}

Environment::const_pointer Task::env() const
{
	Environment::pointer task_env = env_->override();
	if(targets_.size()) {
		(*task_env)["TARGETS"] = make_variable(targets_.begin(), targets_.end());
		(*task_env)["TARGET"] = make_variable(targets_[0]);
	}
	if(sources_.size()) {
		(*task_env)["SOURCES"] = make_variable(sources_.begin(), sources_.end());
		(*task_env)["SOURCE"] = make_variable(sources_[0]);
	}
	return task_env;
}

boost::optional<boost::array<unsigned char, 16> > Task::signature() const
{
	boost::optional<boost::array<unsigned char, 16> > result;
	if(actions_.empty())
		return result;
	MD5 md5_sum;
	foreach(const Action::pointer& action, actions_)
		md5_sum.append(action->to_string(*env(), true));
	return md5_sum.finish();
}

void Task::execute() const
{
	Environment::const_pointer task_env = env();
	foreach(const Action::pointer& action, actions_)
		sconspp::execute(action, *task_env);
	foreach(const Node& target, targets_)
		properties(target).was_rebuilt();
}

}
