#include <boost/optional.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
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

class recursive_variable : public Variable
{
	std::string value_;
	const Environment& env_;

	public:
	recursive_variable(const std::string& value, const Environment& env) : value_(value), env_(env) {}
	std::string to_string() const { return make_subst(env_, value_, false); }
	std::list<std::string> to_string_list() const { return { make_subst(env_, value_, false) }; }
	pointer clone() const { return pointer(new recursive_variable(value_, env_)); }
	std::string& get() { return value_; }
};

Variable::pointer make_recursive_variable(std::string value, const Environment& env)
{
	return Variable::pointer{ new recursive_variable{value, env}};
}

struct make_command_ast
{
	bool silent;
	bool ignore_error;
	bool no_suppress;
	std::string command;
	make_command_ast(std::string command) : command(command) {}
	make_command_ast() {}
};

enum class special_target_type
{
	None=0, POSIX
};

struct special_target_symbol_ : boost::spirit::x3::symbols<special_target_type>
{
	special_target_symbol_() {
		add
			("POSIX", special_target_type::POSIX)
		;
	}
} special_target_symbol;

struct make_rule_ast
{
	NodeStringList targets;
	NodeStringList sources;
	special_target_type special_target = special_target_type::None;
	std::vector<make_command_ast> commands;

	void do_special_target() const {
		switch (special_target) {
			case special_target_type::None:
				break;
			case special_target_type::POSIX:
				break;
		}
	}
	NodeList operator()(const Environment& env) const {
		if(special_target != special_target_type::None) {
			do_special_target();
			return {};
		}
		ActionList actions;
		for(const auto& command : commands)
			actions.push_back(Action::pointer{new ExecCommand(command.command)});
		auto result = sconspp::add_command(env, targets, sources, actions);
		if(default_targets.empty() && !result.empty())
			for(Node target : result)
				default_targets.insert(target);
		return result;
	}
};

struct makefile_ast
{
	Environment::pointer env;
	std::vector<make_rule_ast> rules;

	makefile_ast() { env = Environment::create(make_subst); }
};

x3::rule<class make_comment, std::string> make_comment = "make_comment";
x3::rule<class make_blank_line, std::string> make_blank_line = "make_blank_line";
x3::rule<class make_placeholder, std::string> make_placeholder = "make_placeholder";
x3::rule<class make_substitution_pattern, std::string> make_substitution_pattern = "make_substitution_pattern";
x3::rule<class make_macro, std::vector<std::string>> make_macro = "make_macro";
x3::rule<class make_target, std::string> make_target = "make_target";
x3::rule<class make_special_target, special_target_type> make_special_target = "make_special_target";
x3::rule<class make_command, make_command_ast> make_command = "make_command";
x3::rule<class make_rule, make_rule_ast> make_rule = "make_rule";
x3::rule<class make_makefile, makefile_ast> make_makefile = "makefile";
struct env_tag;

auto const make_comment_def = '#' >> *(char_ - eol) >> &eol;
auto const make_blank_line_def = eol >> *blank >> -make_comment >> &eol;

auto do_substitution = [] (auto& ctx)
{
	auto& env = boost::spirit::x3::get<env_tag>(ctx).get();
	const std::string& var = _attr(ctx);
	if(env.count(var)) {
		_val(ctx) += env[var]->to_string();
	}
};
auto const make_placeholder_def = lit('$') >> (
	(lit('(') >> (+(graph - ')'))[do_substitution] >> lit(')')) |
	lit('{') >> (+(graph - '}'))[do_substitution] >> lit('}') |
	(+graph)[do_substitution]);
auto const make_substitution_pattern_def = +(+((graph - '=' - ':' - '$') | (lit('$') >> char_('$'))) | make_placeholder);
auto const make_macro_def = lexeme[+(graph - '=')] >> "=" >> *(*lit('\t') >> lexeme[+(char_ - blank - eol - '\\' - '#')] >> *lit('\t'));
auto const make_target_def = lexeme[make_substitution_pattern];
auto const make_special_target_def = lexeme[lit('.') >> special_target_symbol];
auto set_silent = [] (auto& ctx){ _val(ctx).silent = true; };
auto set_ignore_error = [] (auto& ctx){ _val(ctx).ignore_error = true; };
auto set_no_suppress = [] (auto& ctx){ _val(ctx).no_suppress = true; };
auto add_command_string = [] (auto& ctx){ _val(ctx).command = _attr(ctx); };
auto const make_command_def = lit('\t') >>
	*(lit('@')[set_silent] | lit('-')[set_ignore_error] | lit('+')[set_no_suppress]) >>
	lexeme[+(char_-eol)][add_command_string];
template <typename C> void split_into(C& container, const std::string& input)
{
	std::list<boost::iterator_range<std::string::const_iterator>> tokens;
	boost::algorithm::split(tokens, input, boost::algorithm::is_space(), boost::algorithm::token_compress_on);
	for(auto token : tokens) {
		container.push_back(std::string(token.begin(), token.end()));
	}
}
auto add_target = [](auto& ctx){ split_into(_val(ctx).targets, _attr(ctx)); };
auto add_special_target = [](auto& ctx){ _val(ctx).special_target = _attr(ctx); };
auto add_source = [](auto& ctx){ split_into(_val(ctx).sources, _attr(ctx)); };
auto add_command = [](auto& ctx){ _val(ctx).commands.push_back(_attr(ctx)); };
auto const make_rule_def = -make_special_target[add_special_target] >> *make_target[add_target] >> ":" >> *make_target[add_source] >> -(eol >> make_command[add_command]);
auto add_macro = [](auto& ctx)
{
	assert(_attr(ctx).size() >= 1);
	Environment& env = *(_val(ctx).env);
	auto& var = env[_attr(ctx)[0]];
	if(_attr(ctx).size() == 1)
		var = make_variable(std::string{""});
	else {
		std::vector<std::string> value { ++_attr(ctx).begin(), _attr(ctx).end() };
		var = make_recursive_variable(boost::algorithm::join(value, " "), env);
	}
};
auto add_rule = [](auto& ctx)
{
	const Environment& env = *(_val(ctx).env);
	auto& ast = _attr(ctx);
	ast(env);
};
auto const make_makefile_def = *eol >> (make_macro[add_macro] | make_rule[add_rule]) % eol;

BOOST_SPIRIT_DEFINE(make_comment, make_blank_line, make_placeholder, make_substitution_pattern, make_macro, make_target, make_special_target, make_command, make_rule, make_makefile);

std::string make_subst(const Environment& env, const std::string& input, bool) {
	std::string result;
	bool match = boost::spirit::x3::parse(input.begin(), input.end(),
		boost::spirit::x3::with<env_tag>(std::ref(env))[*(make_substitution_pattern|+(blank | char_(':') | char_('=')))],
		result
	);
	assert(match);
	return result;
}

void run_makefile(const std::string& makefile_path, int argc, char** argv)
{
	std::ifstream ifs{makefile_path};
	ifs.unsetf(std::ios::skipws);
	boost::spirit::istream_iterator iter{ifs}, i_end;

	static makefile_ast makefile;
	bool match = phrase_parse(iter, i_end, boost::spirit::x3::with<env_tag>(std::ref(*(makefile.env)))[make_makefile],
		blank - '\t' | make_blank_line | make_comment | (lit('\\') >> eol),
		makefile);
	for(auto variable : *(makefile.env)) {
		std::cout << variable.first << " = " << variable.second->to_string() << std::endl;
	}
	assert(match);
}

}
}
