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
#define foreach BOOST_FOREACH

#include "task.hpp"

namespace taskmaster
{

void Task::execute() const
{
	environment::Environment::pointer task_env = env_->override();
	if(targets_.size()) {
		(*task_env)["TARGETS"] = environment::make_variable(targets_.begin(), targets_.end());
		(*task_env)["TARGET"] = environment::make_variable(targets_[0]);
	}
	if(sources_.size()) {
		(*task_env)["SOURCES"] = environment::make_variable(sources_.begin(), sources_.end());
		(*task_env)["SOURCE"] = environment::make_variable(sources_[0]);
	}
	foreach(const action::Action::pointer& action, actions_)
		action::execute(action, *task_env);
}

}
