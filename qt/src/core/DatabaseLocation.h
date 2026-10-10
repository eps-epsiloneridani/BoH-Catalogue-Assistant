// Port of DatabaseLocation.swift to the Linux path policy (plan Task 9, D7/D11):
// `BOH_DB_PATH` → `./Boh.db` → `../Boh.db` → `$XDG_DATA_HOME/BoH Librarian/Boh.db`
// (default `~/.local/share/…`). The data directory is created 700; the app sets
// the db file to 600 after opening.
#pragma once

#include <QString>

namespace boh {

class DatabaseLocation {
public:
    static QString resolvePath();
};

} // namespace boh
