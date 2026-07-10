#include "clipboard.hpp"

#include <qdir.h>
#include <qjsengine.h>
#include <qloggingcategory.h>
#include <qregularexpression.h>
#include <qset.h>
#include <qtimer.h>

Q_LOGGING_CATEGORY(lcClipboard, "caelestia.services.clipboard", QtInfoMsg)

namespace {

// Load-bearing path scheme: ClipItem.qml hardcodes /tmp/caelestia-clipboard/<id>.png.
const QString cacheDir = QStringLiteral("/tmp/caelestia-clipboard");

constexpr int maxConcurrentDecodes = 2;

} // namespace

namespace caelestia::services {

ClipboardCore::ClipboardCore(QObject* parent)
    : QObject(parent)
    , m_fetcher(new QProcess(this)) {
    QObject::connect(
        m_fetcher, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
            if (status != QProcess::NormalExit || exitCode != 0) {
                qCWarning(lcClipboard) << "cliphist list exited with code" << exitCode;
            }
            // Parse whatever was produced even on failure, matching the QML
            // StdioCollector behaviour (stream-finished always fired).
            parseListOutput(QString::fromUtf8(m_fetcher->readAllStandardOutput()));
            preloadImages();
        });
    QObject::connect(m_fetcher, &QProcess::errorOccurred, this, [](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            qCWarning(lcClipboard) << "Failed to start cliphist";
        }
    });
}

QVariantList ClipboardCore::items() const {
    return m_items;
}

QVariantList ClipboardCore::favouriteClips() const {
    return m_favouriteClips;
}

void ClipboardCore::setFavouriteClips(const QVariantList& favouriteClips) {
    if (m_favouriteClips == favouriteClips) {
        return;
    }
    m_favouriteClips = favouriteClips;
    emit favouriteClipsChanged();
}

QString ClipboardCore::imageCacheDir() const {
    return cacheDir;
}

void ClipboardCore::reload() {
    if (m_fetcher->state() != QProcess::NotRunning) {
        return;
    }
    m_fetcher->start(QStringLiteral("cliphist"), { QStringLiteral("list") });
}

void ClipboardCore::parseListOutput(const QString& output) {
    static const QRegularExpression lineRe(QStringLiteral("^(\\d+)\\t(.+)"));
    // Deliberately narrow (png + KiB only) to stay byte-identical with the QML
    // regex — do not widen to other formats or size units.
    static const QRegularExpression imageRe(
        QStringLiteral("^\\[\\[ binary data \\d+ KiB png \\d+x\\d+ \\]\\]"));

    QVariantList result;
    const QStringList lines = output.trimmed().split(u'\n');
    for (const QString& line : lines) {
        if (line.isEmpty()) {
            continue;
        }

        const QRegularExpressionMatch match = lineRe.match(line);
        if (!match.hasMatch()) {
            continue;
        }

        const QString preview = match.captured(2);
        result << QVariantMap{
            { QStringLiteral("id"), match.captured(1).toInt() },
            { QStringLiteral("preview"), preview },
            { QStringLiteral("isImage"), imageRe.match(preview).hasMatch() },
        };
    }

    m_items = result;
    emit itemsChanged();
}

void ClipboardCore::preloadImages() {
    m_decodeQueue.clear();
    for (const QVariant& entry : std::as_const(m_items)) {
        const QVariantMap item = entry.toMap();
        const int id = item.value(QStringLiteral("id")).toInt();
        // id != 0 mirrors the `item.id` truthiness check in the QML source
        if (item.value(QStringLiteral("isImage")).toBool() && id != 0) {
            m_decodeQueue << id;
        }
    }
    pumpDecodeQueue();
}

void ClipboardCore::pumpDecodeQueue() {
    while (m_activeDecodes < maxConcurrentDecodes && !m_decodeQueue.isEmpty()) {
        startDecode(m_decodeQueue.takeFirst(), true);
    }
}

void ClipboardCore::startDecode(int id, bool fromQueue) {
    if (!QDir().mkpath(cacheDir)) {
        qCWarning(lcClipboard) << "Failed to create image cache dir" << cacheDir;
        return;
    }

    auto* process = new QProcess(this);
    // The QML version redirected stdout and stderr into the file (2>&1)
    process->setProcessChannelMode(QProcess::MergedChannels);
    process->setStandardOutputFile(getImagePath(id));

    const auto finish = [this, process, fromQueue] {
        // finished() and errorOccurred() can both fire for one process
        if (process->property("caelestiaDone").toBool()) {
            return;
        }
        process->setProperty("caelestiaDone", true);
        process->deleteLater();
        if (fromQueue) {
            --m_activeDecodes;
            pumpDecodeQueue();
        }
    };
    QObject::connect(process, &QProcess::finished, this, finish);
    QObject::connect(process, &QProcess::errorOccurred, this, finish);

    if (fromQueue) {
        ++m_activeDecodes;
    }
    process->start(QStringLiteral("cliphist"), { QStringLiteral("decode"), QString::number(id) });
}

QVariantList ClipboardCore::getSortedItems() const {
    if (m_items.isEmpty()) {
        return {};
    }

    QSet<QString> favourites;
    for (const QVariant& fav : m_favouriteClips) {
        favourites.insert(fav.toString());
    }

    // Stable partition: favourites first, cliphist order preserved within each group
    QVariantList favs;
    QVariantList rest;
    for (const QVariant& entry : m_items) {
        const QString id = entry.toMap().value(QStringLiteral("id")).toString();
        if (favourites.contains(id)) {
            favs << entry;
        } else {
            rest << entry;
        }
    }
    return favs + rest;
}

QString ClipboardCore::getImagePath(int clipId) const {
    return cacheDir + u'/' + QString::number(clipId) + QStringLiteral(".png");
}

void ClipboardCore::ensureImageCached(int id, QJSValue onReady) {
    startDecode(id, false);

    // Callback is optional and best-effort; 1s delay keeps parity with the
    // QML wait timer. No caller currently passes one.
    if (!onReady.isCallable()) {
        return;
    }

    const QString imgPath = getImagePath(id);
    QTimer::singleShot(1000, this, [this, onReady, imgPath]() mutable {
        auto* engine = qjsEngine(this);
        if (!engine) {
            qCWarning(lcClipboard) << "Unable to invoke callback: no JS engine";
            return;
        }
        onReady.call({ engine->toScriptValue(imgPath) });
    });
}

} // namespace caelestia::services
