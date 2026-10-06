#include "PostgresDatabase.h"

#include <iostream>

PostgresDatabase::PostgresDatabase(const std::string& connectionString): connectionString(connectionString) {}

PostgresDatabase::~PostgresDatabase() {
    disconnect();
}

bool PostgresDatabase::connect() {
    try {
        connection =
            std::make_unique<pqxx::connection>(
                connectionString
            );

        if (!connection->is_open()) {
            return false;
        }

        std::cout << "Connected to PostgreSQL\n";

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "PostgreSQL connection failed: "
                  << e.what()
                  << '\n';

        return false;
    }
}

void PostgresDatabase::disconnect() {
    if (connection) {
        connection->close();
        connection.reset();
    }
}

bool PostgresDatabase::execute(
    const std::string& query
) {
    if (!connection || !connection->is_open()) {
        return false;
    }

    try {
        pqxx::work transaction(*connection);

        transaction.exec(query);

        transaction.commit();

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "PostgreSQL query error: "
                  << e.what()
                  << '\n';

        return false;
    }
}

bool PostgresDatabase::isConnected() const {
    return connection &&
           connection->is_open();
}