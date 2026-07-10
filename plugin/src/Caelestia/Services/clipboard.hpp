#pragma once

#include <qjsvalue.h>
#include <qlist.h>
#include <qprocess.h>
#include <qqmlintegration.h>
#include <qvariant.h>

namespace caelestia::services {

class ClipboardCore : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(QVariantList favouriteClips READ favouriteClips WRITE setFavouriteClips NOTIFY favouriteClipsChanged)
    Q_PROPERTY(QString imageCacheDir READ imageCacheDir CONSTANT)

public:
    explicit ClipboardCore(QObject* parent = nullptr);

    [[nodiscard]] QVariantList items() const;
    [[nodiscard]] QVariantList favouriteClips() const;
    void setFavouriteClips(const QVariantList& favouriteClips);
    [[nodiscard]] QString imageCacheDir() const;

    Q_INVOKABLE void reload();
    Q_INVOKABLE [[nodiscard]] QVariantList getSortedItems() const;
    Q_INVOKABLE [[nodiscard]] QString getImagePath(int clipId) const;
    Q_INVOKABLE void ensureImageCached(int id, QJSValue onReady = QJSValue());

signals:
    void itemsChanged();
    void favouriteClipsChanged();

private:
    void parseListOutput(const QString& output);
    void preloadImages();
    void pumpDecodeQueue();
    void startDecode(int id, bool fromQueue);

    QVariantList m_items;
    QVariantList m_favouriteClips;
    QProcess* m_fetcher;
    QList<int> m_decodeQueue;
    int m_activeDecodes = 0;
};

} // namespace caelestia::services
