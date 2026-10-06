#include "DatabaseFactory.h"

#include "SQLiteDatabase.h"
#include "PostgresDatabase.h"

std::unique_ptr<Database> DatabaseFactory::create(DatabaseConfig& config) {

    if (config.type == DatabaseType::SQLite) {
        return std::make_unique<SQLiteDatabase>(config.sqlite.path);
    }

    if (config.type == DatabaseType::PostgreSQL) {
        std::string conStr = "host=" + config.postgres.host
                            + " port=" + std::to_string(config.postgres.port)
                            + " dbname=" + config.postgres.database
                            + " user=" + config.postgres.user
                            + " password=" + config.postgres.password;

        return std::make_unique<PostgresDatabase>(conStr);
    }
}