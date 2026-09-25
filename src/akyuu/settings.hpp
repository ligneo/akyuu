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

#pragma once

#include <QJsonArray>
#include <QNetworkProxy>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "base/settings.hpp"
#include "media/anime.hpp"
#include "track/update_trigger.hpp"

namespace taiga {

class Settings final : public base::Settings {
public:
  static constexpr std::chrono::seconds kUpdateDelayMin{10};
  static constexpr std::chrono::seconds kUpdateDelayMax{3600};

  static constexpr QLatin1StringView kAppStyleSystem{"system"};

  void init() const;

  Qt::ColorScheme appColorScheme() const;
  std::string appStyle() const;
  std::vector<std::string> externalLinks() const;
  std::vector<std::string> disabledMediaPlayers() const;
  std::vector<std::string> disabledStreamingProviders() const;
  std::string torrentDiscoveryUrl() const;
  std::string torrentSearchUrl() const;
  bool torrentAutoCheckEnabled() const;
  std::chrono::minutes torrentAutoCheckInterval() const;
  bool torrentNotifyNewEpisodes() const;
  bool torrentDownloadNewEpisodes() const;
  bool torrentFilterEnabled() const;
  int torrentArchiveMaxCount() const;
  std::string torrentDownloadSortBy() const;
  Qt::SortOrder torrentDownloadSortOrder() const;
  bool torrentDownloadCreateSubfolder() const;
  bool torrentDownloadFallbackOnFolder() const;
  std::string torrentDownloadLocation() const;
  bool torrentDownloadUseAnimeFolder() const;
  bool torrentDownloadOpen() const;
  bool torrentDownloadUseMagnet() const;
  std::string torrentDownloadAppMode() const;
  std::string torrentDownloadAppPath() const;
  std::string torrentDownloadFileLocation() const;
  std::optional<QJsonArray> torrentFilters() const;
  std::string service() const;
  std::vector<std::string> libraryFolders() const;
  bool libraryWatchFolders() const;
  bool libraryScanOnStartup() const;
  qint64 libraryMinimumFileSize() const;
  bool mediaDetectionEnabled() const;
  std::chrono::milliseconds mediaDetectionInterval() const;
  std::vector<std::string> recognitionIgnoredStrings() const;
  bool recognitionLookupParentDirectories() const;
  QNetworkProxy::ProxyType proxyType() const;
  std::string proxyHost() const;
  int proxyPort() const;
  std::string proxyUsername() const;
  std::string proxyPassword() const;
  bool sidebarVisible() const;
  bool appAutoStart() const;
  bool appCloseToTray() const;
  bool appMinimizeToTray() const;
  bool appStartMinimized() const;
  std::string listDoubleClickAction() const;
  std::string listMiddleClickAction() const;
  bool listHighlightNewEpisodes() const;
  bool listHighlightedOnTop() const;
  bool listShowAiredEpisodes() const;
  bool listShowAvailableEpisodes() const;
  bool streamingMediaEnabled() const;
  bool sharingEnabled() const;
  bool httpShareEnabled() const;
  std::string httpShareUrl() const;
  std::string httpShareFormat() const;
  bool ircShareEnabled() const;
  std::string ircShareFormat() const;
  std::string ircShareChannels() const;
  bool ircShareAllChannels() const;
  bool ircShareUseAction() const;
  bool discordEnabled() const;
  std::string discordApplicationId() const;
  bool discordTimeEnabled() const;
  bool discordUsernameEnabled() const;
  bool discordGroupEnabled() const;
  bool syncEnabled() const;
  bool syncOnStartup() const;
  bool syncGoToNowPlayingNotRecognized() const;
  bool syncGoToNowPlayingRecognized() const;
  bool syncNotifyNotRecognized() const;
  bool syncNotifyRecognized() const;
  std::string syncNotifyFormat() const;
  anime::TitleLanguage titleLanguage() const;
  bool updateAskToConfirm() const;
  std::chrono::seconds updateDelay() const;
  bool updateLibraryOnly() const;
  bool updateOutOfRange() const;
  bool updatePauseWhenUnfocused() const;
  track::UpdateTrigger updateTrigger() const;

