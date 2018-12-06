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

#include "dependency_graph.hpp"
#include "node_properties.hpp"
#include "fs_node.hpp"
#include "alias_node.hpp"
#include "write_dot.hpp"
#include "taskmaster.hpp"
#include "log.hpp"
#include "options.hpp"
#include "frontend.hpp"
#include "util.hpp"

#include <fstream>

#include <boost/filesystem/operations.hpp>

using namespace sconspp;

int main(int argc, char** argv)
{
	try {
		std::vector<std::string> command_line_targets = parse_command_line(argc, argv);
		run_script(argc, argv);

		Node end_goal = add_dummy_node("The end goal");
		if(!command_line_targets.empty()) {
			for(std::string target : command_line_targets) {
				boost::optional<Node> node = get_alias(target);
				if(!node)
					node = get_entry(target);
				if(node)
					add_edge(end_goal, node.get(), graph);
				else
					throw std::runtime_error("I don't see alias or file target named '" + target + "'. That's really all there is to be said on the matter");
			}
		} else
			for(Node node : default_targets)
				add_edge(end_goal, node, graph);
		build(end_goal);
	} catch(const std::exception& e) {
		logging::error() << e.what() << std::endl;
		return 1;
	} catch(const restart_exception& e) {
		get_global_db(true); // flush the database
		e.do_restart(argv);
	}

	{
	std::ofstream dot_file("graph.dot");
	write_dot(dot_file, graph);
	}
}
