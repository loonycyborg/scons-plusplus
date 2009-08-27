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
#include <boost/variant/variant_fwd.hpp>

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
		const action::ActionList& actions
		) const;
	public:
	typedef boost::shared_ptr<Builder> pointer;

	virtual ~Builder() {}

	typedef std::vector<boost::variant<dependency_graph::Node, std::string> > NodeStringList;
	virtual dependency_graph::NodeList operator()(
		const environment::Environment& env,
		const NodeStringList& targets,
		const NodeStringList& sources
		) const = 0;
};

class Command : public Builder
{
	std::string command_;

	public:
	dependency_graph::NodeList operator()(
		const environment::Environment& env,
		const NodeStringList& targets,
		const NodeStringList& sources
		) const;

	Command(std::string command) : command_(command) {}
};

class SimpleBuilder : public Builder
{
	action::ActionList actions_;

	public:
	dependency_graph::NodeList operator()(
		const environment::Environment& env,
		const NodeStringList& targets,
		const NodeStringList& sources
		) const;

	SimpleBuilder(const action::ActionList& actions) : actions_(actions) {}
};

class AliasBuilder : public Builder
{
	action::ActionList actions_;

	public:
	dependency_graph::NodeList operator()(
		const environment::Environment& env,
		const NodeStringList& targets,
		const NodeStringList& sources
		) const;

	AliasBuilder(const action::ActionList& actions) : actions_(actions) {}
};

}

#endif
