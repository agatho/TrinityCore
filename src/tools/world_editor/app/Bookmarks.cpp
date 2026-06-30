#include "Bookmarks.h"

#include <QSettings>

namespace world_editor::app::bookmarks
{

QVector<Bookmark> loadAll()
{
    QVector<Bookmark> out;
    QSettings settings;
    int const size = settings.beginReadArray(QStringLiteral("bookmarks"));
    out.reserve(size);
    for (int i = 0; i < size; ++i)
    {
        settings.setArrayIndex(i);
        Bookmark b;
        // Legacy lists wrote "label"; new format writes "name".  Fall
        // back to "label" so existing operator state survives upgrade.
        b.name   = settings.value(QStringLiteral("name")).toString();
        if (b.name.isEmpty())
            b.name = settings.value(QStringLiteral("label")).toString();
        b.folder = settings.value(QStringLiteral("folder")).toString();
        b.tags   = settings.value(QStringLiteral("tags")).toString();
        b.mapId  = settings.value(QStringLiteral("mapId")).toUInt();
        b.x      = settings.value(QStringLiteral("x")).toFloat();
        b.y      = settings.value(QStringLiteral("y")).toFloat();
        b.z      = settings.value(QStringLiteral("z")).toFloat();
        out.push_back(b);
    }
    settings.endArray();
    return out;
}

void saveAll(QVector<Bookmark> const& items)
{
    QSettings settings;
    // beginWriteArray truncates the previously-persisted array to the
    // new size, so we don't have to clean up stale higher-index slots.
    settings.beginWriteArray(QStringLiteral("bookmarks"), items.size());
    for (int i = 0; i < items.size(); ++i)
    {
        settings.setArrayIndex(i);
        Bookmark const& b = items.at(i);
        settings.setValue(QStringLiteral("name"),   b.name);
        settings.setValue(QStringLiteral("folder"), b.folder);
        settings.setValue(QStringLiteral("tags"),   b.tags);
        settings.setValue(QStringLiteral("mapId"),  b.mapId);
        settings.setValue(QStringLiteral("x"),      b.x);
        settings.setValue(QStringLiteral("y"),      b.y);
        settings.setValue(QStringLiteral("z"),      b.z);
        // Clear the legacy key so a downgraded reader doesn't see two.
        settings.remove(QStringLiteral("label"));
    }
    settings.endArray();
}

QStringList splitTags(QString const& tagsCsv)
{
    QStringList parts = tagsCsv.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (QString& p : parts)
        p = p.trimmed();
    parts.removeAll(QString{});
    return parts;
}

QString actionTitleFor(Bookmark const& b)
{
    QStringList const tagList = splitTags(b.tags);
    if (tagList.isEmpty())
        return b.name;
    return QStringLiteral("%1  [%2]").arg(b.name, tagList.join(QStringLiteral(", ")));
}

bool matchesFilter(Bookmark const& b, QString const& needle)
{
    QString const n = needle.trimmed();
    if (n.isEmpty())
        return true;
    if (b.name.contains(n, Qt::CaseInsensitive))
        return true;
    for (QString const& tag : splitTags(b.tags))
        if (tag.contains(n, Qt::CaseInsensitive))
            return true;
    return false;
}

} // namespace world_editor::app::bookmarks
