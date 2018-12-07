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

#ifndef ALIAS_NODE_HPP
#define ALIAS_NODE_HPP

#include "dependency_graph.hpp"
#include "node_properties.hpp"

namespace sconspp
{

class Alias : public node_properties
{
	std::string name_;
	public:
	Alias(const std::string& name);
	~Alias();
	std::string name() const { return name_; }
	const char* type() const { return "alias"; }
	bool unchanged(PersistentNodeData&) const { return false; }
};

Node add_alias(const std::string& name);
boost::optional<Node> get_alias(const std::string& name);

}

#endif
