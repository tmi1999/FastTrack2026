#pragma once

#include "database.h"

#include <memory>

class DatabaseFactory {
public:
    static std::unique_ptr<Database> create(DatabaseConfig& config);
};