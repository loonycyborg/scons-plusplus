#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_monitor.hpp>

#include "python_interface/python_interface.hpp"
#include "python_interface/python_interface_internal.hpp"

#define SCONSPP_EXEC(code) exec(str(code), ns, ns)
#define SCONSPP_EXECFILE(name) exec_file(name, ns, ns)
#define SCONSPP_CHECK(cond) BOOST_CHECK_MESSAGE((extract<bool>(eval(cond, ns, ns))), cond)

struct python_exception_matches
{
	PyObject* type;
	python_exception_matches(PyObject* type) : type(type) {}
	bool operator()(const error_already_set&)
	{
		bool result = PyErr_ExceptionMatches(type);
		PyErr_Clear();
		return result;
	}
};
#define SCONSPP_CHECK_THROW(expr, exeption) BOOST_CHECK_EXCEPTION(eval(expr, ns, ns), error_already_set, python_exception_matches(exeption))

struct sconspp_fixture
{
	object ns;
	sconspp_fixture()
	{
		ns = python_interface::copy(main_namespace);
	}
};
