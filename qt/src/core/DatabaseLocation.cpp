#include "DatabaseLocation.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QtGlobal>

namespace boh {

QString DatabaseLocation::resolvePath()
{
    const QString override = qEnvironmentVariable("BOH_DB_PATH");
    if (!override.isEmpty())
        return override;
    for (const QString& candidate : {QStringLiteral("Boh.db"), QStringLiteral("../Boh.db")}) {
        if (QFile::exists(candidate))
            return QFileInfo(candidate).absoluteFilePath();
    }
    // Packaged/dev default: $XDG_DATA_HOME (or ~/.local/share) / BoH Librarian.
    QString dataHome = qEnvironmentVariable("XDG_DATA_HOME");
    if (dataHome.isEmpty())
        dataHome = QDir::homePath() + QStringLiteral("/.local/share");
    const QString directory = dataHome + QStringLiteral("/BoH Librarian");
    if (!QDir(directory).exists())
        QDir().mkpath(directory);
    // D11: the data dir is user-private.
    QFile::setPermissions(directory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return directory + QStringLiteral("/Boh.db");
}

} // namespace boh
