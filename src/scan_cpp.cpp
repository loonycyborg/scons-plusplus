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

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_container.hpp>
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

// HACK: need a general framework for env lookup caching.
std::vector<std::string>& lookup_searchpath(const sconspp::Environment& env)
{
	static std::map<const sconspp::Environment*, std::vector<std::string> > lookup_cache;
	if(!lookup_cache.count(&env)) {
		lookup_cache[&env];
		for(std::string dir : env["CPPPATH"]->to_string_list()) {
			lookup_cache[&env].push_back(dir);
		}
	}
	return lookup_cache[&env];
}

namespace sconspp
{
    void scan_cpp(const Environment& env, Node target, Node source)
	{
		try {
			PersistentData& db = get_global_db();
			IncludeDeps& deps = db.record_current_data(source).scanner_cache();
			if(!graph[source]->unchanged(NodeList(), db.record_current_data(source))) {
				deps.clear();
				std::string contents = properties<FSEntry>(source).get_contents();
				std::string::iterator iter(contents.begin()), iend(contents.end());
				cpp<std::string::iterator> preprocessor;
				parse(iter, iend, preprocessor, deps);
			}

			for(const IncludeDeps::value_type& item : deps) {
				std::vector<std::string> search_paths = lookup_searchpath(env);
				if(!item.first) {
					std::string source_dir(properties<FSEntry>(source).dir());
					search_paths.push_back(source_dir);
				}
				boost::optional<Node> included_file = find_file(item.second, search_paths, true);
				if(included_file) {
					bool added;
					boost::tie(boost::tuples::ignore, added) = add_edge(target, *included_file, graph);
					if(added) // Only recurse into newly added edges to prevent infinite recursion in case of circular includes
						scan_cpp(env, target, *included_file);
				}
			}
		} catch(const std::bad_cast&) {}
	}
}
