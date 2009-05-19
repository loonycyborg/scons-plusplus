#define BOOST_TEST_MODULE scons_plus_plus regression tests
#include <boost/test/unit_test.hpp>

#include "python_interface/python_interface.hpp"
#include "python_interface/python_interface_internal.hpp"
#include "test_utils.hpp"

struct python_setup
{
	python_setup()
	{
		python_interface::init_python();
	}
};

BOOST_GLOBAL_FIXTURE(python_setup);

struct sconspp_fixture
{
	object ns;
	sconspp_fixture()
	{
		ns = python_interface::copy(main_namespace);
	}
};

BOOST_FIXTURE_TEST_SUITE(Environment, sconspp_fixture)
BOOST_AUTO_TEST_CASE(test_variable_access)
{
	SCONSPP_EXEC("env = Environment()");
	SCONSPP_EXEC("env['blah'] = 'x'");
	SCONSPP_CHECK("env['blah'] == 'x'");
	SCONSPP_CHECK_THROW("env['_non_existant']", PyExc_KeyError);
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
}
BOOST_AUTO_TEST_SUITE_END()
