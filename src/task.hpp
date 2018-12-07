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

#ifndef TASK_HPP
#define TASK_HPP

#include "dependency_graph.hpp"
#include "environment.hpp"
#include "action.hpp"

#include <boost/array.hpp>
#include <boost/optional.hpp>
#include <boost/function.hpp>

namespace sconspp
{

class Builder;

class Task
{
	friend class Builder;
	Task(
	    const Environment& env,
	    const NodeList& targets,
	    const NodeList& sources,
	    const ActionList& actions) :
			targets_(targets),
			actions_(actions), env_(env.shared_from_this())
		{
			add_sources(sources);
		}
	public:
	typedef boost::function<void(const Environment&, Node, Node)> Scanner;
	typedef boost::shared_ptr<Task> pointer;
	typedef boost::shared_ptr<const Task> const_pointer;

	const NodeList& targets() const { return targets_; }
	const NodeList& sources() const { return sources_; }
	void add_sources(const NodeList& sources);
	ActionList& actions() { return actions_; }
	Environment::const_pointer env() const;

	bool database_decider(NodeList targets);
	bool timestamp_pure_decider(NodeList targets);
	bool (Task::*decider)(NodeList) = &Task::database_decider;

	bool is_up_to_date(NodeList targets) { return (this->*decider)(targets); }

	void scan(Node target, Node source) const { if(scanner_) scanner_(*env_, target, source); }
	void set_scanner(Scanner scanner) { scanner_ = scanner; }

	boost::optional<boost::array<unsigned char, 16> > signature() const;

	int execute() const;

	private:

	NodeList targets_;
	NodeList sources_;

	ActionList actions_;

	Environment::const_pointer env_;

	Scanner scanner_;
};

}

#endif
