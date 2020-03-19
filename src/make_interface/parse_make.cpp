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
#include "node_properties.hpp"
#include "builder.hpp"
#include "fs_node.hpp"
#include "taskmaster.hpp"
#include "util.hpp"

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

enum class command_mod
{
	silent, ignore_error, no_suppress
};

struct command_mod_symbol_: boost::spirit::x3::symbols<command_mod>
{
	command_mod_symbol_() {
		add
			("@", command_mod::silent)
			("-", command_mod::ignore_error)
			("+", command_mod::no_suppress)
		;
	}
} command_mod_symbol;

struct make_command_ast
{
	std::set<command_mod> mods;
	std::string command;
};

}}

BOOST_FUSION_ADAPT_STRUCT(
	sconspp::make_interface::make_command_ast,
	mods,
	command
)

namespace sconspp { namespace make_interface {

enum class special_target_type
{
	None=0, POSIX, PRECIOUS
};

struct special_target_symbol_ : boost::spirit::x3::symbols<special_target_type>
{
	special_target_symbol_() {
		add
			("POSIX", special_target_type::POSIX)
			("PRECIOUS", special_target_type::PRECIOUS)
		;
	}
} special_target_symbol;

enum class macro_mod
{
	none=0, no_overwrite, simply_expanded, shell
};

struct macro_mod_symbol_ : boost::spirit::x3::symbols<macro_mod>
{
	macro_mod_symbol_() {
		add
			("?", macro_mod::no_overwrite)
			(":", macro_mod::simply_expanded)
			("!", macro_mod::shell)
		;
	}
} macro_mod_symbol;

struct macro_ast
{
	std::string varname;
	std::set<macro_mod> mods;
	std::vector<std::string> value;
};

}}

BOOST_FUSION_ADAPT_STRUCT(
	sconspp::make_interface::macro_ast,
	varname,
	mods,
	value
)

namespace sconspp { namespace make_interface {

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
			case special_target_type::PRECIOUS:
				for(auto node : make_file_nodes(sources))
				{
					try {
						properties<FSEntry>(node).precious();
					} catch (const std::bad_cast&) {}
				}
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

	makefile_ast() { env = Environment::create(make_subst, setup_make_task_context); }
};

struct env_tag;
auto do_substitution = [] (auto& ctx)
{
	auto& env = boost::spirit::x3::get<env_tag>(ctx).get();
	const std::string& var = _attr(ctx);
	if(env.count(var)) {
		_val(ctx) += env[var]->to_string();
	}
};

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

auto add_macro = [](auto& ctx)
{
	macro_ast& ast { _attr(ctx) };
	Environment& env = *(_val(ctx).env);

	assert(ast.mods.count(macro_mod::shell) == 0);

	if(env.count(ast.varname) && ast.mods.count(macro_mod::no_overwrite))
		return;

	if(ast.mods.count(macro_mod::simply_expanded)) {
		env[ast.varname] = make_variable(boost::algorithm::join(ast.value, " "));
	} else {
		env[ast.varname] = std::make_shared<recursive_variable>(boost::algorithm::join(ast.value, " "), env);
	}
};
auto add_rule = [](auto& ctx)
{
	const Environment& env = *(_val(ctx).env);
	auto& ast = _attr(ctx);
	ast(env);
};

const auto make_comment              = x3::rule<class make_comment> { "make_comment" }
									 = '#' >> *(char_ - eol) >> &eol;
const auto make_blank_line           = x3::rule<class make_blank_line> { "make_blank_line" }
									 = eol >> *blank >> -make_comment >> &eol;
const auto make_placeholder          = x3::rule<class make_placeholder, std::string> { "make_placeholder" }
									 = lit('$') >> (
										(lit('(') >> (+(graph - ')'))[do_substitution] >> lit(')')) |
										lit('{') >> (+(graph - '}'))[do_substitution] >> lit('}') |
										(+graph)[do_substitution]);
const auto make_substitution_pattern = x3::rule<class make_substitution_pattern, std::string> { "make_substitution_pattern" }
									 = +(+((graph - '=' - ':' - '$') | (lit('$') >> char_('$'))) | make_placeholder);
const auto make_macro                = x3::rule<class make_macro, macro_ast> { "make_macro" }
									 = lexeme[+(graph - '=')] >> lexeme[*macro_mod_symbol >> "="] >> *(*lit('\t') >> lexeme[+(char_ - blank - eol - '\\' - '#')] >> *lit('\t'));
const auto make_target               = x3::rule<class make_target, std::string> { "make_target" }
									 = *lit('\t') >> lexeme[make_substitution_pattern] >> *lit('\t');
const auto make_special_target       = x3::rule<class make_special_target, special_target_type> { "make_special_target" }
									 = lexeme[lit('.') >> special_target_symbol];
const auto make_command              = x3::rule<class make_command, make_command_ast> { "make_command" }
									 = lit('\t') >> lexeme[*command_mod_symbol >> +(char_-eol)];
const auto make_rule                 = x3::rule<class make_rule, make_rule_ast> { "make_rule" }
									 = -make_special_target[add_special_target] >> *make_target[add_target] >> ":" >> *make_target[add_source] >> *(eol >> make_command[add_command]);
const auto make_makefile             = x3::rule<class make_makefile, makefile_ast> { "makefile" }
									 = *eol >> (make_macro[add_macro] | make_rule[add_rule]) % eol;

std::string make_subst(const Environment& env, const std::string& input, bool) {
	std::string result;
	bool match = boost::spirit::x3::parse(input.begin(), input.end(),
		boost::spirit::x3::with<env_tag>(std::ref(env))[*(make_substitution_pattern|+(blank | char_(':') | char_('=')))],
		result
	);
	assert(match);
	return result;
}

class automatic_variable : public Variable
{
public:
	typedef std::list<std::string> (*eval_auto_variable)(const Task& task);

private:
	const Task& task_;
	eval_auto_variable eval_;

