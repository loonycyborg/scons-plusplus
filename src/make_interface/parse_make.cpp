#include <boost/optional.hpp>
//#define BOOST_SPIRIT_X3_DEBUG
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/include/support_istream_iterator.hpp>

#include <string>
#include <vector>
#include <iostream>
#include <fstream>

#include "environment.hpp"
#include "builder.hpp"

#include "parse_make.hpp"

namespace sconspp
{

namespace make_interface
{

namespace x3 = boost::spirit::x3;
namespace ascii = boost::spirit::x3::ascii;

using x3::lit;
using x3::lexeme;
using x3::eol;
using x3::_attr;
using x3::_val;
using x3::omit;

using ascii::char_;
using ascii::string;
using boost::spirit::x3::ascii::blank;
using boost::spirit::x3::ascii::graph;
using boost::spirit::x3::ascii::alnum;

struct make_command_ast
{
	bool silent;
	bool ignore_error;
	bool no_suppress;
	std::string command;
	make_command_ast(std::string command) : command(command) {}
	make_command_ast() {}
};

struct make_rule_ast
{
	NodeStringList targets;
	NodeStringList sources;
	std::vector<make_command_ast> commands;
};

struct makefile_ast
{
	Environment::pointer env;
	std::vector<make_rule_ast> rules;

	makefile_ast() { env = Environment::create(); }
};

x3::rule<class make_macro, std::vector<std::string>> make_macro = "make_macro";
x3::rule<class make_target, std::string> make_target = "make_target";
x3::rule<class make_command, make_command_ast> make_command = "make_command";
x3::rule<class make_rule, make_rule_ast> make_rule = "make_rule";
x3::rule<class make_makefile, makefile_ast> make_makefile = "makefile";

auto const make_macro_def = lexeme[+(graph - '=')] >> "=" >> -lexeme[+(char_ - eol - '#')];
auto const make_target_def = lexeme[+(graph - ':')];
auto set_silent = [] (auto& ctx){ _val(ctx).silent = true; };
auto set_ignore_error = [] (auto& ctx){ _val(ctx).ignore_error = true; };
auto set_no_suppress = [] (auto& ctx){ _val(ctx).no_suppress = true; };
auto add_command_string = [] (auto& ctx){ _val(ctx).command = _attr(ctx); };
auto const make_command_def = lit('\t') >>
	*(lit('@')[set_silent] | lit('-')[set_ignore_error] | lit('+')[set_no_suppress]) >>
	lexeme[+(char_-eol)][add_command_string];
auto add_target = [](auto& ctx){ _val(ctx).targets.push_back(_attr(ctx)); };
auto add_source = [](auto& ctx){ _val(ctx).sources.push_back(_attr(ctx)); };
auto add_command = [](auto& ctx){ _val(ctx).commands.push_back(_attr(ctx)); };
auto const make_rule_def = +make_target[add_target] >> ":" >> *make_target[add_source] >> -(eol >> make_command[add_command]);
auto add_macro = [](auto& ctx)
{
	assert(_attr(ctx).size() == 1 || _attr(ctx).size() == 2);
	Environment& env = *(_val(ctx).env);
	auto& var = env[_attr(ctx)[0]];
	if(_attr(ctx).size() == 2)
		var = make_variable(_attr(ctx)[1]);
};
auto add_rule = [](auto& ctx)
{
	const Environment& env = *(_val(ctx).env);
	auto& ast = _attr(ctx);
	ActionList commands;
	for(const auto& command : ast.commands)
		commands.push_back(Action::pointer{new ExecCommand(command.command)});
	auto targets = sconspp::add_command(env, ast.targets, ast.sources, commands);
	if(default_targets.empty() && !commands.empty())
		for(Node target : targets)
			default_targets.insert(target);
};
auto const make_makefile_def = *eol >> (make_macro[add_macro] | make_rule[add_rule]) % eol;

BOOST_SPIRIT_DEFINE(make_macro, make_target, make_command, make_rule, make_makefile);

void run_makefile(const std::string& makefile_path, int argc, char** argv)
{
	std::ifstream ifs{makefile_path};
	ifs.unsetf(std::ios::skipws);
	boost::spirit::istream_iterator iter{ifs}, i_end;

	makefile_ast makefile;
	bool match = phrase_parse(iter, i_end, make_makefile,
		blank - '\t' | lexeme['#' >> *(char_ - eol)] | (eol >> &eol) | (lit('\\') >> eol),
		makefile);
	for(auto variable : *(makefile.env)) {
		std::cout << variable.first << " = " << variable.second->to_string() << std::endl;
	}
	assert(match);
}

}
}
