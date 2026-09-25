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

#include "main_window.hpp"

#include <QDesktopServices>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkReply>
#include <QShortcut>
#include <QtWidgets>
#include <algorithm>
#include <optional>

#include "base/string.hpp"
#include "gui/common/spinner_widget.hpp"
#include "gui/history/history_widget.hpp"
#include "gui/library/library_widget.hpp"
#include "gui/list/list_widget.hpp"
#include "gui/main/about_dialog.hpp"
#include "gui/main/navigation_controller.hpp"
#include "gui/main/navigation_widget.hpp"
#include "gui/main/now_playing_page.hpp"
#include "gui/main/now_playing_widget.hpp"
#include "gui/main/status_bar.hpp"
#include "gui/main/status_bar_controller.hpp"
#include "gui/profile/profile_widget.hpp"
#include "gui/search/search_widget.hpp"
#include "gui/seasons/seasons_widget.hpp"
#include "gui/settings/settings_dialog.hpp"
#include "gui/torrents/torrents_widget.hpp"
#include "gui/utils/format.hpp"
#include "gui/utils/theme.hpp"
#include "gui/utils/tray_icon.hpp"
#include "gui/utils/widgets.hpp"
#include "link/discord.hpp"
#include "link/http.hpp"
#ifdef Q_OS_LINUX
#include "link/irc.hpp"
#endif
#include "akyuu/accounts.hpp"
#include "akyuu/application.hpp"
#include "akyuu/config.h"
#include "akyuu/network.hpp"
#include "akyuu/script.hpp"
#include "akyuu/session.hpp"
#include "akyuu/settings.hpp"
#include "akyuu/version.hpp"
#include "media/anime_db.hpp"
#include "media/anime_list.hpp"
#include "media/anime_list_export.hpp"
#include "media/anime_utils.hpp"
#include "sync/anilist/anilist.hpp"
#include "sync/kitsu/kitsu.hpp"
#include "sync/myanimelist/myanimelist.hpp"
#include "sync/queue.hpp"
#include "sync/service.hpp"
#include "track/episode.hpp"
#include "track/feed_aggregator.hpp"
#include "track/library.hpp"
#include "track/media.hpp"
#include "track/play.hpp"
#include "track/update_session.hpp"
#include "ui_main_window.h"

#ifdef Q_OS_WINDOWS
#include "gui/platforms/windows.hpp"
#endif