	public:
	automatic_variable(eval_auto_variable eval, const Task& task) : task_(task), eval_(eval) {}
	std::string to_string() const { return boost::algorithm::join(eval_(task_), " "); }
	std::list<std::string> to_string_list() const { return eval_(task_); }
	pointer clone() const { return pointer(new automatic_variable(eval_, task_)); }
};

std::list<std::string> expand_targets(const Task& task)
{
	std::list<std::string> result;
	auto targets = task.targets();
	for(auto target : targets) {
		result.push_back(properties(target).name());
	}
	return result;
}

std::list<std::string> expand_source(const Task& task)
{
	assert(task.sources().size() >= 1);
	return { properties(task.sources()[0]).name() };
}

std::list<std::string> expand_sources(const Task& task)
{
	std::list<std::string> result;
	auto sources = task.sources();
	for(auto source : sources) {
		result.push_back(properties(source).name());
	}
	return result;
}

void setup_make_task_context(Environment& env, const Task& task)
{
	env["@"] = std::make_shared<automatic_variable>(expand_targets, task);
	env["<"] = std::make_shared<automatic_variable>(expand_source, task);
	env["^"] = std::make_shared<automatic_variable>(expand_sources, task);
}

void run_makefile(const std::string& makefile_path, std::vector<std::pair<std::string, std::string>> overrides)
{
	std::ifstream ifs{makefile_path};
	ifs.unsetf(std::ios::skipws);
	boost::spirit::istream_iterator iter{ifs}, i_end;

	static makefile_ast makefile;
	auto& env = *(makefile.env);
	for(auto override : overrides) env[override.first] = make_variable(override.second);
	bool match = phrase_parse(iter, i_end, boost::spirit::x3::with<env_tag>(std::ref(*(makefile.env)))[make_makefile],
		blank - '\t' | make_blank_line | make_comment | (lit('\\') >> eol),
		makefile);
	assert(match);

	auto makefile_node = add_entry_indeterminate(makefile_path);
	if(out_degree(makefile_node, sconspp::graph) > 0) { // there is a way to build the Makefile so we better update it
		auto task = properties(makefile_node).task();
		if(task)
			task->decider = &Task::timestamp_pure_decider;
		bool always_build_saved = always_build;
		always_build = false; // Prevent infinite loop
		if(sconspp::build(makefile_node) > 0) {
			// Makefile was rebuilt so we need to start over
			throw restart_exception();
		}
		always_build = always_build_saved;
	}
}

}
}
