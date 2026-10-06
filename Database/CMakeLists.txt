cmake_minimum_required (VERSION 3.10)

project(DB)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")


# CPM
file(
        DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.40.8/CPM.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
        EXPECTED_HASH SHA256=78ba32abdf798bc616bab7c73aac32a17bbd7b06ad9e26a6add69de8f3ae4791
)
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

CPMAddPackage(
    NAME SQLiteCpp
    GITHUB_REPOSITORY SRombauts/SQLiteCpp
    GIT_TAG 3.3.3
    OPTIONS
        "SQLITECPP_INTERNAL_SQLITE ON"
        "SQLITECPP_BUILD_TESTS OFF"
        "SQLITECPP_BUILD_EXAMPLES OFF"
)
find_package(PostgreSQL REQUIRED)
CPMAddPackage(
    NAME libpqxx
    GITHUB_REPOSITORY jtv/libpqxx
    GIT_TAG 7.10.1
    OPTIONS
        "SKIP_BUILD_TEST ON"
)


add_executable(${PROJECT_NAME}
    main.cpp

    database.h

    SQLiteDatabase.cpp
    SQLiteDatabase.h

    PostgresDatabase.cpp
    PostgresDatabase.h

    DatabaseFactory.cpp
    DatabaseFactory.h
)
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    sqlite3
    pqxx
    SQLite::SQLite3
    PostgreSQL::PostgreSQL
)
#cmake -S . -B build  -DCMAKE_TOOLCHAIN_FILE=E:/Linux/Ubuntu/C++/vcpkg/scripts/buildsystems/vcpkg.cmake


# set(LIBS)

# SDL
# CPMAddPackage("gh:libsdl-org/SDL#58702e5")
# list(APPEND LIBS SDL3::SDL3)

# add_library(LIBS STATIC net.cpp net.h)
# add_executable(server main_server.cpp)
# add_executable(client main_client.cpp)
# target_link_libraries(server PRIVATE LIBS)
# target_link_libraries(client PRIVATE LIBS)

# Copy texture resources
# add_custom_command(
        # TARGET ${PROJECT_NAME}
        # POST_BUILD

        # COMMAND ${CMAKE_COMMAND} -E copy_directory
        # "${CMAKE_CURRENT_SOURCE_DIR}/Resources"
        # "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Resources"
# )