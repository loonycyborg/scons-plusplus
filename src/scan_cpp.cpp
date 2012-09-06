/***************************************************************************
 *   Copyright (C) 2011 by Sergey Popov                                    *
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
#include "scan_cpp.hpp"
#include "fs_node.hpp"
#include <iostream>

#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/home/phoenix/container.hpp>
#include <boost/fusion/include/std_pair.hpp>

namespace {

using boost::spirit::qi::grammar;
using boost::spirit::qi::rule;
using boost::spirit::qi::eol;
using boost::spirit::qi::parse;
using boost::spirit::qi::raw;
using boost::spirit::qi::char_;
using boost::spirit::qi::lit;
using boost::spirit::qi::blank;
using boost::spirit::qi::attr;
using boost::spirit::_1;
using boost::spirit::_val;

typedef std::pair<bool, std::string> IncludeDep;
typedef std::set<IncludeDep> IncludeDeps;

template <typename Iterator>
struct cpp : grammar<Iterator, IncludeDeps() >
{
	cpp() : cpp::base_type(file)
	{
		file = *((!lit('#') >> !lit('/')  >> char_) | comment | directive[boost::phoenix::insert(_val, _1)] | char_);
		directive %= '#' >> *cpp_whitespace >> include >> *cpp_whitespace;
		include %= "include" >> +cpp_whitespace >> include_target;
		include_target %= ('<' >> attr(true) >> raw[*(char_ - '>' - eol)] >> '>') |
			('"' >> attr(false) >> raw[*(char_ - '"' - eol)] >> '"');
		cpp_whitespace = blank | comment;
		comment = c_comment | cxx_comment;
		c_comment = "/*" >> *(char_ - "*/") >> "*/";
		cxx_comment = "//" >> *(char_ - eol) >> &eol;
	}
	rule<Iterator, IncludeDeps()> file;
	rule<Iterator, IncludeDep()> directive, include, include_target;
	rule<Iterator> cpp_whitespace, comment, c_comment, cxx_comment;
};

}

namespace taskmaster
{
	void scan_cpp(const environment::Environment& env, dependency_graph::Node target, dependency_graph::Node source)
	{
		try {
			db::PersistentData& db = db::get_global_db();
			IncludeDeps& deps = db[source].scanner_cache();
			if(!dependency_graph::graph[source]->unchanged(dependency_graph::NodeList(), db[source])) {
				deps.clear();
				std::string contents = dependency_graph::properties<dependency_graph::FSEntry>(source).get_contents();
				std::string::iterator iter(contents.begin()), iend(contents.end());
				cpp<std::string::iterator> preprocessor;
				parse(iter, iend, preprocessor, deps);
			}

			foreach(const IncludeDeps::value_type& item, deps) {
				std::vector<std::string> search_paths;
				if(!item.first) {
					std::string source_dir(dependency_graph::properties<dependency_graph::FSEntry>(source).dir());
					search_paths.push_back(source_dir);
				}
				foreach(std::string dir, env["CPPPATH"]->to_string_list()) {
					search_paths.push_back(dir);
				}
				boost::optional<dependency_graph::Node> included_file = dependency_graph::find_file(item.second, search_paths, true);
				if(included_file) {
					bool added;
					boost::tie(boost::tuples::ignore, added) = add_edge(target, *included_file, dependency_graph::graph);
					if(added) // Only recurse into newly added edges to prevent infinite recursion in case of circular includes
						scan_cpp(env, target, *included_file);
				}
			}
		} catch(const std::bad_cast&) {}
	}
}
