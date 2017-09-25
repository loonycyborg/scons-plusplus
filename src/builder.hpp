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
#include "task.hpp"

namespace sconspp
{

typedef std::vector<boost::variant<Node, std::string> > NodeStringList;

NodeList add_command(const Environment& env,
				 const NodeStringList& targets,
				 const NodeStringList& sources,
				 const ActionList& actions);

NodeList add_alias(const Environment& env,
				 const NodeStringList& targets,
				 const NodeStringList& sources,
				 const ActionList& actions);

class Builder
{
	protected:
	void create_task(
	    const Environment& env,
	    const NodeList& targets,
	    const NodeList& sources,
	    const ActionList& actions,
	    Task::Scanner scanner = Task::Scanner()
		) const;
	public:
	typedef std::shared_ptr<Builder> pointer;

	virtual ~Builder() {}

	virtual NodeList operator()(
	    const Environment& env,
		const NodeStringList& targets,
		const NodeStringList& sources
		) const = 0;

	static NodeList add_task(
			const Environment& env,
			const NodeList& targets,
			const NodeList& sources,
			const ActionList& actions
			);
};

class SimpleBuilder : public Builder
{
	ActionList actions_;

	public:
	NodeList operator()(
	    const Environment& env,
		const NodeStringList& targets,
		const NodeStringList& sources
		) const;

	SimpleBuilder(const ActionList& actions) : actions_(actions) {}
};

}

#endif
