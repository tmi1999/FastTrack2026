#include "SQLiteDatabase.h"

#include <iostream>

SQLiteDatabase::SQLiteDatabase(const std::string& path): path(path) {}

SQLiteDatabase::~SQLiteDatabase() {
    disconnect();
}

bool SQLiteDatabase::connect() {
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        std::cout << "SQLite connection failed: "
                  << sqlite3_errmsg(db)
                  << '\n';

        return false;
    }

    std::cout << "Connected to SQLite: "
              << path
              << '\n';

    return true;
}

void SQLiteDatabase::disconnect() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

bool SQLiteDatabase::execute(const std::string& query) {
    if (!db) {
        return false;
    }

    char* errorMessage = nullptr;

    int result = sqlite3_exec(
        db,
        query.c_str(),
        nullptr,
        nullptr,
        &errorMessage
    );

    if (result != SQLITE_OK) {
        std::cout << "SQLite error: "
                  << errorMessage
                  << '\n';

        sqlite3_free(errorMessage);

        return false;
    }

    return true;
}

bool SQLiteDatabase::isConnected() const {
    return db != nullptr;
}