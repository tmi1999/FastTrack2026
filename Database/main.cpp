#include "DatabaseFactory.h"

#include <iostream>
#include <memory>

int main() {

    DatabaseConfig config;

    config.type = DatabaseType::SQLite;
    // config.type = DatabaseType::PostgreSQL;

    config.sqlite.path = "server.db";

    config.postgres.host = "127.0.0.1";
    config.postgres.port = 5432;
    config.postgres.database = "game_server";
    config.postgres.user = "postgres";
    config.postgres.password = "123456";


    std::unique_ptr<Database> database =
        DatabaseFactory::create(config);


    if (!database->connect()) {
        std::cerr << "Database initialization failed\n";
        return 1;
    }

    if (config.type == DatabaseType::SQLite) {
         database->execute(
            "CREATE TABLE IF NOT EXISTS users ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "username TEXT NOT NULL"
            ");"
        );
    } else {
        database->execute(
            "CREATE TABLE IF NOT EXISTS users ("
            "id SERIAL PRIMARY KEY,"
            "username TEXT NOT NULL"
            ");"
        );
    }

    database->execute(
        "INSERT INTO users (username) VALUES ('foo');"
    );


    std::cout << "Server started\n";

    return 0;
}