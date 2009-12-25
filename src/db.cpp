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

#include "db.hpp"

#include <sqlite3.h>

namespace SQLite
{
	
template<typename T> struct sqlite3_column_impl;
template<typename T> struct sqlite3_column_impl<boost::optional<T> >
{
	static boost::optional<T> sqlite3_column(sqlite3_stmt* stmt, int i)
	{
		if(sqlite3_column_type(stmt, i) == SQLITE_NULL)
			return boost::optional<T>();
		else
			return sqlite3_column_impl<T>::sqlite3_column(stmt, i);
	}
};
template<> struct sqlite3_column_impl<int>
{
	static int sqlite3_column(sqlite3_stmt* stmt, int i)
	{
		return sqlite3_column_int(stmt, i);
	}
};
template<> struct sqlite3_column_impl<bool>
{
	static bool sqlite3_column(sqlite3_stmt* stmt, int i)
	{
		return sqlite3_column_int(stmt, i);
	}
};
template<> struct sqlite3_column_impl<time_t>
{
	static time_t sqlite3_column(sqlite3_stmt* stmt, int i)
	{
		return sqlite3_column_int64(stmt, i);
	}
};

template<typename T> inline T sqlite3_column(sqlite3_stmt* stmt, int i)
{
	return sqlite3_column_impl<T>::sqlite3_column(stmt, i);
}

inline void sqlite3_bind(sqlite3_stmt* stmt, int i, int val)
{
	sqlite3_bind_int(stmt, i, val);
}
inline void sqlite3_bind(sqlite3_stmt* stmt, int i, time_t val)
{
	sqlite3_bind_int64(stmt, i, val);
}
inline void sqlite3_bind(sqlite3_stmt* stmt, int i, const std::string& val)
{
	sqlite3_bind_text(stmt, i, val.c_str(), val.size()+1, SQLITE_TRANSIENT);
}
template<typename T> inline void sqlite3_bind(sqlite3_stmt* stmt, int i, boost::optional<T> val)
{
	if(val)
		sqlite3_bind(stmt, i, val.get());
	else
		sqlite3_bind_null(stmt, i);
}

class Statement : public boost::noncopyable
{
	sqlite3_stmt* statement;
	sqlite3* db;
	public:
	Statement(sqlite3* db, const std::string& sql) : db(db)
	{
		int result = sqlite3_prepare_v2(db, sql.c_str(), sql.size() + 1, &statement, 0);
		if(result != SQLITE_OK) {
			throw std::runtime_error(std::string("sqlite error when preparing statement: \n" + sql + "\n: " +  sqlite3_errmsg(db)));
		}
	}
	~Statement()
	{
		sqlite3_finalize(statement);
	}


	int step()
	{
		int result = sqlite3_step(statement);
		if(result == SQLITE_ROW || result == SQLITE_DONE)
			return result;
		throw std::runtime_error(std::string("sqlite error when evaluating statement: ") + sqlite3_errmsg(db));
	}
	void reset()
	{
		sqlite3_reset(statement);
	}
	int column_type(int i)
	{
		return sqlite3_column_type(statement, i);
	}
	template<typename T> T column(int i)
	{
		return sqlite3_column<T>(statement, i);
	}
	template<typename T> void bind(int i, T val)
	{
		sqlite3_bind(statement, i, val);
	}


	template<typename T> std::vector<T> exec()
	{
		std::vector<T> result;
		while(step() == SQLITE_ROW) {
			result.push_back(column<T>(0));
		}
		reset();
		return result;
	}
	template<typename T, typename T1> std::vector<T> exec(T1 val)
	{
		bind(1, val);
		return exec<T>();
	}
	template<typename T, typename T1, typename T2> std::vector<T> exec(T1 val1, T2 val2)
	{
		bind(1, val1);
		bind(2, val2);
		return exec<T>();
	}
};

class Db : public boost::noncopyable
{
	sqlite3* db;
	public:
	explicit Db(const std::string& filename)
	{
		int result = sqlite3_open(filename.c_str(), &db);
		if(result != SQLITE_OK) {
			throw std::runtime_error(std::string("sqlite error when opening ") + filename + ": " + sqlite3_errmsg(db));
		}
	}
	~Db()
	{
		sqlite3_close(db);
	}
	sqlite3* handle() const { return db; }
	void exec(const std::string& sql)
	{
		Statement stmt(db, sql);
		while(stmt.step() != SQLITE_DONE) {}
	}
};

}

namespace db
{

boost::shared_ptr<SQLite::Db> init_db(const std::string& filename)
{
	boost::shared_ptr<SQLite::Db> db(new SQLite::Db(filename));
	db->exec("create table if not exists nodes "
		"(id INTEGER PRIMARY KEY, type TEXT, name TEXT, existed INTEGER, timestamp INTEGER, signature BLOB)");
	db->exec("create unique index if not exists node_name_index on nodes (type, name)");
	return db;
}

PersistentNodeData::PersistentNodeData(SQLite::Db& db, dependency_graph::Node node) : db(db), node(node)
{
	SQLite::Statement read_data(db.handle(), 
		"select existed, timestamp, signature from nodes where type == ?1 and name == ?2");
	type_ = dependency_graph::graph[node]->type();
	name_ = dependency_graph::graph[node]->name();
	read_data.bind(1, type_);
	read_data.bind(2, name_);
	if(read_data.step() == SQLITE_ROW) {
		existed_ = read_data.column<boost::optional<bool> >(0);
		timestamp_ = read_data.column<boost::optional<int> >(1);
	}
}

PersistentNodeData::~PersistentNodeData()
{
	try {
		dependency_graph::graph[node]->record_persistent_data(*this);
		SQLite::Statement write_data(db.handle(), 
			"insert or replace into nodes (type, name, existed, timestamp) values (?1, ?2, ?3, ?4)");
		write_data.bind(1, type_);
		write_data.bind(2, name_);
		write_data.bind(3, existed_);
		write_data.bind(4, timestamp_);
		while(write_data.step() != SQLITE_DONE) {}
	} catch(const std::exception& e) {
		std::cout << "An exception occured when recording node " << type_ << "::" << name_ << ": " << e.what() << std::endl;
	}
}

}
