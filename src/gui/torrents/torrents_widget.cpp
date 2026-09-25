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

#include "torrents_widget.hpp"

#include <QClipboard>
#include <QCursor>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QHeaderView>
#include <QLayout>
#include <QLineEdit>
#include <QMenu>
#include <QUrl>
#include <map>

#include "base/log.hpp"
#include "base/string.hpp"
#include "gui/main/main_window.hpp"
#include "gui/main/status_bar_controller.hpp"
#include "gui/models/torrent_model.hpp"
#include "gui/utils/theme.hpp"
#include "gui/utils/widgets.hpp"
#include "media/anime.hpp"
#include "track/feed.hpp"
#include "track/feed_aggregator.hpp"
#include "track/feed_archive.hpp"
#include "track/feed_filter_manager.hpp"

namespace gui {

TorrentsWidget::TorrentsWidget(QWidget* parent)
    : PageWidget{parent},
      m_model(new TorrentModel(this)),
      m_proxyModel(new QSortFilterProxyModel(this)),
      m_view(new QTreeView(this)) {
  m_proxyModel->setSourceModel(m_model);
  m_proxyModel->setSortRole(static_cast<int>(TorrentItemDataRole::SortValue));
  m_proxyModel->setFilterKeyColumn(TorrentModel::COLUMN_TITLE);
  m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
  // Category rows are kept as long as one of their items matches
  m_proxyModel->setRecursiveFilteringEnabled(true);

  m_view->setObjectName("torrentsView");
  m_view->setFrameShape(QFrame::Shape::NoFrame);
  m_view->setModel(m_proxyModel);
  m_view->setAlternatingRowColors(true);
  m_view->setAllColumnsShowFocus(true);
  m_view->setContextMenuPolicy(Qt::CustomContextMenu);
  m_view->setSortingEnabled(true);
  m_view->setUniformRowHeights(true);
  m_view->sortByColumn(TorrentModel::COLUMN_DATE, Qt::SortOrder::DescendingOrder);

  m_view->header()->setSectionsMovable(false);
  m_view->header()->setStretchLastSection(true);
  m_view->header()->setTextElideMode(Qt::ElideRight);
  m_view->header()->setSectionResizeMode(QHeaderView::Interactive);

  // Default widths, as in v1. Stretching a column instead would let the description squeeze the
  // title out of view.
  static const std::map<int, int> widths{
      {TorrentModel::COLUMN_TITLE, 260},       {TorrentModel::COLUMN_EPISODE, 80},
      {TorrentModel::COLUMN_GROUP, 100},       {TorrentModel::COLUMN_SIZE, 95},
      {TorrentModel::COLUMN_VIDEO, 80},        {TorrentModel::COLUMN_SEEDERS, 40},
      {TorrentModel::COLUMN_LEECHERS, 40},     {TorrentModel::COLUMN_DOWNLOADS, 50},
      {TorrentModel::COLUMN_DESCRIPTION, 200}, {TorrentModel::COLUMN_FILENAME, 200},
      {TorrentModel::COLUMN_DATE, 140},
  };
  for (const auto& [column, width] : widths) {
    m_view->header()->resizeSection(column, width);
  }

  layout()->addWidget(m_view);

  connect(m_model, &QAbstractItemModel::modelReset, m_view, &QTreeView::expandAll);

  initToolbar();

  connect(m_view, &QWidget::customContextMenuRequested, this, &TorrentsWidget::showContextMenu);
  connect(m_view, &QTreeView::doubleClicked, this, &TorrentsWidget::openItemLink);

  connect(mainWindow()->searchBox(), &QLineEdit::textChanged, this, &TorrentsWidget::setFilterText);
  setFilterText(mainWindow()->searchBox()->text());

  connect(track::aggregator(), &track::Aggregator::fetchingChanged, this,
          [this](bool fetching) { m_actionRefresh->setEnabled(!fetching); });
  // A download can start on its own during an automatic check, so neither outcome may pass
  // unseen.
  const auto showMessage = [](const QString& text) {
    mainWindow()->statusBarController()->showMessage({
        .source = StatusBarController::Source::Torrents,
        .text = text,
        .spin = false,
    });
  };
  connect(track::aggregator(), &track::Aggregator::errorOccurred, this,
          [showMessage](const QString& message) {
            qCritical() << message;
            showMessage(message);
          });
  connect(track::aggregator(), &track::Aggregator::downloadFinished, this,
          [showMessage](const QString& title) {
            showMessage(tr("Sent \"%1\" to the BitTorrent client.").arg(title));
          });

  if (track::aggregator()->feed().items.empty()) track::aggregator()->fetch();
}

void TorrentsWidget::initToolbar() {
  m_actionRefresh = new QAction(theme.getIcon("sync"), tr("Refresh"), this);
  connect(m_actionRefresh, &QAction::triggered, this, []() { track::aggregator()->fetch(); });
  m_toolbar->addAction(m_actionRefresh);

  // v1 puts these next to the check button: act on everything that is marked.
  m_toolbar->addSeparator();

  const auto actionDownload =
      new QAction(theme.getIcon("cloud_download"), tr("Download marked torrents"), this);
  connect(actionDownload, &QAction::triggered, this,
          []() { track::aggregator()->downloadSelected(); });
  m_toolbar->addAction(actionDownload);

  const auto actionDiscard = new QAction(theme.getIcon("delete"), tr("Discard marked"), this);
  connect(actionDiscard, &QAction::triggered, this, [this]() {
    if (!confirm(this, tr("Are you sure you want to discard the marked torrents?"),
                 tr("They will not be offered again."), tr("Discard"))) {
      return;
    }
    track::aggregator()->discardSelected();
  });
  m_toolbar->addAction(actionDiscard);
}

void TorrentsWidget::setFilterText(const QString& text) {
  if (!isVisible()) return;  // the text belongs to another page
  m_proxyModel->setFilterFixedString(text);
}

void TorrentsWidget::search(const QString& title) {
  track::aggregator()->search(title);
}

void TorrentsWidget::openItemLink(const QModelIndex& index) const {
  const auto item = m_model->itemAt(m_proxyModel->mapToSource(index));

  if (!item) return;

  // Opening the item in a browser is the only thing that can be done until downloads are handled.
  const auto link = !item->info_link.empty() ? item->info_link : item->link;

  if (!link.empty()) QDesktopServices::openUrl(QUrl{QString::fromStdString(link)});
}

void TorrentsWidget::showContextMenu() {
  const auto index = m_view->currentIndex();

  if (!index.isValid()) return;

  const auto item = m_model->itemAt(m_proxyModel->mapToSource(index));

  if (!item) return;

  auto* menu = new QMenu(m_view);
  menu->setAttribute(Qt::WA_DeleteOnClose);

  // v1's first menu entry, and the reason the page exists.
  menu->addAction(theme.getIcon("cloud_download"), tr("Download"), this,
                  [item]() { track::aggregator()->download(*item); });

  menu->addAction(tr("Open in browser"), this, [this, index]() { openItemLink(index); });

  if (const auto title = item->episode.element(anitomy::ElementKind::Title); !title.empty()) {
    const auto text = QString::fromStdString(title);
    menu->addAction(tr("Search for more torrents"), this, [this, text]() { search(text); });
  }

  if (!item->magnet_link.empty()) {
    const auto link = QString::fromStdString(item->magnet_link);
    menu->addAction(tr("Copy magnet link"), this,
                    [link]() { QGuiApplication::clipboard()->setText(link); });
  }

  menu->addSeparator();

  const auto sourceIndex = m_proxyModel->mapToSource(index);

  menu->addAction(tr("Discard"), this, [this, sourceIndex, item]() {
    m_model->discardItem(sourceIndex);
    track::archive.add(QString::fromStdString(item->title));
  });

  const auto animeId = item->episode.animeId();

  if (animeId != anime::kUnknownId) {
    menu->addAction(tr("Discard all for this anime"), this, [this, animeId]() {
      m_model->discardItems(animeId);
      track::filterManager.addDiscardFilter(animeId);
    });

    const auto group = item->episode.element(anitomy::ElementKind::ReleaseGroup);

    if (!group.empty()) {
      const auto resolution = item->episode.element(anitomy::ElementKind::VideoResolution);
      menu->addAction(tr("Select fansub group"), this, [this, animeId, group, resolution]() {
        m_model->discardOtherFansubs(animeId, group, resolution);
        track::filterManager.setFansubFilter(animeId, group, resolution);
      });
    }
  }

  menu->popup(QCursor::pos());
}

}  // namespace gui
