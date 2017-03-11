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

#ifndef ACTION_HPP
#define ACTION_HPP

#include <string>
#include <vector>

#include "environment.hpp"

namespace sconspp
{

class Action
{
	public:
	typedef std::shared_ptr<Action> pointer;

	virtual ~Action() {}
	virtual int execute(const Environment&) const = 0;
	virtual std::string to_string(const Environment&, bool for_signature = false) const = 0;
};

typedef std::list<Action::pointer> ActionList;

int execute(const Action::pointer&, const Environment&);

class ExecCommand : public Action
{
	std::string command_;
	std::string output_string_;

	public:
	ExecCommand(const std::string& command) : command_(command), output_string_(command) {}
	ExecCommand(const std::string& command, const std::string& output_string) : command_(command), output_string_(output_string) {}
	
	int execute(const Environment&) const;
	std::string to_string(const Environment&, bool for_signature = false) const;
};

}

#endif
