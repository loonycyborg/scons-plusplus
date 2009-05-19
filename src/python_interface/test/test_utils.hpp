#define SCONSPP_EXEC(str) exec(str, ns, ns)
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
