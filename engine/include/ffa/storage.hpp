#pragma once
#include <opencv2/core.hpp>
#include <string>
#include <vector>
#include <cstdint>

struct sqlite3;  // forward-declared to keep sqlite out of the public header

namespace ffa {

struct Person {
    int64_t     id = 0;
    std::string name;
    std::string reminder;
};

// One enrolled embedding belonging to a person.
struct StoredEmbedding {
    int64_t personId = 0;
    cv::Mat vec;  // 1x128 CV_32F
};

// Local SQLite storage for enrolled people and their face embeddings.
// This is sensitive biometric data — see docs/PRIVACY.md.
class Storage {
public:
    explicit Storage(const std::string& dbPath);
    ~Storage();

    // Create person if new (by name), return their id.
    int64_t upsertPerson(const std::string& name, const std::string& reminder);

    void addEmbedding(int64_t personId, const cv::Mat& vec);

    std::vector<Person>          listPeople();
    std::vector<StoredEmbedding> allEmbeddings();

    // Deletion that actually deletes.
    bool deletePersonByName(const std::string& name);  // delete-one
    void deleteAll();                                   // delete-all

private:
    sqlite3* db_ = nullptr;
    void exec(const char* sql);
    void initSchema();
};

}  // namespace ffa
