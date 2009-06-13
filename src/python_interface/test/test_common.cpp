#include "test_common.hpp"

static void translate_error_already_set(const error_already_set&)
{
	PyErr_Print();
	PyErr_Clear();
	throw std::runtime_error("A python exception was thrown");
}

struct python_setup
{
	python_setup()
	{
		python_interface::init_python();
		boost::unit_test::unit_test_monitor.register_exception_translator<error_already_set>(&translate_error_already_set);
	}
};

BOOST_GLOBAL_FIXTURE(python_setup);
