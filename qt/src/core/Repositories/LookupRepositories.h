// Port of LookupRepositories.swift — Principles and Languages (game-structure
// lookups, not playthrough-scoped).
#pragma once

#include "Models.h"
#include "SQLiteDatabase.h"

namespace boh {

class PrincipleRepository {
public:
    explicit PrincipleRepository(SQLiteDatabase& db) : m_db(db) {}

    std::vector<Principle> all() const;
    std::optional<Principle> get(qint64 id) const;
    void update(const Principle& principle) const;

    static Principle mapRow(const Row& row);

private:
    SQLiteDatabase& m_db;
};

class LanguageRepository {
public:
    explicit LanguageRepository(SQLiteDatabase& db) : m_db(db) {}

    std::vector<Language> all() const;
    std::optional<Language> get(qint64 id) const;
    Language insert(const QString& name, bool native, std::optional<QString> notes = {}) const;
    void update(const Language& language) const;
    void remove(qint64 id) const;

    static Language mapRow(const Row& row);

private:
    SQLiteDatabase& m_db;
};

} // namespace boh
