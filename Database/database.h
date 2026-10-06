#pragma once

#include <string>

class Database {
public:
    virtual ~Database() = default;

    virtual bool connect() = 0;
    virtual void disconnect() = 0;

    virtual bool execute(const std::string& query) = 0;

    virtual bool isConnected() const = 0;
};

enum class DatabaseType {
    SQLite,
    PostgreSQL
};

struct SQLiteConfig {
    std::string path;
};

struct PostgresConfig {
    std::string host;
    int port = 5432;
    std::string database;
    std::string user;
    std::string password;
};

struct DatabaseConfig {
    DatabaseType type;

    SQLiteConfig sqlite;
    PostgresConfig postgres;
};


