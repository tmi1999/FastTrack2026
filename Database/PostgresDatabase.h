#pragma once

#include "database.h"

#include <memory>
#include <pqxx/pqxx>

class PostgresDatabase : public Database {
public:
    PostgresDatabase(
        const std::string& connectionString
    );

    ~PostgresDatabase() override;

    bool connect() override;
    void disconnect() override;

    bool execute(const std::string& query) override;

    bool isConnected() const override;

private:
    std::string connectionString;

    std::unique_ptr<pqxx::connection> connection;
};