namespace gui {

MainWindow::MainWindow() : QMainWindow(), ui_(new Ui::MainWindow) {
  ui_->setupUi(this);

  // v1 has a menu bar, so the menu button that stands in for it is not needed.
  ui_->actionMenu->setVisible(false);

#ifdef Q_OS_WINDOWS
  enableMicaBackground(this);
#endif

  if (const auto geometry = akyuu::session.mainWindowGeometry(); !geometry.isEmpty()) {
    restoreGeometry(geometry);
    centerWidgetToScreen(this);
  }

  // Do not call `init()` here, as it relies on the main window pointer being
  // available through the application instance.
}

MainWindow* mainWindow() {
  return akyuu::app()->mainWindow();
}

NavigationWidget* MainWindow::navigation() const {
  return m_navigationWidget;
}

NowPlayingWidget* MainWindow::nowPlaying() const {
  return m_nowPlayingWidget;
}

QLineEdit* MainWindow::searchBox() const {
  return m_searchBox;
}

StatusBarController* MainWindow::statusBarController() const {
  return m_statusBarController;
}

Ui::MainWindow* MainWindow::ui() const {
  return ui_;
}

void MainWindow::init() {
  initActions();
  initShortcuts();
  initShareMenu();
  initIcons();
  initTrayIcon();
  initToolbar();
  initStatusbar();
  initNavigation();
  initNowPlaying();
  updateTitle();

  // v1 can synchronize the list as soon as it starts
  if (akyuu::settings.syncOnStartup()) synchronize();
}

void MainWindow::initShortcuts() {
  // v1's keys for the main window. They work wherever the focus is, like v1's accelerators.
  const auto focusSearch = [this]() {
    if (!m_searchBox) return;
    m_searchBox->setFocus(Qt::ShortcutFocusReason);
    m_searchBox->selectAll();
  };
  for (const auto& key : {QKeySequence{QKeySequence::Find}, QKeySequence{Qt::Key_F3}}) {
    connect(new QShortcut(key, this), &QShortcut::activated, this, focusSearch);
  }

  connect(new QShortcut(QKeySequence{Qt::CTRL | Qt::Key_Tab}, this), &QShortcut::activated, this,
          [this]() { m_navigationController->cycleListStatus(1); });
  connect(new QShortcut(QKeySequence{Qt::CTRL | Qt::SHIFT | Qt::Key_Backtab}, this),
          &QShortcut::activated, this, [this]() { m_navigationController->cycleListStatus(-1); });
}

// v1's "Announce current episode" submenu, in the same List menu
void MainWindow::initShareMenu() {
  const auto menu = new QMenu(tr("Share current episode"), this);
  menu->addAction(u"Discord"_s, this, [this]() { announceCurrentEpisode(ShareChannel::Discord); });
  menu->addAction(u"HTTP"_s, this, [this]() { announceCurrentEpisode(ShareChannel::Http); });
#ifdef Q_OS_LINUX
  menu->addAction(u"IRC"_s, this, [this]() { announceCurrentEpisode(ShareChannel::Irc); });
#endif

  ui_->menuList->insertMenu(ui_->menuExport->menuAction(), menu);
  ui_->menuList->insertSeparator(ui_->menuExport->menuAction());

  // Only something recognized can be shared, as in v1
  connect(ui_->menuList, &QMenu::aboutToShow, this, [menu]() {
    const auto episode = track::media::detection()->getCurrentEpisode();
    menu->setEnabled(episode && episode->animeId() != anime::kUnknownId);
  });
}

void MainWindow::initActions() {
  ui_->actionProfile->setToolTip(tr("Profile"));
  ui_->actionSynchronize->setToolTip(
      tr("Synchronize with %1").arg(sync::serviceName(sync::currentServiceId())));

  connect(ui_->actionAddNewFolder, &QAction::triggered, this, &MainWindow::addNewFolder);
  connect(ui_->actionExit, &QAction::triggered, this, &QApplication::quit, Qt::QueuedConnection);
  connect(ui_->actionSettings, &QAction::triggered, this, [this]() { SettingsDialog::show(this); });
  connect(ui_->actionLibraryFolders, &QAction::triggered, this,
          [this]() { SettingsDialog::show(this, SettingsPageId::Library); });
  // Both actions pick the anime themselves, so a failure has to be said out loud. v1 shows a
  // message box; v2 already reports playback this way.
  const auto playbackFailed = [this](const QString& text) {
    m_statusBarController->showMessage({
        .source = StatusBarController::Source::Playback,
        .text = text,
        .spin = false,
    });
  };
  connect(ui_->actionPlayNextEpisode, &QAction::triggered, this, [playbackFailed]() {
    if (!track::playNextEpisodeOfLastWatchedAnime()) {
      playbackFailed(tr("Could not find the next episode of the last anime you watched."));
    }
  });
  connect(ui_->actionPlayRandomAnime, &QAction::triggered, this, [playbackFailed]() {
    if (!track::playRandomAnime()) {
      playbackFailed(tr("Could not find any episode to play."));
    }
  });
  connect(ui_->actionExportListAsMarkdown, &QAction::triggered, this,
          [this]() { exportList(ExportFormat::Markdown); });
  connect(ui_->actionExportListAsMyAnimeListXML, &QAction::triggered, this,
          [this]() { exportList(ExportFormat::MyAnimeListXml); });
  connect(ui_->menuExternalLinks, &QMenu::aboutToShow, this, &MainWindow::initExternalLinksMenu);
  connect(ui_->menuServices, &QMenu::aboutToShow, this, &MainWindow::initServicesMenu);
  connect(ui_->menuView, &QMenu::aboutToShow, this, &MainWindow::initViewMenu);
  connect(ui_->actionAbout, &QAction::triggered, this, &MainWindow::about);
  connect(ui_->actionProfile, &QAction::triggered, this, &MainWindow::profile);
  connect(ui_->actionDisplayWindow, &QAction::triggered, this, &MainWindow::displayWindow);
  connect(ui_->actionSynchronize, &QAction::triggered, this, &MainWindow::synchronize);

  connect(ui_->actionScanAvailableEpisodes, &QAction::triggered, this, [this]() {
    m_statusBarController->showMessage({
        .source = StatusBarController::Source::Library,
        .text = tr("Scanning available episodes..."),
    });
    track::library()->scan();
  });
  connect(track::library(), &track::Library::scanCompleted, this,
          [this](const int animeCount, const int episodeCount) {
            m_statusBarController->showMessage({
                .source = StatusBarController::Source::Library,
                .text = tr("Found %1 episodes of %2 anime.").arg(episodeCount).arg(animeCount),
                .spin = false,
            });
          });

  ui_->actionToggleDetection->setChecked(akyuu::settings.mediaDetectionEnabled());
  connect(ui_->actionToggleDetection, &QAction::toggled, this, [](const bool checked) {
    akyuu::settings.setMediaDetectionEnabled(checked);
    track::media::detection()->setEnabled(checked);
  });
  connect(track::media::detection(), &track::media::Detection::enabledChanged, this,
          [this](const bool enabled) {
            const QSignalBlocker blocker(ui_->actionToggleDetection);
            ui_->actionToggleDetection->setChecked(enabled);
          });

  ui_->actionToggleSharing->setChecked(akyuu::settings.sharingEnabled());
  connect(ui_->actionToggleSharing, &QAction::toggled, this, [](const bool checked) {
    akyuu::settings.setSharingEnabled(checked);
    // Like v1, turning it back on does not repeat the current episode; the next one is shared.
    if (!checked) link::discord()->clearPresence();
  });

  connect(ui_->actionCheckForUpdates, &QAction::triggered, this, &MainWindow::checkForUpdates);

  ui_->actionToggleSynchronization->setChecked(akyuu::settings.syncEnabled());
  connect(ui_->actionToggleSynchronization, &QAction::toggled, this,
          [](const bool checked) { akyuu::settings.setSyncEnabled(checked); });
}

void MainWindow::initIcons() {
  ui_->menuLibraryFolders->setIcon(theme.getIcon("folder"));
  ui_->menuExport->setIcon(theme.getIcon("export_notes"));

  ui_->actionAddNewFolder->setIcon(theme.getIcon("create_new_folder"));
  ui_->actionAbout->setIcon(theme.getIcon("info"));
  ui_->actionBack->setIcon(theme.getIcon("arrow_back"));
  ui_->actionCheckForUpdates->setIcon(theme.getIcon("cloud_download"));
  ui_->actionDonate->setIcon(theme.getIcon("favorite"));
  ui_->actionExit->setIcon(theme.getIcon("logout"));
  ui_->actionForward->setIcon(theme.getIcon("arrow_forward"));
  ui_->actionLibraryFolders->setIcon(theme.getIcon("folder"));
  ui_->actionMenu->setIcon(theme.getIcon("menu"));
  ui_->actionPlayNextEpisode->setIcon(theme.getIcon("skip_next"));
  ui_->actionPlayRandomAnime->setIcon(theme.getIcon("shuffle"));
  ui_->actionProfile->setIcon(theme.getIcon("account_circle"));
  ui_->actionScanAvailableEpisodes->setIcon(theme.getIcon("pageview"));
  ui_->actionSettings->setIcon(theme.getIcon("settings"));
  ui_->actionSupport->setIcon(theme.getIcon("help"));
  ui_->actionSynchronize->setIcon(theme.getIcon("sync"));
}

void MainWindow::initNavigation() {
  m_navigationWidget = new NavigationWidget(this);
  m_navigationWidget->setVisible(akyuu::settings.sidebarVisible());
  // Connects to m_navigationWidget's signals on construction, so it must come after.
  m_navigationController = new NavigationController(this);

  const bool hasWatching = std::ranges::any_of(anime::db.entries(), [](const auto& entry) {
    return entry.status == anime::list::Status::Watching;
  });
  if (hasWatching) {
    navigateToListStatus(anime::list::Status::Watching);
  } else {
    navigateTo(MainWindowPage::List);
  }

  ui_->splitter->insertWidget(0, m_navigationWidget);
}

void MainWindow::initNowPlaying() {
  m_nowPlayingWidget = new NowPlayingWidget(ui_->centralWidget);

  ui_->centralWidget->layout()->addWidget(m_nowPlayingWidget);
  m_nowPlayingWidget->hide();

  connect(track::media::detection(), &track::media::Detection::currentEpisodeChanged, this,
          &MainWindow::notifyEpisodeDetected);

  connect(track::media::detection(), &track::media::Detection::currentEpisodeChanged, this,
          &MainWindow::shareEpisode);
}

void MainWindow::shareEpisode(const std::optional<track::Episode>& episode) const {
  if (!episode) {
    link::discord()->clearPresence();
    return;
  }

  // v1 keeps entries marked private out of every sharing channel. Anything already shared is
  // cleared first, so the previous episode is not left on display. Turning sharing off does the
  // same.
  const auto entry = anime::db.entry(episode->animeId());
  if ((entry && entry->is_private) || !akyuu::settings.sharingEnabled()) {
    link::discord()->clearPresence();
    return;
  }

  link::http::announce(*episode);
#ifdef Q_OS_LINUX
  link::irc::announce(*episode);
#endif
  updateDiscordPresence(*episode, false);
}

void MainWindow::updateDiscordPresence(const track::Episode& episode, const bool force) const {
  const auto item = anime::db.item(episode.animeId());
  const auto title = item ? QString::fromStdString(anime::preferredTitle(*item))
                          : QString::fromStdString(episode.element(anitomy::ElementKind::Title));

  // The same lines as v1: "Episode 5/12 by Group", the group being optional.
  auto state = u"$if(%episode%,Episode %episode%$if(%total%,/%total%) )"_s;
  if (akyuu::settings.discordGroupEnabled()) state += u"$if(%group%,by %group%)"_s;

  // v1 shares what is playing over Discord's rich presence.
  link::discord()->updatePresence(title, akyuu::replaceVariables(state, episode),
                                  item ? QString::fromStdString(item->image_url) : QString{},
                                  std::time(nullptr), force);
}

// v1's "Announce current episode" menu: shares on one channel on request, even when that channel
// or sharing as a whole is turned off. Unlike v1 it still keeps private entries to itself.
void MainWindow::announceCurrentEpisode(const ShareChannel channel) {
  const auto episode = track::media::detection()->getCurrentEpisode();
  if (!episode || episode->animeId() == anime::kUnknownId) return;

  if (const auto entry = anime::db.entry(episode->animeId()); entry && entry->is_private) {
    m_statusBarController->showMessage({
        .source = StatusBarController::Source::Playback,
        .text = tr("This anime is private on your list, so it is not shared."),
        .spin = false,
    });
    return;
  }

  switch (channel) {
    case ShareChannel::Discord:
      updateDiscordPresence(*episode, true);
      break;
    case ShareChannel::Http:
      link::http::announce(*episode, true);
      break;
    case ShareChannel::Irc:
#ifdef Q_OS_LINUX
      link::irc::announce(*episode, true);
#endif
      break;
  }
}

// v1's `RefreshSearchText`: every page keeps its own text, and the box says what it does there.
void MainWindow::switchSearchText(MainWindowPage from, MainWindowPage to) {
  if (!m_searchBox || from == to) return;

  m_searchTexts[from] = m_searchBox->text();

  const auto service = sync::serviceName(sync::currentServiceId());
  QString placeholder;
  switch (to) {
    case MainWindowPage::List:
    case MainWindowPage::Seasons:
      placeholder = tr("Filter list");
      break;
    case MainWindowPage::Search:
      placeholder = tr("Search %1 for anime").arg(service);
      break;
    case MainWindowPage::Torrents:
      placeholder = tr("Filter torrents");
      break;
    default:
      break;
  }

  // Pages that do nothing with the text get an empty, disabled box rather than a stale one.
  m_searchBox->setEnabled(!placeholder.isEmpty());
  m_searchBox->setPlaceholderText(placeholder);
  m_searchBox->setText(placeholder.isEmpty() ? QString{} : m_searchTexts[to]);
}

void MainWindow::initPage(MainWindowPage page) {
  static QSet<MainWindowPage> initializedPages;

  if (initializedPages.contains(page)) return;

  static const auto init_page = [](QWidget* page, QWidget* widget) {
    const auto layout = new QHBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(widget);
  };

  switch (page) {
    case MainWindowPage::Home:
      init_page(ui_->homePage, new NowPlayingPage(ui_->homePage));
      break;

    case MainWindowPage::Search:
      m_searchWidget = new SearchWidget(ui_->searchPage);
      init_page(ui_->searchPage, m_searchWidget);
      break;

    case MainWindowPage::List:
      m_listWidget = new ListWidget(ui_->listPage);
      init_page(ui_->listPage, m_listWidget);
      break;

    case MainWindowPage::History:
      m_historyWidget = new HistoryWidget(ui_->historyPage);
      init_page(ui_->historyPage, m_historyWidget);
      break;

    case MainWindowPage::Library:
      m_libraryWidget = new LibraryWidget(ui_->libraryPage);
      init_page(ui_->libraryPage, m_libraryWidget);
      break;

    case MainWindowPage::Seasons:
      m_seasonsWidget = new SeasonsWidget(ui_->seasonsPage);
      init_page(ui_->seasonsPage, m_seasonsWidget);
      break;

    case MainWindowPage::Torrents:
      m_torrentsWidget = new TorrentsWidget(ui_->torrentsPage);
      init_page(ui_->torrentsPage, m_torrentsWidget);
      break;

    case MainWindowPage::Profile:
      m_profileWidget = new ProfileWidget(ui_->profilePage);
      init_page(ui_->profilePage, m_profileWidget);
      break;
  }

  initializedPages.insert(page);
}

void MainWindow::initStatusbar() {
  const auto statusbar = new StatusBar(this);
  statusbar->setObjectName(ui_->statusbar->objectName());
  setStatusBar(statusbar);
  ui_->statusbar = statusbar;

  const auto spinner = new SpinnerWidget(this);
  const auto spinnerContainer = new QWidget(this);
  const auto spinnerLayout = new QVBoxLayout(spinnerContainer);
  spinnerLayout->setContentsMargins(0, 2, 0, 0);
  spinnerLayout->addWidget(spinner);
  ui_->statusbar->addPermanentWidget(spinnerContainer);

  m_statusBarController = new StatusBarController(this, statusbar, spinner);

  const QList<sync::Service*> services{
      sync::anilist::Service::instance(),
      sync::kitsu::Service::instance(),
      sync::myanimelist::Service::instance(),
  };
  for (auto* service : services) {
    connect(service, &sync::Service::authenticationCompleted, this,
            [this](const bool authenticated) {
              if (!authenticated) return;

              const auto sender_service = qobject_cast<sync::Service*>(sender());
              const auto slug = sync::serviceSlug(sender_service->id()).toStdString();
              const auto username = akyuu::accounts.serviceUsername(slug);

              m_statusBarController->showMessage({
                  .source = StatusBarController::Source::Sync,
                  .text = tr("Logged in as %1.").arg(username),
                  .spin = false,
              });
            });
    connect(service, &sync::Service::listEntriesFetched, this, [this]() {
      m_statusBarController->clearMessage(StatusBarController::Source::Sync);
      setEnabled(true);
    });
    connect(service, &sync::Service::errorOccurred, this, [this](const QString& message) {
      const auto sender_service = qobject_cast<sync::Service*>(sender());
      m_statusBarController->showMessage({
          .source = StatusBarController::Source::Sync,
          .text = sync::tagMessage(sender_service->id(), message),
          .spin = false,
      });
      setEnabled(true);
    });
    connect(service, &sync::Service::transferProgress, this,
            [this](const qint64 current, const qint64 total) {
              m_statusBarController->showMessage({
                  .source = StatusBarController::Source::Sync,
                  .text = tr("Synchronizing with %1... (%2)")
                              .arg(sync::serviceName(sync::currentServiceId()))
                              .arg(gui::formatTransferProgress(current, total)),
              });
            });
  }

  connect(&sync::queue, &sync::Queue::changed, this, [this]() {
    if (sync::queue.count() == 0) {
      m_statusBarController->clearMessage(StatusBarController::Source::Sync);
      setEnabled(true);
    }
  });

  connect(&sync::queue, &sync::Queue::processing, this, [this](const int animeId) {
    const auto item = anime::db.item(animeId);
    const auto entry = anime::db.entry(animeId);
    if (!item || !entry) return;

    const auto title = anime::preferredTitle(*item);

    QString text;
    if (entry->pending_delete) {
      text = tr("Deleting list entry... (%1)").arg(title);
    } else if (entry->id == anime::list::kUnknownId) {
      text = tr("Adding to list... (%1)").arg(title);
    } else {
      text = tr("Updating list entry... (%1)").arg(title);
    }

    m_statusBarController->showMessage({
        .source = StatusBarController::Source::Sync,
        .text = text,
    });
  });

  connect(&sync::queue, &sync::Queue::queuedWhileUnauthenticated, this, [this](const int animeId) {
    const auto item = anime::db.item(animeId);
    if (!item) return;

    m_statusBarController->showMessage({
        .source = StatusBarController::Source::Sync,
        .text = tr("%1 is queued for update.").arg(anime::preferredTitle(*item)),
        .spin = false,
    });
  });

  connect(&anime::db, &anime::Database::itemDeleted, this, [this](const int, const QString& title) {
    if (title.isEmpty()) return;

    m_statusBarController->showMessage({
        .source = StatusBarController::Source::Sync,
        .text = tr("Anime removed from database: %1").arg(title),
        .spin = false,
    });
  });
}

// v1 asks for a folder and names the file itself. A save dialog does the same job while letting
// the name be changed, which is what a Linux user expects.
void MainWindow::exportList(const ExportFormat format) {
  const bool markdown = format == ExportFormat::Markdown;

  const auto name = u"animelist_%1.%2"_s.arg(QDateTime::currentSecsSinceEpoch())
                        .arg(markdown ? u"md"_s : u"xml"_s);
  const auto filter = markdown ? tr("Markdown (*.md)") : tr("MyAnimeList XML (*.xml)");

  const auto path = QFileDialog::getSaveFileName(
      this, tr("Select Export Location"), u"%1/%2"_s.arg(QDir::homePath()).arg(name), filter);

  if (path.isEmpty()) return;

  const auto report = [this](const QString& text, const bool spin = false) {
    m_statusBarController->showMessage({
        .source = StatusBarController::Source::Export,
        .text = text,
        .spin = spin,
    });
  };

  if (markdown) {
    report(anime::list::exportAsMarkdown(path.toStdString())
               ? tr("Exported list to: %1").arg(path)
               : tr("Could not export list to: %1").arg(path));
    return;
  }

  const auto service = sync::currentServiceId();

  // An AniList list knows its MyAnimeList IDs only by asking AniList, so they are looked up first.
  if (service == sync::ServiceId::AniList) {
    report(tr("Looking up MyAnimeList IDs..."), true);
    const auto ids = anime::db.entries() |
                     std::views::transform([](const auto& entry) { return entry.anime_id; }) |
                     std::ranges::to<QList>();
    sync::anilist::Service::instance()->fetchMalIds(
        ids, [report, path](const std::optional<QMap<int, int>>& malIds) {
          if (!malIds) {
            report(tr("Could not look up MyAnimeList IDs, so the list was not exported."));
            return;
          }
          int skipped = 0;
          if (!anime::list::exportAsXml(path.toStdString(), &*malIds, &skipped)) {
            report(tr("Could not export list to: %1").arg(path));
            return;
          }
          auto text = tr("Exported list to: %1").arg(path);
          if (skipped > 0) {
            text += u" "_s + tr("(%1 anime left out, MyAnimeList does not have them)").arg(skipped);
          }
          report(text);
        });
    return;
  }

  const auto exported = anime::list::exportAsXml(path.toStdString());

  auto text = exported ? tr("Exported list to: %1").arg(path)
                       : tr("Could not export list to: %1").arg(path);

  // Kitsu's IDs would end up in a MyAnimeList field. Saying so beats handing over a file that looks
  // importable.
  if (exported && service != sync::ServiceId::MyAnimeList) {
    text += u" "_s + tr("(anime IDs are %1's, not MyAnimeList's)").arg(sync::serviceName(service));
  }

  report(text);
}

// v1 downloads its NSIS installer and runs it silently (`/S /D=<folder>`) to replace itself. That
// is a Windows mechanism; on Linux Akyuu is installed by the user or a package manager, so the
// honest thing to do is say whether there is something newer and where to get it.
void MainWindow::checkForUpdates() {
  static const QUrl url{u"https://api.github.com/repos/ligneo/akyuu/releases/latest"_s};

  QNetworkRequest request{url};
  request.setHeaders(akyuu::NetworkAccessManager::commonHeaders());

  const auto reply = akyuu::network()->get(request);

  m_statusBarController->showMessage({
      .source = StatusBarController::Source::Sync,
      .text = tr("Checking for updates..."),
  });

  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    reply->deleteLater();
    m_statusBarController->clearMessage(StatusBarController::Source::Sync);

    if (reply->error() != QNetworkReply::NoError) {
      QMessageBox::warning(this, tr("Check for Updates"),
                           tr("Could not check for updates: %1").arg(reply->errorString()));
      return;
    }

    const auto json = QJsonDocument::fromJson(reply->readAll()).object();
    auto tag = json[u"tag_name"_s].toString();
    if (tag.startsWith(u'v')) tag.remove(0, 1);

    const semaver::Version latest{tag.toStdString()};
    const auto& current = akyuu::version();

    if (!latest || !(latest > current)) {
      QMessageBox::information(
          this, tr("Check for Updates"),
          tr("You are using the latest version (%1). The newest release is %2.")
              .arg(QString::fromStdString(current.to_string()))
              .arg(tag.isEmpty() ? tr("unknown") : tag));
      return;
    }

    const auto page = json[u"html_url"_s].toString();
    const auto answer =
        QMessageBox::question(this, tr("Check for Updates"),
                              tr("Akyuu %1 is available (you have %2). Open the release page?")
                                  .arg(tag)
                                  .arg(QString::fromStdString(current.to_string())));
    if (answer == QMessageBox::Yes) QDesktopServices::openUrl(QUrl{page});
  });
}

