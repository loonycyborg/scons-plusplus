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

#include "alias_node.hpp"

#include <map>

namespace
{
	typedef std::map<std::string, dependency_graph::Node> AliasNamespace;
	AliasNamespace alias_namespace;
}


namespace dependency_graph
{

Alias::Alias(const std::string& name) : name_(name)
{
}

Alias::~Alias()
{
}

Node add_alias(const std::string& name)
{
	Node alias;
	AliasNamespace::iterator iter = alias_namespace.find(name);
	if(iter != alias_namespace.end()) {
		alias = iter->second;
	} else {
		alias = add_vertex(graph);
		alias_namespace[name] = alias;
		graph[alias].reset(new Alias(name));
	}

	return alias;
}

boost::optional<Node> get_alias(const std::string& name)
{
	AliasNamespace::iterator iter = alias_namespace.find(name);
	if(iter != alias_namespace.end())
		return iter->second;
	else
		return boost::optional<Node>();
}

}
