/***************************************************************************
 *   Copyright (C) 2009 by Sergey Popov                                    *
 *   loonycyborg@gmail.com                                                 *
 *                                                                         *
 *  This file is part of SCons++.                                          *
 *                                                                         *
 *  SCons++ is free software; you can redistribute it and/or modify        *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation; either version 3 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  SCons++ is distributed in the hope that it will be useful,             *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#ifndef BUILDER_WRAPPER_HPP
#define BUILDER_WRAPPER_HPP

#include "node_wrapper.hpp"
#include "builder.hpp"
#include "dependency_graph.hpp"
#include "environment.hpp"

#include "action_wrapper.hpp"

using namespace boost::python;

namespace sconspp
{
namespace python_interface
{

NodeList call_builder(const Builder&, const Environment&, object, object);
NodeList call_builder_interface(tuple args, dict kw);

object make_builder(const tuple&, const dict&);

object add_action(Builder* builder, object, object);
object add_emitter(Builder* builder, object, object);

object get_builder_suffix(Builder* builder);
object get_builder_prefix(Builder* builder);
object get_builder_src_suffix(Builder* builder);
}
}

#endif