// v1 builds this menu from a setting, one `Name|URL` per line, with "-" for a separator.
void MainWindow::initExternalLinksMenu() {
  ui_->menuExternalLinks->clear();

  for (const auto& link : akyuu::settings.externalLinks()) {
    const auto text = QString::fromStdString(link);

    if (text.trimmed() == u"-"_s) {
      ui_->menuExternalLinks->addSeparator();
      continue;
    }

    const auto parts = text.split(u'|');
    if (parts.size() < 2) continue;

    const auto url = parts.at(1);
    ui_->menuExternalLinks->addAction(parts.at(0), this,
                                      [url]() { QDesktopServices::openUrl(QUrl{url}); });
  }
}

// v1 lists the user pages of all three services. Only the one being used is of any help here.
void MainWindow::initServicesMenu() {
  // Synchronize already lives in the List menu, so this one only carries the user pages.
  ui_->menuServices->clear();

  const auto addLink = [this](const QString& text, const QString& url) {
    ui_->menuServices->addAction(text, this, [url]() { QDesktopServices::openUrl(QUrl{url}); });
  };

  const auto service = sync::currentServiceId();

  switch (service) {
    case sync::ServiceId::AniList: {
      const auto user = QString::fromStdString(akyuu::accounts.anilistUsername());
      if (user.isEmpty()) break;
      addLink(tr("Go to my profile"), u"https://anilist.co/user/%1"_s.arg(user));
      addLink(tr("Go to my stats"), u"https://anilist.co/user/%1/stats"_s.arg(user));
      break;
    }
    case sync::ServiceId::Kitsu: {
      const auto user = QString::fromStdString(akyuu::accounts.kitsuUsername());
      if (user.isEmpty()) break;
      addLink(tr("Go to my feed"), u"https://kitsu.app"_s);
      addLink(tr("Go to my library"), u"https://kitsu.app/users/%1/library"_s.arg(user));
      addLink(tr("Go to my profile"), u"https://kitsu.app/users/%1"_s.arg(user));
      break;
    }
    case sync::ServiceId::MyAnimeList: {
      const auto user = QString::fromStdString(akyuu::accounts.myanimelistUsername());
      if (user.isEmpty()) break;
      addLink(tr("Go to my panel"), u"https://myanimelist.net/panel.php"_s);
      addLink(tr("Go to my profile"), u"https://myanimelist.net/profile/%1"_s.arg(user));
      addLink(tr("Go to my history"), u"https://myanimelist.net/history/%1"_s.arg(user));
      break;
    }
    default:
      break;
  }
}

