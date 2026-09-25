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

#include "discord.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>

#include "akyuu/accounts.hpp"
#include "akyuu/settings.hpp"
#include "base/log.hpp"
#include "sync/service.hpp"

namespace link {

using namespace Qt::StringLiterals;

namespace {

enum Opcode {
  Handshake = 0,
  Frame = 1,
};

// Discord listens on one of ten sockets, in the runtime directory or one of the sandbox folders
// a Flatpak or Snap install uses.
QStringList socketPaths() {
  auto runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);

  if (runtime.isEmpty()) runtime = QDir::tempPath();

  const QStringList directories{
      runtime,
      u"%1/app/com.discordapp.Discord"_s.arg(runtime),
      u"%1/snap.discord"_s.arg(runtime),
  };

  QStringList paths;

  for (const auto& directory : directories) {
    for (int i = 0; i < 10; ++i) {
      paths.append(u"%1/discord-ipc-%2"_s.arg(directory).arg(i));
    }
  }

  return paths;
}

}  // namespace

Discord::Discord(QObject* parent) : QObject(parent) {}

bool Discord::connectToDiscord() {
  if (socket_.state() == QLocalSocket::ConnectedState) return true;

  for (const auto& path : socketPaths()) {
    socket_.connectToServer(path);
    if (socket_.waitForConnected(200)) {
      send(Opcode::Handshake,
           {{u"v"_s, 1},
            {u"client_id"_s, QString::fromStdString(akyuu::settings.discordApplicationId())}});
      qDebug() << "Connected to Discord:" << path;
      return true;
    }
    socket_.abort();
  }

  return false;
}

// Every frame is a little-endian opcode and length, then the JSON.
void Discord::send(const int opcode, const QJsonObject& payload) {
  const auto json = QJsonDocument{payload}.toJson(QJsonDocument::Compact);

  QByteArray frame;
  frame.resize(8);

  qToLittleEndian<quint32>(opcode, frame.data());
  qToLittleEndian<quint32>(json.size(), frame.data() + 4);
  frame.append(json);

  socket_.write(frame);
  socket_.flush();
}

void Discord::applySettings() {
  if (!akyuu::settings.discordEnabled()) clearPresence();
}

void Discord::updatePresence(QString details, QString state, const QString& largeImage,
                             const std::time_t timestamp, const bool force) {
  if (!force && !akyuu::settings.discordEnabled()) return;

  // v1 cuts both lines at 64 characters; Discord turns down an activity with longer ones.
  constexpr qsizetype kLimit = 64;
  for (auto text : {&details, &state}) {
    if (text->size() > kLimit) *text = text->left(kLimit - 3) + u"..."_s;
  }
  if (!connectToDiscord()) return;

  const auto service = sync::currentServiceId();

  const auto userName = [service]() -> QString {
    switch (service) {
      case sync::ServiceId::AniList:
        return QString::fromStdString(akyuu::accounts.anilistUsername());
      case sync::ServiceId::Kitsu:
        return QString::fromStdString(akyuu::accounts.kitsuUsername());
      case sync::ServiceId::MyAnimeList:
        return QString::fromStdString(akyuu::accounts.myanimelistUsername());
      default:
        return {};
    }
  }();

  const auto smallText = akyuu::settings.discordUsernameEnabled() && !userName.isEmpty()
                             ? u"%1 at %2"_s.arg(userName).arg(sync::serviceName(service))
                             : sync::serviceName(service);

  QJsonObject assets;
  assets[u"large_image"_s] = largeImage.isEmpty() ? u"default"_s : largeImage;
  assets[u"large_text"_s] = details;
  assets[u"small_image"_s] = sync::serviceSlug(service);
  assets[u"small_text"_s] = smallText;

  QJsonObject activity;
  activity[u"details"_s] = details;
  activity[u"state"_s] = state;
  activity[u"assets"_s] = assets;

  if (akyuu::settings.discordTimeEnabled()) {
    activity[u"timestamps"_s] = QJsonObject{{u"start"_s, static_cast<qint64>(timestamp)}};
  }

  send(Opcode::Frame, {
                          {u"cmd"_s, u"SET_ACTIVITY"_s},
                          {u"nonce"_s, QUuid::createUuid().toString(QUuid::WithoutBraces)},
                          {u"args"_s,
                           QJsonObject{
                               {u"pid"_s, QCoreApplication::applicationPid()},
                               {u"activity"_s, activity},
                           }},
                      });
}

void Discord::clearPresence() {
  if (socket_.state() != QLocalSocket::ConnectedState) return;

  send(Opcode::Frame, {
                          {u"cmd"_s, u"SET_ACTIVITY"_s},
                          {u"nonce"_s, QUuid::createUuid().toString(QUuid::WithoutBraces)},
                          {u"args"_s, QJsonObject{{u"pid"_s, QCoreApplication::applicationPid()}}},
                      });
}

}  // namespace link
