/**
 * Akyuu
 * Copyright (C) 2010-2024, Eren Okka
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

#include "settings_dialog.hpp"

#include <QTabWidget>

#include "base/string.hpp"
#include "gui/settings/settings_accounts_page.hpp"
#include "gui/settings/settings_advanced_page.hpp"
#include "gui/settings/settings_anime_list_page.hpp"
#include "gui/settings/settings_application_page.hpp"
#include "gui/settings/settings_cache_page.hpp"
#include "gui/settings/settings_discord_page.hpp"
#include "gui/settings/settings_http_page.hpp"
#include "gui/settings/settings_irc_page.hpp"
#include "gui/settings/settings_library_page.hpp"
#include "gui/settings/settings_media_players_page.hpp"
#include "gui/settings/settings_recognition_page.hpp"
#include "gui/settings/settings_streaming_page.hpp"
#include "gui/settings/settings_torrent_downloads_page.hpp"
#include "gui/settings/settings_torrent_filters_page.hpp"
#include "gui/settings/settings_torrents_page.hpp"
#include "gui/utils/theme.hpp"
#include "ui_settings_dialog.h"

#ifdef Q_OS_WINDOWS
#include "gui/platforms/windows.hpp"
#endif

namespace gui {

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent), ui_(new Ui::SettingsDialog) {
  ui_->setupUi(this);

#ifdef Q_OS_WINDOWS
  enableMicaBackground(this);
#endif

  ui_->treeWidget->setIndentation(0);

  // As in v1: a short list of sections, and the pages of a section as tabs.
  const auto add_item = [this](QString icon, QString text, QWidget* widget) {
    auto item = new QTreeWidgetItem(ui_->treeWidget, QStringList(text));
    item->setIcon(0, theme.getIcon(icon));
    item->setSizeHint(0, QSize{0, 32});
    item->setData(0, Qt::UserRole, ui_->stackedWidget->addWidget(widget));
  };

  const auto add_section = [this, add_item](QString icon, QString text) {
    auto tabs = new QTabWidget(this);
    tabs->setStyleSheet(u"QTabWidget::tab-bar { alignment: left; }"_s);  // some styles center them
    add_item(icon, text, tabs);
    return tabs;
  };

  // The accounts page brings its own tabs, one per service.
  const auto accountsPage = new AccountsPage(this);
  add_item("account_circle", "Services", accountsPage);
  accountsPage->load();
  pages_.push_back(accountsPage);
  const auto library = add_section("folder", "Library");
  const auto application = add_section("web_asset", "Application");
  const auto recognition = add_section("check_circle", "Recognition");
  const auto sharing = add_section("share", "Sharing");
  const auto torrents = add_section("rss_feed", "Torrents");
  const auto advanced = add_section("warning", "Advanced");

  addPage(library, "Folders", new LibraryPage(this));
  addPage(application, "Anime list", new AnimeListPage(this));
  addPage(application, "General", new ApplicationPage(this));
  addPage(recognition, "General", new RecognitionPage(this));
  addPage(recognition, "Media players", new MediaPlayersPage(this));
  addPage(recognition, "Streaming media", new StreamingPage(this));
  addPage(sharing, "Discord", new DiscordPage(this));
  addPage(sharing, "HTTP", new HttpPage(this));
#ifdef Q_OS_LINUX
  // v1 drives mIRC over DDE. Konversation takes its place here, so the tab is named after the
  // protocol rather than after the client.
  addPage(sharing, "IRC", new IrcPage(this));
#endif
  addPage(torrents, "Discovery", new TorrentsPage(this));
  addPage(torrents, "Downloads", new TorrentDownloadsPage(this));
  const auto filtersIndex = addPage(torrents, "Filters", new TorrentFiltersPage(this));
  const auto advancedPage = new AdvancedPage(this);
  addPage(advanced, "Settings", advancedPage);
  advanced->addTab(advancedPage->proxyPage(), "Proxy");
  addPage(advanced, "Cache", new CachePage(this));

  connect(ui_->treeWidget, &QTreeWidget::currentItemChanged, this,
          [this](QTreeWidgetItem* current) {
            if (!current) return;
            ui_->titleLabel->setText(current->text(0));
            ui_->stackedWidget->setCurrentIndex(current->data(0, Qt::UserRole).toInt());
          });

  items_ = {
      {SettingsPageId::Accounts, {ui_->treeWidget->topLevelItem(0), 0}},
      {SettingsPageId::Library, {ui_->treeWidget->topLevelItem(1), 0}},
      {SettingsPageId::TorrentFilters, {ui_->treeWidget->topLevelItem(5), filtersIndex}},
  };

  setCurrentPage(SettingsPageId::Accounts);
}

void SettingsDialog::setCurrentPage(const SettingsPageId page) {
  const auto it = items_.find(page);
  if (it == items_.end()) return;

  const auto [item, tab] = it->second;
  ui_->treeWidget->setCurrentItem(item);
  const auto index = item->data(0, Qt::UserRole).toInt();
  if (const auto tabs = qobject_cast<QTabWidget*>(ui_->stackedWidget->widget(index))) {
    tabs->setCurrentIndex(tab);
  }
}

void SettingsDialog::accept() {
  for (const auto page : pages_) {
    page->save();
  }

  QDialog::accept();
}

int SettingsDialog::addPage(QTabWidget* tabs, const QString& title, SettingsPage* page) {
  const auto index = tabs->addTab(page, title);
  page->load();
  pages_.push_back(page);
  return index;
}

void SettingsDialog::show(QWidget* parent, const SettingsPageId page) {
  auto dlg = new SettingsDialog(parent);
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->setModal(true);
  dlg->setCurrentPage(page);
  dlg->QDialog::show();
}

}  // namespace gui
