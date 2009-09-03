#define BOOST_TEST_MODULE scons_plus_plus regression tests
#include "test_common.hpp"

BOOST_FIXTURE_TEST_SUITE(Environment, sconspp_fixture)
BOOST_AUTO_TEST_CASE(test_variable_access)
{
	SCONSPP_EXEC("env = Environment()");
	SCONSPP_EXEC("env['blah'] = 'x'");
	SCONSPP_CHECK("env['blah'] == 'x'");
	SCONSPP_CHECK_THROW("env['_non_existant']", PyExc_KeyError);
	SCONSPP_EXEC("env['blah'] = [1,2,3]");
	SCONSPP_CHECK("env['blah'][1] == 2");
	SCONSPP_EXEC("env['blah'][1] = 5");
	SCONSPP_CHECK("env['blah'][1] == 5");
	SCONSPP_EXEC("del env['blah']");
	SCONSPP_CHECK_THROW("env['blah']", PyExc_KeyError);
}
BOOST_AUTO_TEST_CASE(test_substitution)
{
	SCONSPP_EXEC("env = Environment()");
	SCONSPP_CHECK("env.subst('blah') == 'blah'");
	SCONSPP_CHECK("env.subst('$_nonexistant') == ''");
	SCONSPP_CHECK("env.subst('blah $_nonexistant blah') == 'blah  blah'");
	SCONSPP_EXEC("env['varname'] = 'foo'");
	SCONSPP_CHECK("env.subst('$varname') == 'foo'");
	SCONSPP_CHECK("env.subst('blah $varname blah') == 'blah foo blah'");
	SCONSPP_CHECK("env.subst(\"${'blah'}\") == 'blah'");
	SCONSPP_EXEC("env['varname'] = [1,2,3]");
	SCONSPP_CHECK("env.subst(\"${varname[1]}\") == '2'");
}
BOOST_AUTO_TEST_CASE(test_clone)
{
	SCONSPP_EXEC("env1 = Environment()");
	SCONSPP_EXEC("env1['foo'] = 'x'");
	SCONSPP_EXEC("env1['bar'] = [1,2,3]");
	SCONSPP_EXEC("env2 = env1.Clone()");
	SCONSPP_CHECK("env2['foo'] == 'x'");
	SCONSPP_CHECK("env2['bar'] == [1,2,3]");
	SCONSPP_EXEC("env2['foo'] = 'y'");
	SCONSPP_EXEC("env2['bar'][1] = 5");
	SCONSPP_CHECK("env1['foo'] == 'x'");
	SCONSPP_CHECK("env1['bar'] == [1,2,3]");
	SCONSPP_CHECK("env2['foo'] == 'y'");
	SCONSPP_CHECK("env2['bar'] == [1,5,3]");
}
BOOST_AUTO_TEST_CASE(test_update)
{
	SCONSPP_EXEC("env = Environment()");
	SCONSPP_EXEC("env.Replace(var1 = 'foo')");
	SCONSPP_CHECK("env['var1'] == 'foo'");
	SCONSPP_EXEC("env.Replace(var2 = 'bar', var3 = 'baz')");
	SCONSPP_CHECK("env['var2'] == 'bar'");
	SCONSPP_CHECK("env['var3'] == 'baz'");

	const char* updates[] = { "Prepend", "Append" };
	const char* uniques[] = { "Unique", "" };
	BOOST_FOREACH(const char* update, boost::make_iterator_range(updates, updates + 2)) {
		BOOST_FOREACH(const char* unique, boost::make_iterator_range(uniques, uniques + 2)) {
			std::string method = std::string(update) + unique;
			SCONSPP_EXEC("method = '" + method + "'");
			std::string var = method + "Var";
			SCONSPP_EXEC("var = '" + var + "'");
			SCONSPP_EXEC("env[var] = ['str1']");
			SCONSPP_EXEC("env." + method + "(" + var + " = 'str2')");
			if(std::string(update) == "Prepend") {
				SCONSPP_CHECK("env[var] == ['str2', 'str1']");
			} else {
				SCONSPP_CHECK("env[var] == ['str1', 'str2']");
			}
			SCONSPP_EXEC("env." + method + "(" + var + " = ['str2', 'str3'])");
			if(std::string(update) == "Prepend") {
				if(std::string(unique) == "Unique") {
					SCONSPP_CHECK("env[var] == ['str3', 'str2', 'str1']");
				} else {
					SCONSPP_CHECK("env[var] == ['str2', 'str3', 'str2', 'str1']");
				}
			} else {
				if(std::string(unique) == "Unique") {
					SCONSPP_CHECK("env[var] == ['str1', 'str2', 'str3']");
				} else {
					SCONSPP_CHECK("env[var] == ['str1', 'str2', 'str2', 'str3']");
				}
			}
			SCONSPP_EXEC("env[var] = {'a' : 'b'}");
			SCONSPP_EXEC("env." + method + "(" + var + " = {'c' : 'd'})");
			SCONSPP_CHECK("env[var] == {'a' : 'b', 'c' : 'd' }");
			SCONSPP_EXEC("env." + method + "(" + var + " = {'c' : 'x'})");
			SCONSPP_CHECK("env[var] == {'a' : 'b', 'c' : 'x' }");
		}
	}
}

BOOST_AUTO_TEST_CASE(test_add_method)
{
	SCONSPP_EXEC("env = Environment()");
	SCONSPP_EXEC("def Custom(env): return 'foo'");
	SCONSPP_EXEC("env.AddMethod(Custom)");
	SCONSPP_CHECK("env.Custom() == 'foo'");
}
BOOST_AUTO_TEST_CASE(test_helpers)
{
	SCONSPP_EXEC("foo = Split('f o o')");
	SCONSPP_CHECK("foo == ['f', 'o', 'o']");
	SCONSPP_EXEC("bar = Flatten([[1,2], 3])");
	SCONSPP_CHECK("bar == [1, 2, 3]");
	SCONSPP_EXEC("bar = Flatten([[1,2,[3,4]], [[5,6],7,[]]])");
	SCONSPP_CHECK("bar == [1, 2, 3, 4, 5, 6, 7]");
}

BOOST_AUTO_TEST_SUITE_END()
