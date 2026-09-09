/*
 * Bookmarks - shared persistence model for the editor's viewer bookmarks.
 *
 * A bookmark is a named (mapId, X, Y, Z) viewpoint that the operator can
 * jump back to from the View -> Bookmarks menu or the Find&Jump dialog.
 *
 * Persisted in QSettings under "bookmarks/N/{name, mapId, x, y, z,
 * folder, tags}".  Legacy lists (pre-folder) used the key "label" instead
 * of "name"; loadAll() reads either so existing operator state survives
 * the upgrade.
 *
 * `folder` is a free-form string the manager dialog groups by.  Empty
 * folder is treated as the implicit "Quick" group.  `tags` is a
 * comma-separated string that the manager dialog's filter line searches
 * against (in addition to `name`).
 */

#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include <cstdint>

namespace world_editor::app
{

struct Bookmark
{
    QString  name;
    QString  folder;
    QString  tags;
    uint32_t mapId = 0;
    float    x     = 0.0f;
    float    y     = 0.0f;
    float    z     = 0.0f;
};

namespace bookmarks
{
// Read every persisted bookmark from QSettings in order.
QVector<Bookmark> loadAll();

// Replace the entire persisted list with `items` (in order).  All callers
// snapshot, mutate, then re-save the whole list so partial writes can't
// leave the array in a half-updated state.
void saveAll(QVector<Bookmark> const& items);

// "Tag1, Tag2, Tag3" -> ["Tag1", "Tag2", "Tag3"] (trimmed, empties dropped).
QStringList splitTags(QString const& tagsCsv);

// Pretty action title:  "<name>  [tag1, tag2]"  (no brackets when no tags).
QString actionTitleFor(Bookmark const& b);

// Case-insensitive substring match against name OR any tag.  Empty
// `needle` matches everything.
bool matchesFilter(Bookmark const& b, QString const& needle);
} // namespace bookmarks

} // namespace world_editor::app
