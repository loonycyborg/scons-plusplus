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

BOOST_AUTO_TEST_SUITE(Environment)
BOOST_AUTO_TEST_CASE(test_variable_access)
{
	SCONSPP_EXEC("env = Environment()");
	SCONSPP_EXEC("env['blah'] = 'x'");
	SCONSPP_CHECK("env['blah'] == 'x'");
	SCONSPP_CHECK_THROW("env['_non_existant']", PyExc_KeyError);
}
BOOST_AUTO_TEST_SUITE_END()
