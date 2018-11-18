#pragma once

namespace sconspp
{

class Task;

namespace make_interface
{
	std::string make_subst(const Environment&, const std::string& input, bool);
	void setup_make_task_context(Environment&, const Task&);
	void run_makefile(const std::string&, int argc, char** argv);

}
}
