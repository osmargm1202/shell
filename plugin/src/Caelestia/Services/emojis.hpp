#pragma once

#include <qhash.h>
#include <qqmlintegration.h>
#include <qvariant.h>

namespace caelestia::services {

class EmojisCore : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(QString sourcePath READ sourcePath WRITE setSourcePath NOTIFY sourcePathChanged)
    Q_PROPERTY(QVariantList favouriteEmojis READ favouriteEmojis WRITE setFavouriteEmojis NOTIFY favouriteEmojisChanged)

public:
    explicit EmojisCore(QObject* parent = nullptr);

    [[nodiscard]] QVariantList items() const;
    [[nodiscard]] QString sourcePath() const;
    void setSourcePath(const QString& sourcePath);
    [[nodiscard]] QVariantList favouriteEmojis() const;
    void setFavouriteEmojis(const QVariantList& favouriteEmojis);

    Q_INVOKABLE void reload();
    Q_INVOKABLE [[nodiscard]] QVariantList getSortedItems() const;
    Q_INVOKABLE void recordUsage(const QString& charStr);

signals:
    void itemsChanged();
    void sourcePathChanged();
    void favouriteEmojisChanged();

private:
    void loadFrequencies();
    void saveFrequencies() const;
    [[nodiscard]] static QString frequenciesPath();

    QVariantList m_items;
    QVariantList m_favouriteEmojis;
    QString m_sourcePath;
    QHash<QString, int> m_frequencies;
    bool m_loaded = false;
};

} // namespace caelestia::services
