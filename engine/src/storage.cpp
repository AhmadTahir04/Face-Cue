#include "ffa/storage.hpp"
#include <sqlite3.h>
#include <stdexcept>
#include <cstring>
#include <filesystem>

namespace ffa {

Storage::Storage(const std::string& dbPath) {
    // Ensure the parent directory exists so sqlite can create the file.
    std::filesystem::path p(dbPath);
    if (p.has_parent_path())
        std::filesystem::create_directories(p.parent_path());

    if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK)
        throw std::runtime_error("Cannot open DB: " + dbPath);
    exec("PRAGMA foreign_keys = ON;");
    initSchema();
}

Storage::~Storage() {
    if (db_) sqlite3_close(db_);
}

void Storage::exec(const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown";
        sqlite3_free(err);
        throw std::runtime_error("SQL error: " + msg);
    }
}

void Storage::initSchema() {
    exec("CREATE TABLE IF NOT EXISTS people ("
         " id INTEGER PRIMARY KEY AUTOINCREMENT,"
         " name TEXT UNIQUE NOT NULL,"
         " reminder TEXT DEFAULT '',"
         " created_at TEXT DEFAULT CURRENT_TIMESTAMP);");
    exec("CREATE TABLE IF NOT EXISTS embeddings ("
         " id INTEGER PRIMARY KEY AUTOINCREMENT,"
         " person_id INTEGER NOT NULL,"
         " vec BLOB NOT NULL,"
         " created_at TEXT DEFAULT CURRENT_TIMESTAMP,"
         " FOREIGN KEY(person_id) REFERENCES people(id) ON DELETE CASCADE);");
}

int64_t Storage::upsertPerson(const std::string& name, const std::string& reminder) {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "INSERT INTO people(name, reminder) VALUES(?,?) "
        "ON CONFLICT(name) DO UPDATE SET reminder=excluded.reminder;",
        -1, &st, nullptr);
    sqlite3_bind_text(st, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, reminder.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);

    sqlite3_prepare_v2(db_, "SELECT id FROM people WHERE name=?;", -1, &st, nullptr);
    sqlite3_bind_text(st, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    int64_t id = 0;
    if (sqlite3_step(st) == SQLITE_ROW) id = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return id;
}

void Storage::addEmbedding(int64_t personId, const cv::Mat& vec) {
    cv::Mat v;
    vec.convertTo(v, CV_32F);
    if (!v.isContinuous()) v = v.clone();

    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "INSERT INTO embeddings(person_id, vec) VALUES(?,?);", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, personId);
    sqlite3_bind_blob(st, 2, v.data,
                      static_cast<int>(v.total() * v.elemSize()), SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

std::vector<Person> Storage::listPeople() {
    std::vector<Person> out;
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT id, name, reminder FROM people ORDER BY name;", -1, &st, nullptr);
    while (sqlite3_step(st) == SQLITE_ROW) {
        Person p;
        p.id       = sqlite3_column_int64(st, 0);
        p.name     = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
        const unsigned char* rem = sqlite3_column_text(st, 2);
        p.reminder = rem ? reinterpret_cast<const char*>(rem) : "";
        out.push_back(std::move(p));
    }
    sqlite3_finalize(st);
    return out;
}

std::vector<StoredEmbedding> Storage::allEmbeddings() {
    std::vector<StoredEmbedding> out;
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT person_id, vec FROM embeddings;", -1, &st, nullptr);
    while (sqlite3_step(st) == SQLITE_ROW) {
        StoredEmbedding e;
        e.personId    = sqlite3_column_int64(st, 0);
        const void* blob = sqlite3_column_blob(st, 1);
        int bytes        = sqlite3_column_bytes(st, 1);
        int floats       = bytes / static_cast<int>(sizeof(float));
        cv::Mat v(1, floats, CV_32F);
        std::memcpy(v.data, blob, bytes);
        e.vec = v.clone();
        out.push_back(std::move(e));
    }
    sqlite3_finalize(st);
    return out;
}

bool Storage::deletePersonByName(const std::string& name) {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_, "DELETE FROM people WHERE name=?;", -1, &st, nullptr);
    sqlite3_bind_text(st, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
    return sqlite3_changes(db_) > 0;  // embeddings cascade-delete
}

void Storage::deleteAll() {
    exec("DELETE FROM embeddings; DELETE FROM people;");
}

}  // namespace ffa
