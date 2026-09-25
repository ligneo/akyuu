/**
 * Taiga
 * Copyright (C) 2010-2026, Eren Okka
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

#include "settings.hpp"

#include <QFile>
#include <QJsonArray>
#include <algorithm>
#include <ranges>

#include "akyuu/accounts.hpp"
#include "akyuu/path.hpp"
#include "akyuu/version.hpp"
#include "base/string.hpp"
#include "compat/settings.hpp"
#include "sync/service.hpp"

namespace akyuu {

void Settings::init() const {
  const auto appVersion = akyuu::version().to_string();

  // v1 to v2
  if (!QFile::exists(fileName())) {
    compat::v1::readSettings(std::format("{}/v1/settings.xml", get_data_path()), *this, accounts);
    setValue("meta.version", appVersion);
    return;
  }

  // v2.x
  const auto fileVersion = value("meta.version").toString().toStdString();
  if (fileVersion != appVersion) {
    setValue("meta.version", appVersion);
  }
}

QString Settings::fileName() const {
  return u"%1/settings.json"_s.arg(get_data_path());
}

////////////////////////////////////////////////////////////////////////////////

Qt::ColorScheme Settings::appColorScheme() const {
  return value("app.colorScheme", static_cast<int>(Qt::ColorScheme::Unknown))
      .value<Qt::ColorScheme>();
}

std::string Settings::appStyle() const {
#ifdef Q_OS_WINDOWS
  // Fusion is more consistent than the Windows 11 style.
  const auto defaultStyle = u"fusion"_s;
#else
  const auto defaultStyle = QString{kAppStyleSystem};
#endif
  return value("app.style", defaultStyle).toString().toStdString();
}

std::vector<std::string> Settings::disabledMediaPlayers() const {
  return value("recognition.mediaPlayers.disabled").toJsonArray().toVariantList() |
         std::views::transform([](const QVariant& v) { return v.toString().toStdString(); }) |
         std::ranges::to<std::vector>();
}

std::string Settings::service() const {
  return value("v1.service", sync::serviceSlug(sync::ServiceId::AniList)).toString().toStdString();
}

std::vector<std::string> Settings::libraryFolders() const {
  return value("library.folders").toJsonArray().toVariantList() |
         std::views::transform([](const QVariant& v) { return v.toString().toStdString(); }) |
         std::ranges::to<std::vector>();
}

// v1's `recognition/anitomy/ignored_strings`. The bundled Anitomy has no option for these, so
// they are taken out of the file name before it is parsed, which has the same effect.
std::vector<std::string> Settings::recognitionIgnoredStrings() const {
  return value("recognition.ignoredStrings").toJsonArray() |
         std::views::transform([](const QJsonValue& v) { return v.toString().toStdString(); }) |
         std::ranges::to<std::vector>();
}

// v1's `recognition/general/lookup_parent_directories`, on by default there as well.
bool Settings::recognitionLookupParentDirectories() const {
  return value("recognition.lookupParentDirectories", true).toBool();
}

bool Settings::mediaDetectionEnabled() const {
  return value("track.detection.enabled", true).toBool();
}

std::chrono::milliseconds Settings::mediaDetectionInterval() const {
  const auto interval = value("track.detection.interval", 3000).toInt();
  return std::chrono::milliseconds{interval};
}

QNetworkProxy::ProxyType Settings::proxyType() const {
  const auto type = value("network.proxy.type", u"http"_s).toString();
  if (type == u"socks5") return QNetworkProxy::Socks5Proxy;
  return QNetworkProxy::HttpProxy;
}

std::string Settings::proxyHost() const {
  return value("network.proxy.host").toString().toStdString();
}

int Settings::proxyPort() const {
  return value("network.proxy.port", -1).toInt();
}

std::string Settings::proxyUsername() const {
  return value("network.proxy.username").toString().toStdString();
}

std::string Settings::proxyPassword() const {
  return value("network.proxy.password").toString().toStdString();
}

// v1's `program/general/hidesidebar`, the other way round.
bool Settings::sidebarVisible() const {
  return value("app.sidebarVisible", true).toBool();
}

// v1's `program/list/progress/showaired` and `showavailable`, both on by default there.
// v1's `anime/folders/watch/enabled`, on by default there as well.
// v1's `program/general/autostart`, `close`, `minimize` and `program/startup/minimize`.
bool Settings::appAutoStart() const {
  return value("app.autoStart", false).toBool();
}

bool Settings::appCloseToTray() const {
  return value("app.closeToTray", false).toBool();
}

bool Settings::appMinimizeToTray() const {
  return value("app.minimizeToTray", false).toBool();
}

bool Settings::appStartMinimized() const {
  return value("app.startMinimized", false).toBool();
}

bool Settings::libraryWatchFolders() const {
  return value("library.folders.watch", true).toBool();
}

// v1's `program/startup/checkeps`.
bool Settings::libraryScanOnStartup() const {
  return value("library.scanOnStartup", false).toBool();
}

// v1's `anime/folders/scan/minfilesize`, which it stores in bytes.
qint64 Settings::libraryMinimumFileSize() const {
  return value("library.minimumFileSize", 0).toLongLong();
}

// v1 lets both clicks be chosen from the same list of actions.
std::string Settings::listDoubleClickAction() const {
  return value("animeList.action.doubleClick", u"details"_s).toString().toStdString();
}

std::string Settings::listMiddleClickAction() const {
  return value("animeList.action.middleClick", u"playNextEpisode"_s).toString().toStdString();
}

// v1's `program/list/filter/episodes/highlight`.
bool Settings::listHighlightNewEpisodes() const {
  return value("animeList.highlightNewEpisodes", true).toBool();
}

// v1's `program/list/filter/episodes/highlightedontop`
bool Settings::listHighlightedOnTop() const {
  return value("animeList.highlightedOnTop", false).toBool();
}

bool Settings::listShowAiredEpisodes() const {
  return value("animeList.progress.showAired", true).toBool();
}

bool Settings::listShowAvailableEpisodes() const {
  return value("animeList.progress.showAvailable", true).toBool();
}

bool Settings::streamingMediaEnabled() const {
  return value("recognition.streaming.enabled", false).toBool();
}

// v1's `announce/discord/*`. The application id has no control in v1 either; it belongs in the
// advanced table.
// v1's `announce/http/*`, with its default format.
// v1's `program/general/enablesharing`: one switch above every sharing channel.
bool Settings::sharingEnabled() const {
  return value("sharing.enabled", true).toBool();
}

bool Settings::httpShareEnabled() const {
  return value("sharing.http.enabled", false).toBool();
}

std::string Settings::httpShareUrl() const {
  return value("sharing.http.url").toString().toStdString();
}

std::string Settings::httpShareFormat() const {
  static const auto defaultFormat =
      u"user=%user%&name=%title%&ep=%episode%&eptotal=%total%&score=%score%&picurl=%image%"_s;
  return value("sharing.http.format", defaultFormat).toString().toStdString();
}

// v1's `announce/mirc/*`. Its "active channel" mode has no counterpart: Konversation does not
// tell anyone which tab is in front, so the choice is between every joined channel and a list.
bool Settings::ircShareEnabled() const {
  return value("sharing.irc.enabled", false).toBool();
}

std::string Settings::ircShareFormat() const {
  static const auto defaultFormat =
      u"\00304$if($greater(%episode%,%watched%),Watching,Rewatching):\003 %title%"
      u"$if(%episode%, \00303%episode%$if(%total%,/%total%))\003 "
      u"$if(%score%,\00314[Score: %score%]\003) "
      u"\00312%animeurl%"_s;
  return value("sharing.irc.format", defaultFormat).toString().toStdString();
}

std::string Settings::ircShareChannels() const {
  return value("sharing.irc.channels", u"#kitsu, #myanimelist"_s).toString().toStdString();
}

bool Settings::ircShareAllChannels() const {
  return value("sharing.irc.allChannels", false).toBool();
}

bool Settings::ircShareUseAction() const {
  return value("sharing.irc.useAction", true).toBool();
}

bool Settings::discordEnabled() const {
  return value("sharing.discord.enabled", false).toBool();
}

std::string Settings::discordApplicationId() const {
  return value("sharing.discord.applicationId", u"379871385176244224"_s).toString().toStdString();
}

bool Settings::discordTimeEnabled() const {
  return value("sharing.discord.showTime", true).toBool();
}

bool Settings::discordUsernameEnabled() const {
  return value("sharing.discord.showUsername", true).toBool();
}

bool Settings::discordGroupEnabled() const {
  return value("sharing.discord.showGroup", true).toBool();
}

bool Settings::syncEnabled() const {
  return value("sync.enabled", true).toBool();
}

// v1's `account/myanimelist/login`, which despite the name synchronizes whichever service is active
bool Settings::syncOnStartup() const {
  return value("sync.onStartup", false).toBool();
}

// v1's `account/update/gotonowplaying` and `gotonowplayingnot`, with their defaults.
bool Settings::syncGoToNowPlayingNotRecognized() const {
  return value("sync.goToNowPlaying.notRecognized", false).toBool();
}

bool Settings::syncGoToNowPlayingRecognized() const {
  return value("sync.goToNowPlaying.recognized", true).toBool();
}

bool Settings::syncNotifyNotRecognized() const {
  return value("sync.notify.notRecognized", true).toBool();
}

bool Settings::syncNotifyRecognized() const {
  return value("sync.notify.recognized", true).toBool();
}

// v1's `program/notifications/balloon/format`, with its default.
std::string Settings::syncNotifyFormat() const {
  static const auto defaultFormat =
      u"$if(%title%,%title%)\\n"
      u"$if(%episode%,Episode %episode%$if(%total%,/%total%) )$if(%group%,by %group%)\\n"
      u"$if(%name%,%name%)"_s;
  return value("sync.notify.format", defaultFormat).toString().toStdString();
}

anime::TitleLanguage Settings::titleLanguage() const {
  if (!titleLanguageCache_) {
    const auto language = value("library.titleLanguage", u"romaji"_s).toString();
    if (language == u"english") {
      titleLanguageCache_ = anime::TitleLanguage::English;
    } else if (language == u"native") {
      titleLanguageCache_ = anime::TitleLanguage::Native;
    } else {
      titleLanguageCache_ = anime::TitleLanguage::Romaji;
    }
  }
  return *titleLanguageCache_;
}

// v1 asked before every update. With this off, only an episode that skips ahead asks.
bool Settings::updateAskToConfirm() const {
  return value("track.update.askToConfirm", true).toBool();
}

std::chrono::seconds Settings::updateDelay() const {
  const std::chrono::seconds delay{value("track.update.delay", 120).toInt()};
  return std::clamp(delay, kUpdateDelayMin, kUpdateDelayMax);
}

bool Settings::updateLibraryOnly() const {
  return value("track.update.libraryOnly", false).toBool();
}

// v1's `account/update/outofrange`: an episode that skips ahead is ignored instead of asked about.
bool Settings::updateOutOfRange() const {
  return value("track.update.outOfRange", false).toBool();
}

bool Settings::updatePauseWhenUnfocused() const {
  return value("track.update.pauseWhenUnfocused", false).toBool();
}

track::UpdateTrigger Settings::updateTrigger() const {
  const auto trigger = value("track.update.trigger", u"afterDelay"_s).toString();
  if (trigger == u"onPlayerClose") return track::UpdateTrigger::OnPlayerClose;
  return track::UpdateTrigger::AfterDelay;
}

////////////////////////////////////////////////////////////////////////////////

void Settings::setAppColorScheme(const Qt::ColorScheme scheme) const {
  setValue("app.colorScheme", static_cast<int>(scheme));
}

void Settings::setAppStyle(const std::string& style) const {
  setValue("app.style", style);
}

// Each entry is v1's `Name|URL`, and a lone "-" is a separator. Keeping v1's shape means a list
// copied out of its settings works here as it is.
std::vector<std::string> Settings::externalLinks() const {
  static const QStringList defaults{
      u"MALgraph|https://anime.plus/"_s,
      u"-"_s,
      u"AniChart|https://anichart.net/airing"_s,
      u"Monthly.moe|https://www.monthly.moe/weekly"_s,
      u"Senpai Anime Charts|https://www.senpai.moe/?mode=calendar"_s,
      u"-"_s,
      u"Anime Scene Search Engine|https://trace.moe/"_s,
      u"Anime Streaming Search Engine|https://because.moe/"_s,
  };

  const auto stored = value("app.externalLinks");

  const auto list = stored.isValid()
                        ? stored.toJsonArray() | std::views::transform([](const QJsonValue& v) {
                            return v.toString();
                          }) | std::ranges::to<QStringList>()
                        : defaults;

  return list | std::views::transform([](const QString& s) { return s.toStdString(); }) |
         std::ranges::to<std::vector>();
}

std::vector<std::string> Settings::disabledStreamingProviders() const {
  return value("recognition.streaming.disabledProviders").toJsonArray().toVariantList() |
         std::views::transform([](const QVariant& v) { return v.toString().toStdString(); }) |
         std::ranges::to<std::vector>();
}

std::string Settings::torrentDiscoveryUrl() const {
  static const auto defaultUrl = u"https://www.tokyotosho.info/rss.php?filter=1,11&zwnj=0"_s;
  return value("torrents.discovery.url", defaultUrl).toString().toStdString();
}

std::string Settings::torrentSearchUrl() const {
  static const auto defaultUrl = u"https://nyaa.si/?page=rss&c=1_2&f=0&q=%title%"_s;
  return value("torrents.discovery.searchUrl", defaultUrl).toString().toStdString();
}

bool Settings::torrentAutoCheckEnabled() const {
  return value("torrents.discovery.autoCheck", true).toBool();
}

std::chrono::minutes Settings::torrentAutoCheckInterval() const {
  return std::chrono::minutes{value("torrents.discovery.interval", 60).toInt()};
}

bool Settings::torrentNotifyNewEpisodes() const {
  return value("torrents.discovery.newAction", u"notify"_s).toString() == u"notify";
}

// v1's other action for a new torrent. It only makes sense with filters, which decide what counts
// as selected; without them every release of every episode would be downloaded.
bool Settings::torrentDownloadNewEpisodes() const {
  return value("torrents.discovery.newAction").toString() == u"download";
}

int Settings::torrentArchiveMaxCount() const {
  return value("torrents.archive.maxCount", 1000).toInt();
}

// v1 orders the download queue by episode number or release date.
std::string Settings::torrentDownloadSortBy() const {
  return value("torrents.download.sortBy", u"episodeNumber"_s).toString().toStdString();
}

Qt::SortOrder Settings::torrentDownloadSortOrder() const {
  return value("torrents.download.sortOrder").toString() == u"descending"
             ? Qt::SortOrder::DescendingOrder
             : Qt::SortOrder::AscendingOrder;
}

// Where the BitTorrent client is told to put the files. v1 prefers the anime's own folder and
// falls back to this one; v2 has no per-anime folder, so this is the only one.
// v1's `rss/torrent/options/autocreatefolder`.
bool Settings::torrentDownloadCreateSubfolder() const {
  return value("torrents.download.createSubfolder", false).toBool();
}

// v1's `rss/torrent/options/autousefolder`. Before it existed here, a location being set meant
// the same thing, which is what the default keeps.
bool Settings::torrentDownloadFallbackOnFolder() const {
  return value("torrents.download.fallbackOnFolder", !torrentDownloadLocation().empty()).toBool();
}

std::string Settings::torrentDownloadLocation() const {
  return value("torrents.download.location").toString().toStdString();
}

// v1's `rss/torrent/options/autosetfolder`.
bool Settings::torrentDownloadUseAnimeFolder() const {
  return value("torrents.download.useAnimeFolder", true).toBool();
}

// The three below have no control of their own in v1 either; they live in its Advanced table.
bool Settings::torrentDownloadUseMagnet() const {
  return value("torrents.download.useMagnet", false).toBool();
}

std::string Settings::torrentDownloadFileLocation() const {
  return value("torrents.download.fileLocation").toString().toStdString();
}

bool Settings::torrentDownloadOpen() const {
  return value("torrents.download.open", true).toBool();
}

// "default" hands the file to whatever the desktop opens it with; anything else is the command in
// `torrents.download.appPath`.
std::string Settings::torrentDownloadAppMode() const {
  return value("torrents.download.appMode", u"default"_s).toString().toStdString();
}

std::string Settings::torrentDownloadAppPath() const {
  return value("torrents.download.appPath").toString().toStdString();
}

bool Settings::torrentFilterEnabled() const {
  return value("torrents.filters.enabled", true).toBool();
}

// An absent key means the filters have never been set up, which is when the default presets are
// used. An empty array means the user removed every filter, and is left alone.
std::optional<QJsonArray> Settings::torrentFilters() const {
  const auto filters = value("torrents.filters");
  if (!filters.isValid()) return std::nullopt;
  return filters.toJsonArray();
}

void Settings::setExternalLinks(std::vector<std::string> links) const {
  const auto list =
      links |
      std::views::transform([](const std::string& s) { return QString::fromStdString(s); }) |
      std::ranges::to<QList>();
  setValue("app.externalLinks", QJsonArray::fromStringList(list));
}

void Settings::setDisabledMediaPlayers(std::vector<std::string> players) const {
  const auto list =
      players |
      std::views::transform([](const std::string& s) { return QString::fromStdString(s); }) |
      std::ranges::to<QList>();
  setValue("recognition.mediaPlayers.disabled", QJsonArray::fromStringList(list));
}

void Settings::setTorrentDiscoveryUrl(const std::string& url) const {
  setValue("torrents.discovery.url", url);
}

void Settings::setTorrentDownloadNewEpisodes(const bool enabled) const {
  if (enabled) setValue("torrents.discovery.newAction", u"download"_s);
}

void Settings::setTorrentNotifyNewEpisodes(const bool enabled) const {
  setValue("torrents.discovery.newAction", enabled ? u"notify"_s : u"none"_s);
}

void Settings::setTorrentSearchUrl(const std::string& url) const {
  setValue("torrents.discovery.searchUrl", url);
}

void Settings::setTorrentAutoCheckEnabled(const bool enabled) const {
  setValue("torrents.discovery.autoCheck", enabled);
}

void Settings::setTorrentAutoCheckInterval(const std::chrono::minutes interval) const {
  setValue("torrents.discovery.interval", static_cast<int>(interval.count()));
}

void Settings::setDisabledStreamingProviders(std::vector<std::string> providers) const {
  const auto list =
      providers |
      std::views::transform([](const std::string& s) { return QString::fromStdString(s); }) |
      std::ranges::to<QList>();
  setValue("recognition.streaming.disabledProviders", QJsonArray::fromStringList(list));
}

void Settings::setTorrentArchiveMaxCount(const int count) const {
  setValue("torrents.archive.maxCount", count);
}

void Settings::setTorrentDownloadSortBy(const std::string& sortBy) const {
  setValue("torrents.download.sortBy", sortBy);
}

void Settings::setTorrentDownloadSortOrder(const Qt::SortOrder order) const {
  setValue("torrents.download.sortOrder",
           order == Qt::SortOrder::DescendingOrder ? u"descending"_s : u"ascending"_s);
}

void Settings::setTorrentDownloadFallbackOnFolder(const bool enabled) const {
  setValue("torrents.download.fallbackOnFolder", enabled);
}

void Settings::setTorrentDownloadCreateSubfolder(const bool enabled) const {
  setValue("torrents.download.createSubfolder", enabled);
}

void Settings::setTorrentDownloadLocation(const std::string& path) const {
  setValue("torrents.download.location", path);
}

void Settings::setTorrentDownloadUseAnimeFolder(const bool enabled) const {
  setValue("torrents.download.useAnimeFolder", enabled);
}

void Settings::setTorrentDownloadOpen(const bool open) const {
  setValue("torrents.download.open", open);
}

void Settings::setTorrentDownloadUseMagnet(const bool use) const {
  setValue("torrents.download.useMagnet", use);
}

void Settings::setTorrentDownloadAppMode(const std::string& mode) const {
  setValue("torrents.download.appMode", mode);
}

void Settings::setTorrentDownloadAppPath(const std::string& path) const {
  setValue("torrents.download.appPath", path);
}

void Settings::setTorrentDownloadFileLocation(const std::string& path) const {
  setValue("torrents.download.fileLocation", path);
}

void Settings::setTorrentFilterEnabled(const bool enabled) const {
  setValue("torrents.filters.enabled", enabled);
}

void Settings::setTorrentFilters(const QJsonArray& filters) const {
  setValue("torrents.filters", filters);
}

void Settings::setService(const std::string& service) const {
  setValue("v1.service", service);
}

void Settings::setLibraryFolders(std::vector<std::string> folders) const {
  const auto list =
      folders |
      std::views::transform([](const std::string& s) { return QString::fromStdString(s); }) |
      std::ranges::to<QList>();
  setValue("library.folders", QJsonArray::fromStringList(list));
}

void Settings::setRecognitionIgnoredStrings(std::vector<std::string> strings) const {
  const auto list =
      strings |
      std::views::transform([](const std::string& s) { return QString::fromStdString(s); }) |
      std::ranges::to<QList>();
  setValue("recognition.ignoredStrings", QJsonArray::fromStringList(list));
}

void Settings::setRecognitionLookupParentDirectories(const bool lookup) const {
  setValue("recognition.lookupParentDirectories", lookup);
}

void Settings::setMediaDetectionEnabled(const bool enabled) const {
  setValue("track.detection.enabled", enabled);
}

void Settings::setMediaDetectionInterval(const std::chrono::milliseconds interval) const {
  setValue("track.detection.interval", static_cast<qint64>(interval.count()));
}

void Settings::setProxyType(const QNetworkProxy::ProxyType type) const {
  const std::string slug = type == QNetworkProxy::Socks5Proxy ? "socks5" : "http";
  setValue("network.proxy.type", slug);
}

void Settings::setProxyHost(const std::string& host) const {
  setValue("network.proxy.host", host);
}

void Settings::setProxyPort(const int port) const {
  setValue("network.proxy.port", port);
}

void Settings::setProxyUsername(const std::string& username) const {
  setValue("network.proxy.username", username);
}

void Settings::setProxyPassword(const std::string& password) const {
  setValue("network.proxy.password", password);
}

void Settings::setSidebarVisible(const bool visible) const {
  setValue("app.sidebarVisible", visible);
}

void Settings::setAppAutoStart(const bool enabled) const {
  setValue("app.autoStart", enabled);
}

void Settings::setAppCloseToTray(const bool enabled) const {
  setValue("app.closeToTray", enabled);
}

void Settings::setAppMinimizeToTray(const bool enabled) const {
  setValue("app.minimizeToTray", enabled);
}

void Settings::setAppStartMinimized(const bool enabled) const {
  setValue("app.startMinimized", enabled);
}

void Settings::setLibraryWatchFolders(const bool watch) const {
  setValue("library.folders.watch", watch);
}

void Settings::setLibraryScanOnStartup(const bool scan) const {
  setValue("library.scanOnStartup", scan);
}

void Settings::setLibraryMinimumFileSize(const qint64 bytes) const {
  setValue("library.minimumFileSize", bytes);
}

void Settings::setListDoubleClickAction(const std::string& action) const {
  setValue("animeList.action.doubleClick", action);
}

void Settings::setListMiddleClickAction(const std::string& action) const {
  setValue("animeList.action.middleClick", action);
}

void Settings::setListHighlightNewEpisodes(const bool highlight) const {
  setValue("animeList.highlightNewEpisodes", highlight);
}

void Settings::setListHighlightedOnTop(const bool onTop) const {
  setValue("animeList.highlightedOnTop", onTop);
}

void Settings::setListShowAiredEpisodes(const bool show) const {
  setValue("animeList.progress.showAired", show);
}

void Settings::setListShowAvailableEpisodes(const bool show) const {
  setValue("animeList.progress.showAvailable", show);
}

void Settings::setStreamingMediaEnabled(const bool enabled) const {
  setValue("recognition.streaming.enabled", enabled);
}

void Settings::setSharingEnabled(const bool enabled) const {
  setValue("sharing.enabled", enabled);
}

void Settings::setHttpShareEnabled(const bool enabled) const {
  setValue("sharing.http.enabled", enabled);
}

void Settings::setHttpShareUrl(const std::string& url) const {
  setValue("sharing.http.url", url);
}

void Settings::setHttpShareFormat(const std::string& format) const {
  setValue("sharing.http.format", format);
}

void Settings::setIrcShareEnabled(const bool enabled) const {
  setValue("sharing.irc.enabled", enabled);
}

void Settings::setIrcShareFormat(const std::string& format) const {
  setValue("sharing.irc.format", format);
}

void Settings::setIrcShareChannels(const std::string& channels) const {
  setValue("sharing.irc.channels", channels);
}

void Settings::setIrcShareAllChannels(const bool all) const {
  setValue("sharing.irc.allChannels", all);
}

void Settings::setIrcShareUseAction(const bool use) const {
  setValue("sharing.irc.useAction", use);
}

void Settings::setDiscordEnabled(const bool enabled) const {
  setValue("sharing.discord.enabled", enabled);
}

void Settings::setDiscordApplicationId(const std::string& id) const {
  setValue("sharing.discord.applicationId", id);
}

void Settings::setDiscordTimeEnabled(const bool enabled) const {
  setValue("sharing.discord.showTime", enabled);
}

void Settings::setDiscordUsernameEnabled(const bool enabled) const {
  setValue("sharing.discord.showUsername", enabled);
}

void Settings::setDiscordGroupEnabled(const bool enabled) const {
  setValue("sharing.discord.showGroup", enabled);
}

void Settings::setSyncEnabled(const bool enabled) const {
  setValue("sync.enabled", enabled);
}

void Settings::setSyncOnStartup(const bool enabled) const {
  setValue("sync.onStartup", enabled);
}

void Settings::setSyncGoToNowPlayingNotRecognized(const bool enabled) const {
  setValue("sync.goToNowPlaying.notRecognized", enabled);
}

void Settings::setSyncGoToNowPlayingRecognized(const bool enabled) const {
  setValue("sync.goToNowPlaying.recognized", enabled);
}

void Settings::setSyncNotifyNotRecognized(const bool enabled) const {
  setValue("sync.notify.notRecognized", enabled);
}

void Settings::setSyncNotifyRecognized(const bool enabled) const {
  setValue("sync.notify.recognized", enabled);
}

void Settings::setSyncNotifyFormat(const std::string& format) const {
  setValue("sync.notify.format", format);
}

void Settings::setTitleLanguage(const anime::TitleLanguage language) const {
  const auto slug = [language]() -> std::string {
    switch (language) {
      default:
      case anime::TitleLanguage::Romaji:
        return "romaji";
      case anime::TitleLanguage::English:
        return "english";
      case anime::TitleLanguage::Native:
        return "native";
    }
  }();
  setValue("library.titleLanguage", slug);
  titleLanguageCache_ = language;
}

void Settings::setUpdateAskToConfirm(const bool enabled) const {
  setValue("track.update.askToConfirm", enabled);
}

void Settings::setUpdateDelay(const std::chrono::seconds delay) const {
  setValue("track.update.delay", static_cast<int>(delay.count()));
}

void Settings::setUpdateLibraryOnly(const bool enabled) const {
  setValue("track.update.libraryOnly", enabled);
}

void Settings::setUpdateOutOfRange(const bool enabled) const {
  setValue("track.update.outOfRange", enabled);
}

void Settings::setUpdatePauseWhenUnfocused(const bool enabled) const {
  setValue("track.update.pauseWhenUnfocused", enabled);
}

void Settings::setUpdateTrigger(const track::UpdateTrigger trigger) const {
  const std::string slug =
      trigger == track::UpdateTrigger::OnPlayerClose ? "onPlayerClose" : "afterDelay";
  setValue("track.update.trigger", slug);
}

}  // namespace akyuu