// v1's View menu. The sidebar does the same job, but the menu is where the keyboard reaches it.
void MainWindow::initViewMenu() {
  ui_->menuView->clear();

  static const QList<QPair<QString, MainWindowPage>> pages{
      {tr("Home"), MainWindowPage::Home},       {tr("Anime List"), MainWindowPage::List},
      {tr("History"), MainWindowPage::History}, {tr("Profile"), MainWindowPage::Profile},
      {tr("Search"), MainWindowPage::Search},   {tr("Library"), MainWindowPage::Library},
      {tr("Seasons"), MainWindowPage::Seasons}, {tr("Torrents"), MainWindowPage::Torrents},
  };

  for (const auto& [text, page] : pages) {
    ui_->menuView->addAction(text, this, [this, page]() { navigateTo(page); });
  }

  ui_->menuView->addSeparator();
  ui_->menuView->addAction(ui_->actionBack);
  ui_->menuView->addAction(ui_->actionForward);
  ui_->menuView->addSeparator();

  const auto action = ui_->menuView->addAction(tr("Show sidebar"), this, [this](bool checked) {
    m_navigationWidget->setVisible(checked);
    akyuu::settings.setSidebarVisible(checked);
  });
  action->setCheckable(true);
  action->setChecked(m_navigationWidget->isVisible());
}

