#define BOOST_TEST_MODULE scons_plus_plus regression tests
#include "test_common.hpp"
#include "python_interface/node_wrapper.hpp"
#include <boost/graph/graph_utility.hpp>

namespace sconspp
{
namespace python_interface
{

BOOST_FIXTURE_TEST_SUITE(Environment, sconspp_fixture)
BOOST_AUTO_TEST_CASE(test_variable_access)
{
	SCONSPP_EXEC("env = Environment()");
	SCONSPP_EXEC("env['blah'] = 'x'");
	SCONSPP_CHECK("env['blah'] == 'x'");
	SCONSPP_CHECK("env.get('blah') == 'x'");
	SCONSPP_CHECK_THROW("env['_non_existant']", PyExc_KeyError);
	SCONSPP_CHECK("env.get('_non_existant') == None");
	SCONSPP_EXEC("env['blah'] = [1,2,3]");
	SCONSPP_CHECK("env['blah'][1] == 2");
	SCONSPP_EXEC("env['blah'][1] = 5");
	SCONSPP_CHECK("env['blah'][1] == 5");
	SCONSPP_EXEC("del env['blah']");
	SCONSPP_CHECK_THROW("env['blah']", PyExc_KeyError);
	SCONSPP_CHECK("env.get('blah') == None");
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
	SCONSPP_CHECK("env.subst('$varname') == '1 2 3'");
	SCONSPP_CHECK("env.subst('foo $( foo $) foo') == 'foo  foo  foo'");
	SCONSPP_CHECK("env.subst('foo $( foo $) foo', True) == 'foo  foo'");
	SCONSPP_CHECK_THROW("env.subst('${foo')", PyExc_Exception);
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

BOOST_AUTO_TEST_CASE(test_depends)
{
	using boost::is_adjacent;
	SCONSPP_EXEC("target = Alias('test_depends_target')");
	SCONSPP_EXEC("dependency = Alias('test_depends_dependency')");
	SCONSPP_EXEC("Depends(target, dependency)");
	Node 
		target = python_interface::extract_node(ns["target"][0]),
		dependency = python_interface::extract_node(ns["dependency"][0]);
	BOOST_CHECK(is_adjacent(graph, target, dependency));
}

BOOST_AUTO_TEST_CASE(test_glob)
{
	SCONSPP_EXEC("env = Environment()");
	SCONSPP_EXEC("f1 = env.File('test_glob_dir/file1.ext')");
	SCONSPP_EXEC("glob_files = Glob('test_glob_dir/*.ext', ondisk = False)");
	SCONSPP_CHECK("glob_files[0].path == 'test_glob_dir/file1.ext'");
	SCONSPP_EXEC("f2 = env.File('test_glob_dir/file2.ext')");
	SCONSPP_EXEC("glob_files = Glob('test_glob_dir/*.ext', ondisk = False)");
	SCONSPP_CHECK("glob_files[0].path == 'test_glob_dir/file1.ext'");
	SCONSPP_CHECK("glob_files[1].path == 'test_glob_dir/file2.ext'");
	SCONSPP_EXEC("from tempfile import NamedTemporaryFile");
	SCONSPP_EXEC("from os.path import dirname");
	SCONSPP_EXEC("tmpfile = NamedTemporaryFile(prefix = 'test_glob_ondisk', suffix = '.ext')");
	SCONSPP_EXEC("tmpfile.write('foo')");
	SCONSPP_EXEC("glob_file = Glob(dirname(tmpfile.name) + '/test_glob_ondisk*.ext')");
	SCONSPP_CHECK("glob_file[0].path == tmpfile.name");
}

BOOST_AUTO_TEST_CASE(test_node_get_contents)
{
	SCONSPP_EXEC("env = Environment()");
	SCONSPP_EXEC("from tempfile import NamedTemporaryFile");
	SCONSPP_EXEC("tmpfile = NamedTemporaryFile(prefix = 'test_node_get_contents', suffix = '.ext')");
	SCONSPP_EXEC("tmpfile.write('bar')");
	SCONSPP_EXEC("tmpfile.flush()");
	SCONSPP_CHECK("env.File(tmpfile.name).get_contents() == 'bar'");
}

BOOST_AUTO_TEST_CASE(test_find_file)
{
	SCONSPP_EXEC("from tempfile import NamedTemporaryFile");
	SCONSPP_EXEC("from os.path import basename, dirname");
	SCONSPP_EXEC("tmpfile = NamedTemporaryFile(prefix = 'test_find_file', suffix = '.ext')");
	SCONSPP_EXEC("tmpfile.write('baz')");
	SCONSPP_EXEC("tmpfile.flush()");
	SCONSPP_CHECK("FindFile(basename(tmpfile.name), dirname(tmpfile.name)).abspath == tmpfile.name");
	SCONSPP_CHECK("FindFile('nonexistant_file_name', dirname(tmpfile.name)) == None");
}

BOOST_AUTO_TEST_SUITE_END()

}
}
