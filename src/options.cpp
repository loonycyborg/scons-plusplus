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
#include <boost/optional.hpp>

#include "options.hpp"
#include "log.hpp"
#include "taskmaster.hpp"
#include "frontend.hpp"
#include "environment.hpp"

namespace sconspp
{

template<typename T>
struct optional_last_overrides
{
	boost::optional<T> value;
	optional_last_overrides() {}
	optional_last_overrides(T value) : value(value) {} 
};

template<typename T>
static void validate(boost::any& v, 
	const std::vector<std::string>& values,
	optional_last_overrides<T>* target_type, int)
{
	const std::string& s = boost::program_options::validators::get_single_string(values);
	optional_last_overrides<T> ret;
	ret.value = boost::lexical_cast<T>(s);
	v = ret;
}

std::pair<std::vector<std::string>, std::vector<std::pair<std::string, std::string>>> parse_command_line(int argc, char** argv)
{
	boost::program_options::options_description desc("Usage: scons++ [option]... [target]...\nOptions");
	desc.add_options()
		("scripting-frontend,F", boost::program_options::value<Frontend>(&commandline_frontend), "Scripting frontend to use. Possible values: 'scons', 'make'")
		("file,f", boost::program_options::value<std::string>(&buildfile), "Exact name of buildfile to parse")
		("debug,d", "Enable debug messages")
		("jobs,j", boost::program_options::value<optional_last_overrides<unsigned int> >()
			->implicit_value(optional_last_overrides<unsigned int>(), "unlimited")
			->default_value(optional_last_overrides<unsigned int>(0), "0"),
			"Maximun number of parallel jobs. 0 means autodetect, no arg means unlimited")
		("always-build,B", boost::program_options::bool_switch(), "Rebuild all tasks no matter whether they're up-to-date")
		("keep-going,k", boost::program_options::bool_switch(), "Continue building after a task fails and build all targets that don't depend on failed targets")
		("target,T", boost::program_options::value<std::vector<std::string> >(), "Specify build target(s)")
		("override,D", boost::program_options::value<std::vector<std::string> >(), "Override construction variables")
		("help,h", "Produce this message and exit")
		("target-or-macro", boost::program_options::value<std::vector<std::string> >(), "Specify a build target or override a macro with MACRO=VALUE(equivalent to positional arguments)");
	boost::program_options::positional_options_description p;
	p.add("target-or-macro", -1);
	boost::program_options::variables_map vm;
	auto parsed { boost::program_options::command_line_parser(argc, argv).options(desc).positional(p).allow_unregistered().run() };
	boost::program_options::store(parsed, vm);
	boost::program_options::notify(vm);

	std::vector<std::string> unknown_options;
	for(auto option : parsed.options) {
		if(option.unregistered) {
			if(frontend_allows_unknown_options(commandline_frontend)) {
				unknown_options.push_back(option.original_tokens[0]);
			} else {
				throw std::runtime_error("unknown option: " + option.string_key);
			}
		}
	}

	if(vm.count("help")) {
		std::cout << desc << std::endl;
		exit(0);
	}

	if(vm.count("debug")) {
		logging::min_severity = 3;
	}
	optional_last_overrides<unsigned int> num_jobs = vm["jobs"].as<optional_last_overrides<unsigned int> >();
	sconspp::num_jobs = num_jobs.value;
	always_build = vm["always-build"].as<bool>();
	keep_going = vm["keep-going"].as<bool>();

	std::vector<std::string> targets;
	if(vm.count("target")) {
		targets = vm["target"].as<std::vector<std::string>>();
	}
	std::vector<std::pair<std::string, std::string>> overrides;
	if(vm.count("override")) {
		for(auto override : vm["override"].as<std::vector<std::string>>()) {
			auto i = override.find("=");
			overrides.emplace_back(override.substr(0, i), (i == std::string::npos) ? "1" : override.substr(i+1));
		}
	}
	if(vm.count("target-or-macro")) {
		for(auto str : vm["target-or-macro"].as<std::vector<std::string>>()) {
			auto i = str.find("=");
			if(i == std::string::npos) {
				targets.push_back(str);
			} else {
				overrides.emplace_back(str.substr(0, i), str.substr(i+1));
			}
		}
	}
	return { targets, overrides };
}

}