// v1's `program/general/minimize`. Qt has no separate minimize event, so the state change is
// what tells us.
void MainWindow::changeEvent(QEvent* event) {
  if (event->type() == QEvent::WindowStateChange && isMinimized()) {
    if (akyuu::settings.appMinimizeToTray() && m_trayIcon && m_trayIcon->isVisible()) {
      QTimer::singleShot(0, this, &QWidget::hide);
    }
  }

  QMainWindow::changeEvent(event);
}

void MainWindow::initToolbar() {
  ui_->toolbar->setIconSize(QSize{24, 24});

  // Menu
  {
    const auto button = static_cast<QToolButton*>(ui_->toolbar->widgetForAction(ui_->actionMenu));
    button->setPopupMode(QToolButton::InstantPopup);
    // The menu bar is hidden, so this button is the only way to reach it. It used to carry a few
    // of its entries, which left the rest of them unreachable.
    button->setMenu([this]() {
      auto menu = new QMenu(this);
      menu->addMenu(ui_->menuList);
      menu->addMenu(ui_->menuLibrary);
      menu->addMenu(ui_->menuServices);
      menu->addMenu(ui_->menuView);
      menu->addMenu(ui_->menuTools);
      menu->addSeparator();
      menu->addMenu(ui_->menuHelp);
      menu->addSeparator();
      menu->addAction(ui_->actionExit);
      return menu;
    }());
  }

  // Search box
  {
    m_searchBox = new QLineEdit();
    m_searchBox->setClearButtonEnabled(true);
    m_searchBox->setFixedWidth(320);
    m_searchBox->setPlaceholderText(tr("Search"));

    const auto before = ui_->actionSettings;
    const auto insertSpacer = [this](QAction* before) {
      ui_->toolbar->insertWidget(before, [this]() {
        auto spacer = new QWidget(this);
        spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        return spacer;
      }());
    };

    // v1 keeps the search box at the right end
    insertSpacer(before);
    ui_->toolbar->insertWidget(before, m_searchBox);
  }
}

