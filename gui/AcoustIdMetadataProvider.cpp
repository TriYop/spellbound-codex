#include "AcoustIdMetadataProvider.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace gui {

// Free AcoustID test client key — replace with a registered application key for production.
// Register at: https://acoustid.org/new-application
static constexpr const char* kAcoustIdKey = "8XaBELgH";

std::optional<AcoustIdMetadataProvider::Fingerprint>
AcoustIdMetadataProvider::runFpcalc(const std::string& path) {
    QProcess proc;
    proc.start("fpcalc", {"-json", QString::fromStdString(path)});
    if (!proc.waitForFinished(30000) || proc.exitCode() != 0)
        return std::nullopt;

    const QJsonDocument doc = QJsonDocument::fromJson(proc.readAllStandardOutput());
    if (doc.isNull()) return std::nullopt;

    const QJsonObject obj = doc.object();
    Fingerprint fp;
    fp.fp       = obj["fingerprint"].toString().toStdString();
    fp.duration = obj["duration"].toInt(0);
    if (fp.fp.empty() || fp.duration <= 0) return std::nullopt;
    return fp;
}

std::optional<pb::TrackMetadata>
AcoustIdMetadataProvider::queryAcoustId(const Fingerprint& fp) {
    QNetworkAccessManager nam;

    QUrlQuery query;
    query.addQueryItem("client",      kAcoustIdKey);
    query.addQueryItem("fingerprint", QString::fromStdString(fp.fp));
    query.addQueryItem("duration",    QString::number(fp.duration));
    query.addQueryItem("meta",        "recordings+releasegroups");

    QUrl url("https://api.acoustid.org/v2/lookup");
    url.setQuery(query);

    QNetworkRequest req(url);
    QNetworkReply* reply = nam.get(req);

    // Spin a local event loop until the reply arrives or 5 s timeout.
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply,  &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout,         &loop, &QEventLoop::quit);
    timer.start(5000);
    loop.exec();

    if (!reply->isFinished() || reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        return std::nullopt;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    reply->deleteLater();
    if (doc.isNull()) return std::nullopt;

    const QJsonObject root = doc.object();
    if (root["status"].toString() != "ok") return std::nullopt;

    const QJsonArray results = root["results"].toArray();
    if (results.isEmpty()) return std::nullopt;

    const QJsonArray recordings = results[0].toObject()["recordings"].toArray();
    if (recordings.isEmpty()) return std::nullopt;

    const QJsonObject rec = recordings[0].toObject();

    pb::TrackMetadata meta;
    meta.source = pb::MetadataSource::acoustid;
    meta.title  = rec["title"].toString().toStdString();
    if (meta.title && meta.title->empty()) meta.title.reset();

    const QJsonArray artists = rec["artists"].toArray();
    if (!artists.isEmpty()) {
        meta.artist = artists[0].toObject()["name"].toString().toStdString();
        if (meta.artist && meta.artist->empty()) meta.artist.reset();
    }

    const QJsonArray releaseGroups = rec["releasegroups"].toArray();
    if (!releaseGroups.isEmpty()) {
        const QJsonObject rg = releaseGroups[0].toObject();
        meta.album = rg["title"].toString().toStdString();
        if (meta.album && meta.album->empty()) meta.album.reset();
    }

    return meta;
}

std::optional<pb::TrackMetadata>
AcoustIdMetadataProvider::lookup(const std::string& audioFilePath) {
    const auto fp = runFpcalc(audioFilePath);
    if (!fp) return std::nullopt;
    return queryAcoustId(*fp);
}

} // namespace gui
