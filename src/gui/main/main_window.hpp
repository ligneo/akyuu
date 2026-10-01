/**
 * Akyuu
 * Copyright (C) 2010-2026, Eren Okka
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

#pragma once

#include <QMainWindow>
#include <map>
#include <optional>

#include "gui/main/navigation_controller.hpp"
#include "track/episode.hpp"

class QLineEdit;

namespace Ui {
class MainWindow;
}

namespace anime::list {
enum class Status;
}

namespace gui {

class HistoryWidget;
class LibraryWidget;
class ListWidget;
class NavigationWidget;
class NowPlayingWidget;
class ProfileWidget;
class SearchWidget;
class SeasonsWidget;
class TorrentsWidget;
class StatusBarController;
class TrayIcon;

enum class ExportFormat {
  Markdown,
  MyAnimeListXml,
};

class MainWindow final : public QMainWindow {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(MainWindow)

  friend class NavigationController;

public:
  enum class ShareChannel {
    Discord,
    Http,
    Irc,
  };

  MainWindow();
  ~MainWindow() = default;

  NavigationWidget* navigation() const;
  NowPlayingWidget* nowPlaying() const;
  QLineEdit* searchBox() const;
  StatusBarController* statusBarController() const;
  Ui::MainWindow* ui() const;

  void init();
  void announceCurrentEpisode(const ShareChannel channel);

public slots:
  void addNewFolder();
  void displayWindow();
  void navigateTo(MainWindowPage page);
  void navigateToListStatus(anime::list::Status status);
  void updateTitle();

private slots:
  void about();
  void notifyEpisodeDetected(std::optional<track::Episode> episode);
  void shareEpisode(const std::optional<track::Episode>& episode) const;
  void updateDiscordPresence(const track::Episode& episode, const bool force) const;
  void synchronize();
  void profile();

protected:
  void changeEvent(QEvent* event) override;
  void closeEvent(QCloseEvent* event) override;

private:
  void initActions();
  void saveSession();
  void initShortcuts();
  void initShareMenu();
  void initIcons();
  void initNavigation();
  void initNowPlaying();
  void initPage(MainWindowPage page);
  void switchSearchText(MainWindowPage from, MainWindowPage to);
  void initStatusbar();
  void exportList(const ExportFormat format);
  void checkForUpdates(bool silent = false);
  void initExternalLinksMenu();
  void initServicesMenu();
  void initViewMenu();
  void initToolbar();
  void initTrayIcon();

  Ui::MainWindow* ui_ = nullptr;

  HistoryWidget* m_historyWidget = nullptr;
  LibraryWidget* m_libraryWidget = nullptr;
  ListWidget* m_listWidget = nullptr;
  NavigationController* m_navigationController = nullptr;
  NavigationWidget* m_navigationWidget = nullptr;
  NowPlayingWidget* m_nowPlayingWidget = nullptr;
  ProfileWidget* m_profileWidget = nullptr;
  QLineEdit* m_searchBox = nullptr;
  std::map<MainWindowPage, QString> m_searchTexts;
  SearchWidget* m_searchWidget = nullptr;
  SeasonsWidget* m_seasonsWidget = nullptr;
  TorrentsWidget* m_torrentsWidget = nullptr;
  StatusBarController* m_statusBarController = nullptr;
  TrayIcon* m_trayIcon = nullptr;
};

MainWindow* mainWindow();

}  // namespace gui