void MainWindow::initTrayIcon() {
  auto menu = new QMenu(this);
  menu->addAction(ui_->actionDisplayWindow);
  menu->setDefaultAction(ui_->actionDisplayWindow);
  menu->addSeparator();
  // v1's tray menu reaches the same folders, service pages and links as the main one
  menu->addMenu(ui_->menuLibraryFolders);
  menu->addMenu(ui_->menuServices);
  menu->addMenu(ui_->menuExternalLinks);
  menu->addSeparator();
  menu->addAction(ui_->actionSettings);
  menu->addSeparator();
  menu->addAction(ui_->actionExit);

  m_trayIcon = new TrayIcon(this, menu);
  m_trayIcon->setSleeping(!akyuu::settings.mediaDetectionEnabled());

  connect(track::media::detection(), &track::media::Detection::enabledChanged, this,
          [this](const bool enabled) { m_trayIcon->setSleeping(!enabled); });

  connect(track::aggregator(), &track::Aggregator::newEpisodesFound, this,
          [this](const QStringList& lines) {
            m_trayIcon->showMessage(tr("New torrents available"), lines.join(u'\n'));
          });

  connect(m_trayIcon, &TrayIcon::activated, this, &MainWindow::displayWindow);
  connect(m_trayIcon, &TrayIcon::messageClicked, this, &MainWindow::displayWindow);

  connect(track::updateSession(), &track::UpdateSession::confirmationRequested, this,
          [this](const track::UpdateState& state) {
            if (isActiveWindow()) return;

            const auto episode = track::media::detection()->getCurrentEpisode();
            const auto item = episode ? anime::db.item(episode->animeId()) : nullptr;
            if (!item) return;

            const auto title = QString::fromStdString(anime::preferredTitle(*item));

            if (state.reason == track::UpdateDecision::Reason::SkipsAhead) {
              m_trayIcon->showMessage(tr("Confirm list update"),
                                      tr("%1: episode %2 is ahead of your progress (%3).")
                                          .arg(title)
                                          .arg(state.episode)
                                          .arg(state.previousEpisode));
            } else {
              m_trayIcon->showMessage(tr("Do you want to update your anime list?"),
                                      u"%1\n%2"_s.arg(title, tr("Episode %1").arg(state.episode)));
            }
          });

  connect(track::media::detection(), &track::media::Detection::currentEpisodeChanged, this,
          [this](std::optional<track::Episode> episode) {
            if (!episode) {
              m_trayIcon->setBadge(TrayIcon::Badge::None);
            } else {
              const bool recognized = anime::db.item(episode->animeId()) != nullptr;
              m_trayIcon->setBadge(recognized ? TrayIcon::Badge::Success : TrayIcon::Badge::Error);
            }
          });
}

