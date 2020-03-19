#pragma once

#include "environment.hpp"

namespace sconspp
{

class Task;

namespace make_interface
{
	std::string make_subst(const Environment&, const std::string& input, bool);
	void setup_make_task_context(Environment&, const Task&);
	void run_makefile(const std::string&, std::vector<std::string> command_line_target_strings, std::vector<std::pair<std::string, std::string> > overrides);

}
}
