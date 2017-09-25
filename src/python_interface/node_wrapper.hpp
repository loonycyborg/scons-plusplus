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

#include <boost/cast.hpp>

#include "dependency_graph.hpp"
#include "node_properties.hpp"
#include "task.hpp"
#include "builder.hpp"
#include "sconscript.hpp"
#include "fs_node.hpp"



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

inline std::string extract_string_subst(const Environment& env, py::object obj)
{
	return env.subst(obj.cast<std::string>());
}

NodeList extract_file_nodes(py::object obj);
NodeList extract_file_nodes(const Environment& env, py::object obj);

inline Node extract_node(py::object obj)
{
	return obj.cast<NodeWrapper>().node;
}

NodeStringList extract_nodes(py::object obj);
NodeStringList extract_nodes(const Environment& env, py::object obj);

}
}

namespace pybind11 { namespace detail {
	template <> struct type_caster<sconspp::NodeList> {
	public:
		PYBIND11_TYPE_CASTER(sconspp::NodeList, _("NodeList"));

		bool load(handle src, bool) {
			value = sconspp::python_interface::extract_file_nodes(reinterpret_borrow<object>(src));
			return true;
		}

		static handle cast(sconspp::NodeList src, return_value_policy /* policy */, handle /* parent */) {
			list result;
			for(auto node : src)
				result.append(sconspp::python_interface::NodeWrapper(node));
			result.inc_ref();
			return result;
		}
	};
}} // namespace pybind11::detail

#endif