void MainWindow::closeEvent(QCloseEvent* event) {
  // v1's `program/general/close`: the window goes away but Akyuu keeps detecting.
  if (akyuu::settings.appCloseToTray() && m_trayIcon && m_trayIcon->isVisible()) {
    hide();
    event->ignore();
    return;
  }

  akyuu::session.setMainWindowGeometry(saveGeometry());
  if (m_listWidget) m_listWidget->saveState();
  if (m_searchWidget) m_searchWidget->saveState();
  if (m_seasonsWidget) m_seasonsWidget->saveState();
  event->accept();
}

void MainWindow::addNewFolder() {
  constexpr auto options =
      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks | QFileDialog::ReadOnly;

  const auto directory = QFileDialog::getExistingDirectory(this, tr("Add New Folder"), "", options);

  if (!directory.isEmpty()) {
    QMessageBox::information(this, "New Folder", directory);
  }
}

void MainWindow::navigateTo(MainWindowPage page) {
  m_navigationController->navigateTo(page);
}

void MainWindow::navigateToListStatus(anime::list::Status status) {
  m_navigationController->navigateToListStatus(status);
}

void MainWindow::updateTitle() {
  auto title = u"Akyuu"_s;

  if (akyuu::app()->isDebug()) {
    title += u" [debug]"_s;
  }

  setWindowTitle(title);
}

