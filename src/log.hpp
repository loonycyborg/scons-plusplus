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

class null_streambuf : public std::streambuf
{
	virtual int overflow(int c) { return std::char_traits< char >::not_eof(c); }
public:
	null_streambuf() {}
};

static std::ostream null_ostream(new null_streambuf);

enum Severity
{
	Error,
	Warning,
	Info,
	Debug
};
const char* const severity_msgs[] = { "***", "warning:", "info:", "debug:" };
extern unsigned int min_severity;

enum Domain
{
	General,
	Taskmaster,
	System,
	Makefile
};
const char* const domain_msgs[] = { "", "taskmaster:", "system:", "Makefile:" } ;

template<Severity severity>
class log
{
	Domain domain;
	public:
	log(Domain domain = General) : domain(domain) {}
	template<class T>
	std::ostream& operator<<(const T& msg) 
	{
		if(severity <= min_severity)
			return std::cerr << "scons++: " <<
				severity_msgs[severity] << " " <<
				domain_msgs[domain] << (strlen(domain_msgs[domain]) >= 1 ? " " : "") <<
				msg;
		else
			return null_ostream;
	}
};
typedef log<Error> error;
typedef log<Warning> warning;
typedef log<Info> info;
typedef log<Debug> debug;

}
