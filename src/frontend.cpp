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

Frontend commandline_frontend = Frontend::scons;

void run_script(int argc, char** argv)
{
	bool script_found = false;
	const char* scons_script_names[] = { "SConstruct++", "SConstruct" };
	const char* make_script_names[] = { "Makefile" };
	switch(commandline_frontend)
	{
		case Frontend::scons:
			python_interface::init_python();
			for(const char* name : scons_script_names) {
				if(boost::filesystem::exists(name)) {
					script_found = true;
					python_interface::run_script(name, argc, argv);
					break;
				}
			}
			if(!script_found)
				throw std::runtime_error("No SConstruct file found.");
		break;
		case Frontend::make:
			python_interface::init_python();
			for(const char* name : make_script_names) {
				if(boost::filesystem::exists(name)) {
					script_found = true;
					make_interface::run_makefile(name, argc, argv);
					break;
				}
			}
			if(!script_found)
				throw std::runtime_error("No Makefile found.");
		break;
	}
}

}