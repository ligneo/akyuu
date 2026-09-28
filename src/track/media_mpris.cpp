/**
 * Akyuu
 * Copyright (C) 2026, cenky <cenkkgl@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "media_mpris.hpp"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QFileInfo>
#include <QRegularExpression>
#include <QVariantMap>
#include <algorithm>

#include "base/string.hpp"

namespace track::media {

namespace {

constexpr auto kServicePrefix = "org.mpris.MediaPlayer2.";
constexpr auto kObjectPath = "/org/mpris/MediaPlayer2";
constexpr auto kPlayerInterface = "org.mpris.MediaPlayer2.Player";
constexpr auto kPropertiesInterface = "org.freedesktop.DBus.Properties";

QString getExecutablePath(const uint processId) {
  return QFileInfo{u"/proc/%1/exe"_s.arg(processId)}.symLinkTarget();
}

// Tor Browser is Firefox, down to the process name and the MPRIS service, so the bundled Firefox
// entry matches it and Akyuu would read the title of whatever page is open. Only the path tells
// the two apart: the bundle is extracted into a `tor-browser` directory with the browser under
// `Browser/`, and torbrowser-launcher keeps that layout under `~/.local/share/torbrowser`.
// Reporting what someone is doing in Tor Browser is the last thing this feature should do, so it
// is left out rather than made an option.
bool isTorBrowser(const QString& executablePath) {
  const auto parts = executablePath.split(u'/', Qt::SkipEmptyParts);

  return std::ranges::any_of(parts, [](const QString& part) {
    return !part.compare(u"tor-browser", Qt::CaseInsensitive) ||
           !part.compare(u"torbrowser", Qt::CaseInsensitive);
  });
}

const anisthesia::Player* findWebBrowser(const std::vector<anisthesia::Player>& players,
                                         const std::string& processName) {
  const auto matches = [&processName](const std::string& pattern) {
    if (pattern.starts_with('^')) {
      const QRegularExpression re{
          QRegularExpression::anchoredPattern(QString::fromStdString(pattern))};
      return re.match(QString::fromStdString(processName)).hasMatch();
    }
    return compareStrings(pattern, processName, Qt::CaseInsensitive) == 0;
  };

  for (const auto& player : players) {
    if (player.type != anisthesia::PlayerType::WebBrowser) continue;
    if (std::ranges::any_of(player.executables, matches)) return &player;
  }

  return nullptr;
}

QVariant getProperty(const QDBusConnection& bus, const QString& service, const QString& name) {
  auto message = QDBusMessage::createMethodCall(service, kObjectPath, kPropertiesInterface, "Get");
  message << QString{kPlayerInterface} << name;

  // Detection runs on the GUI thread, so an unresponsive player must not block it for long
  const auto reply = bus.call(message, QDBus::Block, 100);
  if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) return {};

  return reply.arguments().constFirst().value<QDBusVariant>().variant();
}

}  // namespace

std::vector<anisthesia::Result> getMprisResults(const std::vector<anisthesia::Player>& players) {
  std::vector<anisthesia::Result> results;

  const auto bus = QDBusConnection::sessionBus();
  if (!bus.isConnected()) return results;

  const QStringList services = bus.interface()->registeredServiceNames();

  for (const auto& service : services) {
    if (!service.startsWith(kServicePrefix)) continue;

    // Proxies such as playerctld are not backed by a process of their own
    const auto processId = bus.interface()->servicePid(service);
    if (!processId.isValid()) continue;

    const auto executablePath = getExecutablePath(processId.value());
    if (isTorBrowser(executablePath)) continue;

    const auto processName = QFileInfo{executablePath}.fileName().toStdString();
    const auto player = findWebBrowser(players, processName);
    if (!player) continue;

    if (getProperty(bus, service, "PlaybackStatus").toString() == "Stopped") continue;

    const auto metadata = qdbus_cast<QVariantMap>(getProperty(bus, service, "Metadata"));
    const auto title = metadata.value("xesam:title").toString().toStdString();
    const auto url = metadata.value("xesam:url").toString().toStdString();

    // Only the URL tells which streaming provider is playing. Without it the title would skip the
    // provider filter and be recognized as is, whatever the video is. Chromium leaves it out.
    if (url.empty()) continue;

    anisthesia::Result result;
    result.player = *player;
    result.id = {.process = processId.value()};

    const auto addMedia = [&result](const anisthesia::MediaInfoType type,
                                    const std::string& value) {
      if (value.empty()) return;
      anisthesia::Media media;
      media.information.push_back({type, value});
      result.media.push_back(std::move(media));
    };
    addMedia(anisthesia::MediaInfoType::Title, title);
    addMedia(anisthesia::MediaInfoType::Url, url);
    if (result.media.empty()) continue;

    results.push_back(std::move(result));
  }

  return results;
}

}  // namespace track::media
