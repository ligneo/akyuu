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

  ui_->treeWidget->setIndentation(22);

  const auto add_item = [this](QString icon, QString text) {
    auto item = new QTreeWidgetItem(ui_->treeWidget, QStringList(text));
    item->setIcon(0, theme.getIcon(icon));
    item->setSizeHint(0, QSize{0, 24});
    return item;
  };

  const auto add_child = [this](QTreeWidgetItem* parent, QString text) {
    return new QTreeWidgetItem(parent, QStringList(text));
  };

  const auto accountsItem = add_item("account_circle", "Accounts");
  const auto applicationItem = add_item("web_asset", "Application");
  const auto animeListItem = add_item("list_alt", "Anime List");
  const auto libraryItem = add_item("folder", "Library");
  QTreeWidgetItem* recognitionItem = nullptr;
  QTreeWidgetItem* mediaPlayersItem = nullptr;
  QTreeWidgetItem* streamingItem = nullptr;
  QTreeWidgetItem* discordItem = nullptr;
  QTreeWidgetItem* httpItem = nullptr;
  QTreeWidgetItem* ircItem = nullptr;
  {
    recognitionItem = add_item("check_circle", "Recognition");
    mediaPlayersItem = add_child(recognitionItem, "Media players");
    streamingItem = add_child(recognitionItem, "Streaming");
  }
  {
    auto item = add_item("share", "Sharing");
    discordItem = add_child(item, "Discord");
    httpItem = add_child(item, "HTTP");
#ifdef Q_OS_LINUX
    // v1 drives mIRC over DDE. Konversation takes its place here, so the entry is named after the
    // protocol rather than after the client.
    ircItem = add_child(item, "IRC");
#else
    add_child(item, "mIRC")->setDisabled(true);  // placeholder
#endif
  }
  QTreeWidgetItem* torrentsItem = nullptr;
  QTreeWidgetItem* torrentDownloadsItem = nullptr;
  QTreeWidgetItem* torrentFiltersItem = nullptr;
  {
    torrentsItem = add_item("rss_feed", "Torrents");
    torrentDownloadsItem = add_child(torrentsItem, "Downloads");
    torrentFiltersItem = add_child(torrentsItem, "Filters");
  }
  QTreeWidgetItem* advancedItem = nullptr;
  QTreeWidgetItem* cacheItem = nullptr;
  {
    advancedItem = add_item("warning", "Advanced");
    cacheItem = add_child(advancedItem, "Cache");
  }

  ui_->treeWidget->expandAll();

  addPage(accountsItem, new AccountsPage(this));
  addPage(applicationItem, new ApplicationPage(this));
  addPage(animeListItem, new AnimeListPage(this));
  addPage(libraryItem, new LibraryPage(this));
  addPage(recognitionItem, new RecognitionPage(this));
  addPage(mediaPlayersItem, new MediaPlayersPage(this));
  addPage(streamingItem, new StreamingPage(this));
  addPage(discordItem, new DiscordPage(this));
  addPage(httpItem, new HttpPage(this));
#ifdef Q_OS_LINUX
  addPage(ircItem, new IrcPage(this));
#endif
  addPage(torrentsItem, new TorrentsPage(this));
  addPage(torrentDownloadsItem, new TorrentDownloadsPage(this));
  addPage(torrentFiltersItem, new TorrentFiltersPage(this));
  addPage(advancedItem, new AdvancedPage(this));
  addPage(cacheItem, new CachePage(this));

  connect(ui_->treeWidget, &QTreeWidget::currentItemChanged, this,
          [this](QTreeWidgetItem* current, QTreeWidgetItem* previous) {
            // As in v1, a section without a page of its own opens its first page.
            if (current && !current->data(0, Qt::UserRole).isValid()) {
              // Moving up from its first page must not bounce back to it.
              if (previous && previous->parent() == current) {
                if (const auto above = ui_->treeWidget->itemAbove(current)) {
                  ui_->treeWidget->setCurrentItem(above);
                  return;
                }
              }
              for (int i = 0; i < current->childCount(); ++i) {
                const auto child = current->child(i);
                if (!child->isDisabled() && child->data(0, Qt::UserRole).isValid()) {
                  ui_->treeWidget->setCurrentItem(child);
                  return;
                }
              }
            }

            if (current) {
              auto text = current->text(0);
              if (current->parent()) {
                text = u"%1 / %2"_s.arg(current->parent()->text(0), text);
              }
              ui_->titleLabel->setText(text);

              // Pages that are not implemented yet share the placeholder at index 0
              ui_->stackedWidget->setCurrentIndex(current->data(0, Qt::UserRole).toInt());
            }
          });

  items_ = {
      {SettingsPageId::Accounts, accountsItem},
      {SettingsPageId::Library, libraryItem},
      {SettingsPageId::TorrentFilters, torrentFiltersItem},
  };

  setCurrentPage(SettingsPageId::Accounts);
}

void SettingsDialog::setCurrentPage(const SettingsPageId page) {
  const auto it = items_.find(page);
  if (it != items_.end()) ui_->treeWidget->setCurrentItem(it->second);
}

void SettingsDialog::accept() {
  for (const auto page : pages_) {
    page->save();
  }

  QDialog::accept();
}

void SettingsDialog::addPage(QTreeWidgetItem* item, SettingsPage* page) {
  const auto index = ui_->stackedWidget->addWidget(page);
  item->setData(0, Qt::UserRole, index);
  page->load();
  pages_.push_back(page);
}

void SettingsDialog::show(QWidget* parent, const SettingsPageId page) {
  auto dlg = new SettingsDialog(parent);
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->setModal(true);
  dlg->setCurrentPage(page);
  dlg->QDialog::show();
}

}  // namespace gui
