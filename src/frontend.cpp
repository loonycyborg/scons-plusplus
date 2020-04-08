/***************************************************************************
 *   Copyright (C) 2017 by Sergey Popov                                    *
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

#include <boost/filesystem.hpp>

#include "frontend.hpp"
#include "python_interface/python_interface.hpp"
#include "make_interface/parse_make.hpp"

namespace sconspp
{

std::istream& operator>>(std::istream& in, Frontend& frontend)
{
	std::string token;
	in >> token;
	if (token == "scons")
        	frontend = Frontend::scons;
	else if (token == "make")
		frontend = Frontend::make;
	else
		in.setstate(std::ios_base::failbit);
	return in;
}

bool frontend_allows_unknown_options(Frontend frontend)
{
	if(frontend == Frontend::scons)
		return true;

	return false;
}

Frontend commandline_frontend = Frontend::scons;
std::string buildfile;

void run_script(std::vector<std::pair<std::string, std::string>> overrides, std::vector<std::string> command_line_target_strings, int argc, char** argv)
{
	bool script_found = false;
	const char* scons_script_names[] = { "SConstruct++", "SConstruct" };
	const char* make_script_names[] = { "Makefile" };

	if(!buildfile.empty()) {
		script_found = boost::filesystem::exists(buildfile);
		if(!script_found)
			throw std::runtime_error(std::string("Explicitly set buildfile '") + buildfile + "' not found.");
	}

	switch(commandline_frontend)
	{
		case Frontend::scons:
			python_interface::init_python(overrides);
			if(!script_found) {
				for(const char* name : scons_script_names) {
					if(boost::filesystem::exists(name)) {
						script_found = true;
						buildfile = name;
						break;
					}
				}
			}
			if(!script_found)
				throw std::runtime_error("No SConstruct file found.");
			else
				python_interface::run_script(buildfile, command_line_target_strings, argc, argv);
		break;
		case Frontend::make:
			for(const char* name : make_script_names) {
				if(boost::filesystem::exists(name)) {
					script_found = true;
					buildfile = name;
					break;
				}
			}
			if(!script_found)
				throw std::runtime_error("No Makefile found.");
			else
				make_interface::run_makefile(buildfile, command_line_target_strings, overrides);
		break;
	}
}

}
