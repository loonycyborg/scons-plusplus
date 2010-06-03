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

namespace builder
{
class Builder;
}

namespace taskmaster
{

class Task
{
	friend class builder::Builder;
	Task(
		const environment::Environment& env,
		const dependency_graph::NodeList& targets,
		const dependency_graph::NodeList& sources,
		const action::ActionList& actions) :
			targets_(targets),
			actions_(actions), env_(env.shared_from_this())
		{
			add_sources(sources);
		}
	public:
	typedef boost::function<void(const environment::Environment&, dependency_graph::Node, dependency_graph::Node)> Scanner;
	typedef boost::shared_ptr<Task> pointer;
	typedef boost::shared_ptr<const Task> const_pointer;

	const dependency_graph::NodeList& targets() const { return targets_; }
	const dependency_graph::NodeList& sources() const { return sources_; }
	void add_sources(const dependency_graph::NodeList& sources);
	action::ActionList& actions() { return actions_; }
	environment::Environment::const_pointer env() const;

	void scan(dependency_graph::Node target, dependency_graph::Node source) const { if(scanner_) scanner_(*env_, target, source); }
	void set_scanner(Scanner scanner) { scanner_ = scanner; }

	boost::optional<boost::array<unsigned char, 16> > signature() const;

	void execute() const;

	private:

	dependency_graph::NodeList targets_;
	dependency_graph::NodeList sources_;

	action::ActionList actions_;

	environment::Environment::const_pointer env_;

	Scanner scanner_;
};

}

#endif
