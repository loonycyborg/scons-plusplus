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

#include <boost/graph/graphviz.hpp>

#include "write_dot.hpp"
#include "node_properties.hpp"

using dependency_graph::Graph;
using dependency_graph::Node;

namespace visualization
{
class DAG_writer
{
	const Graph& graph;
	public:
	DAG_writer(const Graph& graph_) : graph(graph_) {}
	void operator()(std::ostream& out, const Node& node) const
	{
		out << "[label=\"" << graph[node]->name() << "\"]";
	}
};

void write_dot(std::ostream& os, const dependency_graph::Graph& graph)
{
	boost::write_graphviz(os, graph, DAG_writer(graph), boost::default_writer(), boost::default_writer(), dependency_graph::IdMap(graph));
}

}
