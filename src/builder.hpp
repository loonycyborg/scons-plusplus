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

#ifndef BUILDER_HPP
#define BUILDER_HPP

#include <deque>
#include <boost/function.hpp>

#include "dependency_graph.hpp"
#include "environment.hpp"
#include "action.hpp"

namespace builder
{

class Builder
{
	protected:
	void create_task(
		const environment::Environment& env,
		const dependency_graph::NodeList& targets,
		const dependency_graph::NodeList& sources,
		const std::deque<action::Action::pointer>& actions
		) const;
	public:
	typedef boost::shared_ptr<Builder> pointer;

	virtual ~Builder() {}
	virtual dependency_graph::NodeList operator()(
		const environment::Environment& env,
		const dependency_graph::NodeList& targets,
		const dependency_graph::NodeList& sources
		) const = 0;

	typedef boost::function<dependency_graph::Node (const environment::Environment&, const std::string&)> NodeFactory;
	virtual NodeFactory target_factory() const;
	virtual NodeFactory source_factory() const;
	virtual std::string adjust_target_name(const environment::Environment&, const std::string& name) const { return name; }
	virtual std::string adjust_source_name(const environment::Environment&, const std::string& name) const { return name; }
};

dependency_graph::Node default_factory(const environment::Environment&, const std::string& name);

class Command : public Builder
{
	std::string command_;

	public:
	dependency_graph::NodeList operator()(
		const environment::Environment& env,
		const dependency_graph::NodeList& targets,
		const dependency_graph::NodeList& sources
		) const;

	Command(std::string command) : command_(command) {}
};

class SimpleBuilder : public Builder
{
	std::deque<action::Action::pointer> actions_;

	public:
	dependency_graph::NodeList operator()(
		const environment::Environment& env,
		const dependency_graph::NodeList& targets,
		const dependency_graph::NodeList& sources
		) const;

	SimpleBuilder(const std::deque<action::Action::pointer>& actions) : actions_(actions) {}
};

}

#endif
