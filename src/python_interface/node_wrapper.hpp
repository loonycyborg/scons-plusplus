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

#ifndef NODE_WRAPPER_HPP
#define NODE_WRAPPER_HPP

#include <boost/python.hpp>
#include <boost/python/stl_iterator.hpp>
#include <boost/cast.hpp>

#include "dependency_graph.hpp"
#include "node_properties.hpp"
#include "task.hpp"
#include "builder.hpp"
#include "sconscript.hpp"
#include "fs_node.hpp"

using namespace boost::python;

namespace sconspp
{
namespace python_interface
{

struct NodeWrapper
{
	Node node;
	explicit NodeWrapper(Node node_) : node(node_) {}
	std::string to_string() const { return graph[node]->name(); }

	NodeList sources() const { return graph[node]->task()->sources(); }
	std::string path() const {
		return properties<FSEntry>(node).name();
	}
	std::string abspath() const {
		return properties<FSEntry>(node).abspath();
	}
	std::string name() const {
		return properties<FSEntry>(node).file();
	}
	std::string dir() const {
		return properties<FSEntry>(node).dir();
	}
	std::string get_contents() const {
		try {
			return properties<FSEntry>(node).get_contents();
		} catch(const std::bad_cast&) { return properties(node).name(); }
	}
	std::string scanner_key() const {
		try {
			FSEntry& props = properties<FSEntry>(node);
			return props.suffix();
		} catch(const std::bad_cast&) {}
		return std::string();
	}
	bool exists() const
	{
		return properties<FSEntry>(node).exists();
	}
};

inline std::string extract_string_subst(const Environment& env, object obj)
{
	return env.subst(extract<std::string>(obj));
}

NodeList extract_file_nodes(object obj);
NodeList extract_file_nodes(const Environment& env, object obj);

inline Node extract_node(object obj)
{
	return extract<NodeWrapper>(obj)().node;
}

Builder::NodeStringList extract_nodes(object obj);
Builder::NodeStringList extract_nodes(const Environment& env, object obj);

}
}

#endif
