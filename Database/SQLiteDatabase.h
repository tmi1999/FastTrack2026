#pragma once
#include "database.h"
#include <sqlite3.h>

class SQLiteDatabase : public Database {
public:
    SQLiteDatabase(const std::string& path);
    ~SQLiteDatabase() override;

    bool connect() override;
    void disconnect() override;

    bool execute(const std::string& query) override;

    bool isConnected() const override;

private:
    std::string path;
    sqlite3* db = nullptr;
};