void MainWindow::displayWindow() {
  setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
  activateWindow();
}

void MainWindow::about() {
  displayAboutDialog(this);
}

void MainWindow::notifyEpisodeDetected(std::optional<track::Episode> episode) {
  if (!episode || !m_trayIcon) return;

  const auto item = anime::db.item(episode->animeId());

  if (item ? akyuu::settings.syncGoToNowPlayingRecognized()
           : akyuu::settings.syncGoToNowPlayingNotRecognized()) {
    navigateTo(MainWindowPage::Home);
  }

  if (item) {
    if (!akyuu::settings.syncNotifyRecognized()) return;

    m_trayIcon->showMessage(
        tr("Episode recognized"),
        akyuu::replaceVariables(QString::fromStdString(akyuu::settings.syncNotifyFormat()),
                                *episode));
  } else {
    if (!akyuu::settings.syncNotifyNotRecognized()) return;

    m_trayIcon->showMessage(tr("Episode not recognized"),
                            QString::fromStdString(episode->element(anitomy::ElementKind::Title)));
  }
}

void MainWindow::synchronize() {
  setEnabled(false);

  const auto serviceName = sync::serviceName(sync::currentServiceId());
  const auto text = sync::willAuthenticate() ? tr("Authenticating with %1...").arg(serviceName)
                                             : tr("Synchronizing with %1...").arg(serviceName);

  m_statusBarController->showMessage({
      .source = StatusBarController::Source::Sync,
      .text = text,
  });

  if (!sync::synchronize()) {
    m_statusBarController->clearMessage(StatusBarController::Source::Sync);
    setEnabled(true);
  }
}

void MainWindow::profile() {
  navigateTo(MainWindowPage::Profile);
}

}  // namespace gui
