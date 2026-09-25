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

#include "irc.hpp"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QRegularExpression>

#include "akyuu/script.hpp"
#include "akyuu/settings.hpp"
#include "base/log.hpp"

namespace link::irc {

using namespace Qt::StringLiterals;

namespace {

constexpr auto kService = "org.kde.konversation";
constexpr auto kPath = "/irc";
constexpr auto kInterface = "org.kde.konversation";
constexpr int kTimeoutMs = 2000;

QDBusMessage call(const QString& method, const QVariantList& arguments) {
  auto message =
      QDBusMessage::createMethodCall(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                                     QString::fromLatin1(kInterface), method);
  message.setArguments(arguments);
  // The default timeout is 25 seconds. Nothing here is worth blocking the interface for that long,
  // and the settings page asks three of these while it is being opened.
  return QDBusConnection::sessionBus().call(message, QDBus::Block, kTimeoutMs);
}

QStringList callForList(const QString& method, const QVariantList& arguments = {}) {
  const QDBusReply<QStringList> reply = call(method, arguments);
  if (!reply.isValid()) {
    qWarning() << "Could not ask the IRC client for" << method << reply.error().message();
    return {};
  }
  return reply.value();
}

// Konversation runs whatever is given to `say` through its command parser, so a message that
// begins with a slash would be taken as a command. `/say` puts it back to being text.
QString toCommand(const QString& message, const bool useAction) {
  if (useAction) return u"/me "_s + message;
  return message.startsWith(u'/') ? u"/say "_s + message : message;
}

// v1 separates channels by spaces, commas or semicolons, and prefixes a name that has no marker.
QStringList parseChannels(const QString& value) {
  QStringList channels;

  static const QRegularExpression separators{u"[ ,;]"_s};

  for (auto channel : value.split(separators, Qt::SkipEmptyParts)) {
    channel = channel.trimmed();
    if (channel.isEmpty()) continue;
    // v1 marks the active channel with an asterisk. There is no active channel to send to here,
    // so the marker is only removed.
    if (channel.startsWith(u'*')) channel.remove(0, 1);
    if (!channel.startsWith(u'#')) channel.prepend(u'#');
    channels.append(channel);
  }

  return channels;
}

}  // namespace

bool isRunning() {
  const auto interface = QDBusConnection::sessionBus().interface();
  // Asking whether the service is registered, rather than calling it: the client has a D-Bus
  // service file, so a call would start it and take it online on its own.
  return interface && interface->isServiceRegistered(QString::fromLatin1(kService)).value();
}

QStringList connections() {
  if (!isRunning()) return {};
  return callForList(u"listConnectedServers"_s);
}

QStringList joinedChannels(const QString& connection) {
  if (!isRunning()) return {};
  return callForList(u"listJoinedChannels"_s, {connection});
}

void announce(const track::Episode& episode, const bool force) {
  if (!force && !akyuu::settings.ircShareEnabled()) return;
  if (!isRunning()) return;

  const auto message =
      akyuu::replaceVariables(QString::fromStdString(akyuu::settings.ircShareFormat()), episode);

  if (message.isEmpty()) return;

  const auto command = toCommand(message, akyuu::settings.ircShareUseAction());
  const auto wanted =
      akyuu::settings.ircShareAllChannels()
          ? QStringList{}
          : parseChannels(QString::fromStdString(akyuu::settings.ircShareChannels()));

  for (const auto& connection : connections()) {
    for (const auto& channel : joinedChannels(connection)) {
      // A channel that is not joined cannot be written to, so the list is a filter rather than a
      // set of targets.
      if (!wanted.isEmpty() && !wanted.contains(channel, Qt::CaseInsensitive)) continue;
      call(u"say"_s, {connection, channel, command});
    }
  }
}

}  // namespace link::irc
