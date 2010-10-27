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

#include <iostream>

namespace logging {

enum Severity
{
	Error,
	Warning,
	Debug
};
const char* const severity_msgs[] = { "***", "warning:", "debug:" };

enum Domain
{
	General,
	Taskmaster
};
const char* const domain_msgs[] = { "", "taskmaster:" } ;

template<Severity severity>
class log
{
	Domain domain;
	public:
	log(Domain domain = General) : domain(domain) {}
	template<class T>
	std::ostream& operator<<(const T& msg) 
	{
		return std::cerr << "scons++: " <<
			severity_msgs[severity] << " " <<
			domain_msgs[domain] << (strlen(domain_msgs[domain]) >= 1 ? " " : "") <<
			msg;
	}
};
typedef log<Error> error;
typedef log<Warning> warning;
typedef log<Debug> debug;

}
