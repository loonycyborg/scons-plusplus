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
#pragma once
#include <iostream>
#include <vector>

namespace sconspp
{
	enum struct Frontend { scons, make };
	bool frontend_allows_unknown_options(Frontend);
	std::istream& operator>>(std::istream& in, Frontend& frontend);
	extern Frontend commandline_frontend;
	extern std::string buildfile;

	void run_script(std::vector<std::pair<std::string, std::string>> overrides, std::vector<std::string> command_line_target_strings, int argc, char** argv);
}
