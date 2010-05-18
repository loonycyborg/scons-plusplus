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

#include <iostream>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/trim.hpp>

#include "action.hpp"
#include "util.hpp"

namespace action
{

void execute(const action::Action::pointer& action, const environment::Environment& env)
{
	std::cout << action->to_string(env) << std::endl;
	action->execute(env);
}

void Command::execute(const environment::Environment& env) const
{
	std::vector<std::string> command;
	std::string command_str = env.subst(command_);
	boost::algorithm::trim(command_str);
	boost::algorithm::split(command, command_str, boost::is_any_of(" "), boost::token_compress_on);

	util::exec(command);
}

std::string Command::to_string(const environment::Environment& env) const
{
	return env.subst(command_, true);
}

}