  void setAppColorScheme(const Qt::ColorScheme scheme) const;
  void setAppStyle(const std::string& style) const;
  void setExternalLinks(std::vector<std::string> links) const;
  void setDisabledMediaPlayers(std::vector<std::string> players) const;
  void setDisabledStreamingProviders(std::vector<std::string> providers) const;
  void setTorrentDiscoveryUrl(const std::string& url) const;
  void setTorrentSearchUrl(const std::string& url) const;
  void setTorrentAutoCheckEnabled(const bool enabled) const;
  void setTorrentAutoCheckInterval(const std::chrono::minutes interval) const;
  void setTorrentNotifyNewEpisodes(const bool enabled) const;
  void setTorrentDownloadNewEpisodes(const bool enabled) const;
  void setTorrentFilterEnabled(const bool enabled) const;
  void setTorrentArchiveMaxCount(const int count) const;
  void setTorrentDownloadSortBy(const std::string& sortBy) const;
  void setTorrentDownloadSortOrder(const Qt::SortOrder order) const;
  void setTorrentDownloadCreateSubfolder(const bool enabled) const;
  void setTorrentDownloadFallbackOnFolder(const bool enabled) const;
  void setTorrentDownloadLocation(const std::string& path) const;
  void setTorrentDownloadUseAnimeFolder(const bool enabled) const;
  void setTorrentDownloadOpen(const bool open) const;
  void setTorrentDownloadUseMagnet(const bool use) const;
  void setTorrentDownloadAppMode(const std::string& mode) const;
  void setTorrentDownloadAppPath(const std::string& path) const;
  void setTorrentDownloadFileLocation(const std::string& path) const;
  void setTorrentFilters(const QJsonArray& filters) const;
  void setService(const std::string& service) const;
  void setLibraryFolders(std::vector<std::string> folders) const;
  void setLibraryWatchFolders(const bool watch) const;
  void setLibraryScanOnStartup(const bool scan) const;
  void setLibraryMinimumFileSize(const qint64 bytes) const;
  void setMediaDetectionEnabled(const bool enabled) const;
  void setMediaDetectionInterval(const std::chrono::milliseconds interval) const;
  void setRecognitionIgnoredStrings(std::vector<std::string> strings) const;
  void setRecognitionLookupParentDirectories(const bool lookup) const;
  void setProxyType(const QNetworkProxy::ProxyType type) const;
  void setProxyHost(const std::string& host) const;
  void setProxyPort(const int port) const;
  void setProxyUsername(const std::string& username) const;
  void setProxyPassword(const std::string& password) const;
  void setSidebarVisible(const bool visible) const;
  void setAppAutoStart(const bool enabled) const;
  void setAppCloseToTray(const bool enabled) const;
  void setAppMinimizeToTray(const bool enabled) const;
  void setAppStartMinimized(const bool enabled) const;
  void setListDoubleClickAction(const std::string& action) const;
  void setListMiddleClickAction(const std::string& action) const;
  void setListHighlightNewEpisodes(const bool highlight) const;
  void setListHighlightedOnTop(const bool onTop) const;
  void setListShowAiredEpisodes(const bool show) const;
  void setListShowAvailableEpisodes(const bool show) const;
  void setStreamingMediaEnabled(const bool enabled) const;
  void setSharingEnabled(const bool enabled) const;
  void setHttpShareEnabled(const bool enabled) const;
  void setHttpShareUrl(const std::string& url) const;
  void setHttpShareFormat(const std::string& format) const;
  void setIrcShareEnabled(const bool enabled) const;
  void setIrcShareFormat(const std::string& format) const;
  void setIrcShareChannels(const std::string& channels) const;
  void setIrcShareAllChannels(const bool all) const;
  void setIrcShareUseAction(const bool use) const;
  void setDiscordEnabled(const bool enabled) const;
  void setDiscordApplicationId(const std::string& id) const;
  void setDiscordTimeEnabled(const bool enabled) const;
  void setDiscordUsernameEnabled(const bool enabled) const;
  void setDiscordGroupEnabled(const bool enabled) const;
  void setSyncEnabled(const bool enabled) const;
  void setSyncOnStartup(const bool enabled) const;
  void setSyncGoToNowPlayingNotRecognized(const bool enabled) const;
  void setSyncGoToNowPlayingRecognized(const bool enabled) const;
  void setSyncNotifyNotRecognized(const bool enabled) const;
  void setSyncNotifyRecognized(const bool enabled) const;
  void setSyncNotifyFormat(const std::string& format) const;
  void setTitleLanguage(const anime::TitleLanguage language) const;
  void setUpdateAskToConfirm(const bool enabled) const;
  void setUpdateDelay(const std::chrono::seconds delay) const;
  void setUpdateLibraryOnly(const bool enabled) const;
  void setUpdateOutOfRange(const bool enabled) const;
  void setUpdatePauseWhenUnfocused(const bool enabled) const;
  void setUpdateTrigger(const track::UpdateTrigger trigger) const;

private:
  QString fileName() const override;

  mutable std::optional<anime::TitleLanguage> titleLanguageCache_;
};

inline Settings settings;

}  // namespace taiga
