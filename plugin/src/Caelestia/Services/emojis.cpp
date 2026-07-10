#include "emojis.hpp"

#include <algorithm>
#include <qdir.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qloggingcategory.h>
#include <qsavefile.h>
#include <qset.h>

Q_LOGGING_CATEGORY(lcEmojis, "caelestia.services.emojis", QtInfoMsg)

namespace caelestia::services {

EmojisCore::EmojisCore(QObject* parent)
    : QObject(parent) {}

QVariantList EmojisCore::items() const {
    return m_items;
}

QString EmojisCore::sourcePath() const {
    return m_sourcePath;
}

void EmojisCore::setSourcePath(const QString& sourcePath) {
    if (m_sourcePath == sourcePath) {
        return;
    }
    m_sourcePath = sourcePath;
    emit sourcePathChanged();
}

QVariantList EmojisCore::favouriteEmojis() const {
    return m_favouriteEmojis;
}

void EmojisCore::setFavouriteEmojis(const QVariantList& favouriteEmojis) {
    if (m_favouriteEmojis == favouriteEmojis) {
        return;
    }
    m_favouriteEmojis = favouriteEmojis;
    emit favouriteEmojisChanged();
}

void EmojisCore::reload() {
    // One-shot: the asset never changes within a shell run
    if (m_loaded) {
        return;
    }
    m_loaded = true;

    loadFrequencies();

    QFile file(m_sourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(lcEmojis) << "Failed to open emoji list" << m_sourcePath;
        return;
    }

    const QString text = QString::fromUtf8(file.readAll()).trimmed();

    QVariantList result;
    const QStringList lines = text.split(u'\n');
    for (const QString& line : lines) {
        if (line.isEmpty()) {
            continue;
        }

        const qsizetype spaceIdx = line.indexOf(u' ');
        if (spaceIdx < 0) {
            continue;
        }

        result << QVariantMap{
            { QStringLiteral("char"), line.left(spaceIdx) },
            { QStringLiteral("name"), line.mid(spaceIdx + 1).trimmed() },
        };
    }

    m_items = result;
    emit itemsChanged();
}

QVariantList EmojisCore::getSortedItems() const {
    if (m_items.isEmpty()) {
        return {};
    }

    QSet<QString> favourites;
    for (const QVariant& fav : m_favouriteEmojis) {
        favourites.insert(fav.toString());
    }

    QVariantList sorted = m_items;
    // Favourites first, then usage frequency desc; stable to preserve file order
    std::stable_sort(sorted.begin(), sorted.end(), [&](const QVariant& a, const QVariant& b) {
        const QString aChar = a.toMap().value(QStringLiteral("char")).toString();
        const QString bChar = b.toMap().value(QStringLiteral("char")).toString();

        const bool aFav = favourites.contains(aChar);
        const bool bFav = favourites.contains(bChar);
        if (aFav != bFav) {
            return aFav;
        }

        return m_frequencies.value(aChar, 0) > m_frequencies.value(bChar, 0);
    });
    return sorted;
}

void EmojisCore::recordUsage(const QString& charStr) {
    ++m_frequencies[charStr];
    saveFrequencies();
}

QString EmojisCore::frequenciesPath() {
    // Matches Paths.config in QML: $XDG_CONFIG_HOME (fallback ~/.config) + /caelestia
    QString base = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.config");
    }
    return base + QStringLiteral("/caelestia/emoji-frequencies.json");
}

void EmojisCore::loadFrequencies() {
    m_frequencies.clear();

    QFile file(frequenciesPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return; // missing file → empty map, same as the QML `|| echo '{}'` fallback
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return; // corrupt file → empty map
    }

    const QJsonObject obj = doc.object();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        m_frequencies.insert(it.key(), it.value().toInt());
    }
}

void EmojisCore::saveFrequencies() const {
    const QString path = frequenciesPath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        qCWarning(lcEmojis) << "Failed to create config dir for" << path;
        return;
    }

    QJsonObject obj;
    for (auto it = m_frequencies.constBegin(); it != m_frequencies.constEnd(); ++it) {
        obj.insert(it.key(), it.value());
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcEmojis) << "Failed to open" << path << "for writing";
        return;
    }
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    if (!file.commit()) {
        qCWarning(lcEmojis) << "Failed to save frequencies to" << path;
    }
}

} // namespace caelestia::services
