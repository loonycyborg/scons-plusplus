/***************************************************************************
 *   Copyright (C) 2010 by Sergey Popov                                    *
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

#include <vector>
#include <boost/program_options.hpp>
#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

#include "options.hpp"
#include "log.hpp"

namespace options
{

std::vector<std::string> parse(int argc, char** argv)
{
	boost::program_options::options_description desc("Usage: scons++ [option]... [target]...\nOptions");
	desc.add_options()
		("debug,d", "Enable debug messages")
		("help,h", "Produce this message and exit")
		("target", boost::program_options::value<std::vector<std::string> >(), "Specify a build target(equivalent to the positional arguments)");
	boost::program_options::positional_options_description p;
	p.add("target", -1);
	boost::program_options::variables_map vm;
	boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
	boost::program_options::notify(vm);

	if(vm.count("help")) {
		std::cout << desc << std::endl;
		exit(0);
	}

	if(vm.count("debug")) {
		logging::min_severity = 2;
	}

	std::vector<std::string> targets;
	if(vm.count("target"))
		targets = vm["target"].as<std::vector<std::string> >();
	return targets;
}

}
