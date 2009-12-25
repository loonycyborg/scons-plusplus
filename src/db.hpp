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

#ifndef DB_HPP
#define DB_HPP

#include <boost/optional.hpp>
#include <boost/utility.hpp>
#include <boost/array.hpp>

#include "dependency_graph.hpp"

namespace SQLite
{
class Db;
}

namespace db
{

class PersistentNodeData : public boost::noncopyable
{
	boost::optional<int> id_;
	std::string type_;
	std::string name_;
	boost::optional<bool> existed_;
	boost::optional<time_t> timestamp_;
	boost::optional<boost::array<unsigned char, 16> > signature_;

	SQLite::Db& db;
	dependency_graph::Node node;

	public:
	PersistentNodeData();
	PersistentNodeData(SQLite::Db& db, dependency_graph::Node node);
	~PersistentNodeData();

	boost::optional<bool> existed() const { return existed_; }
	boost::optional<time_t> timestamp() const { return timestamp_; }
	boost::optional<boost::array<unsigned char, 16> > signature() const { return signature_; }
	boost::optional<bool>& existed() { return existed_; }
	boost::optional<time_t>& timestamp() { return timestamp_; }
	boost::optional<boost::array<unsigned char, 16> >& signature() { return signature_; }
};

boost::shared_ptr<SQLite::Db> init_db(const std::string& filename);

}

#endif
