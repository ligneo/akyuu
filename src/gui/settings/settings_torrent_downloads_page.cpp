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

#include "settings_torrent_downloads_page.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include "akyuu/settings.hpp"

namespace gui {

namespace {

using namespace Qt::StringLiterals;

// A folder can only be handed to a client that takes one on its command line. This is v1's list,
// minus the two that do not exist on Linux.
constexpr auto kSupportedClients =
    "aria2, Deluge (deluge-console), qBittorrent and Transmission (transmission-remote)";

}  // namespace

TorrentDownloadsPage::TorrentDownloadsPage(QWidget* parent)
    : SettingsPage(parent),
      m_comboSortBy(new QComboBox(this)),
      m_comboSortOrder(new QComboBox(this)),
      m_checkUseAnimeFolder(new QCheckBox(tr("Use anime folders as the download folder"), this)),
      m_editLocation(new QLineEdit(this)),
      m_checkCreateSubfolder(
          new QCheckBox(tr("Create a subfolder using the anime title as its name"), this)),
      m_checkOpen(new QCheckBox(tr("Open downloaded .torrent files"), this)),
      m_radioDefaultApp(
          new QRadioButton(tr("Use the default application associated with .torrent files"), this)),
      m_radioCustomApp(new QRadioButton(tr("Use a custom application:"), this)),
      m_editAppPath(new QLineEdit(this)) {
  const auto layout = new QVBoxLayout(this);

  // Download queue
  {
    const auto group = new QGroupBox(tr("Download queue"), this);
    const auto rowLayout = new QHBoxLayout(group);

    m_comboSortBy->addItem(tr("Sort by episode number"), u"episodeNumber"_s);
    m_comboSortBy->addItem(tr("Sort by release date"), u"releaseDate"_s);
    m_comboSortOrder->addItem(tr("In ascending order"),
                              static_cast<int>(Qt::SortOrder::AscendingOrder));
    m_comboSortOrder->addItem(tr("In descending order"),
                              static_cast<int>(Qt::SortOrder::DescendingOrder));

    rowLayout->addWidget(m_comboSortBy, 1);
    rowLayout->addWidget(m_comboSortOrder, 1);

    layout->addWidget(group);
  }

  // Download location
  {
    const auto group = new QGroupBox(tr("Download location"), this);
    const auto groupLayout = new QVBoxLayout(group);

    const auto pathLayout = new QHBoxLayout();
    m_buttonBrowseLocation = new QPushButton(tr("Browse..."), group);
    m_editLocation->setPlaceholderText(tr("Leave empty to let the client decide"));
    pathLayout->addWidget(m_editLocation);
    pathLayout->addWidget(m_buttonBrowseLocation);

    m_checkFallback =
        new QCheckBox(tr("If no anime folder is set, use this folder instead:"), group);

    // Each option only matters with the one above it, so they step in as in v1.
    const auto nested = new QVBoxLayout();
    nested->setContentsMargins(20, 0, 0, 0);
    nested->addWidget(m_checkFallback);
    const auto pathNested = new QVBoxLayout();
    pathNested->setContentsMargins(20, 0, 0, 0);
    pathNested->addLayout(pathLayout);
    pathNested->addWidget(m_checkCreateSubfolder);
    nested->addLayout(pathNested);

    groupLayout->addWidget(m_checkUseAnimeFolder);
    groupLayout->addLayout(nested);

    const auto note = new QLabel(
        tr("Note: This feature is only supported by %1.").arg(QString::fromUtf8(kSupportedClients)),
        group);
    note->setWordWrap(true);
    note->setForegroundRole(QPalette::PlaceholderText);
    note->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    groupLayout->addWidget(note);

    connect(m_buttonBrowseLocation, &QPushButton::clicked, this, [this]() {
      const auto directory = QFileDialog::getExistingDirectory(
          this, tr("Download Location"),
          m_editLocation->text().isEmpty() ? QDir::homePath() : m_editLocation->text(),
          QFileDialog::ShowDirsOnly);
      if (!directory.isEmpty()) m_editLocation->setText(QDir::toNativeSeparators(directory));
    });

    layout->addWidget(group);
  }

  // BitTorrent client
  {
    const auto group = new QGroupBox(tr("BitTorrent client"), this);
    const auto groupLayout = new QVBoxLayout(group);

    const auto pathLayout = new QHBoxLayout();
    const auto buttonBrowse = new QPushButton(tr("Browse..."), group);
    m_editAppPath->setPlaceholderText(tr("Path to the application"));
    pathLayout->addWidget(m_editAppPath);
    pathLayout->addWidget(buttonBrowse);

    const auto nested = new QVBoxLayout();
    nested->setContentsMargins(20, 0, 0, 0);
    nested->addWidget(m_radioDefaultApp);
    nested->addWidget(m_radioCustomApp);
    const auto pathNested = new QVBoxLayout();
    pathNested->setContentsMargins(20, 0, 0, 0);
    pathNested->addLayout(pathLayout);
    nested->addLayout(pathNested);

    groupLayout->addWidget(m_checkOpen);
    groupLayout->addLayout(nested);

    connect(buttonBrowse, &QPushButton::clicked, this, [this]() {
      const auto path = QFileDialog::getOpenFileName(
          this, tr("BitTorrent Client"),
          m_editAppPath->text().isEmpty() ? u"/usr/bin"_s : m_editAppPath->text());
      if (!path.isEmpty()) m_editAppPath->setText(path);
    });

    layout->addWidget(group);
  }

  layout->addStretch();

  connect(m_checkUseAnimeFolder, &QCheckBox::toggled, this, &TorrentDownloadsPage::refreshState);
  connect(m_editLocation, &QLineEdit::textChanged, this, &TorrentDownloadsPage::refreshState);
  connect(m_checkFallback, &QCheckBox::toggled, this, &TorrentDownloadsPage::refreshState);
  connect(m_checkOpen, &QCheckBox::toggled, this, &TorrentDownloadsPage::refreshState);
  connect(m_radioCustomApp, &QRadioButton::toggled, this, &TorrentDownloadsPage::refreshState);
}

void TorrentDownloadsPage::load() {
  const auto sortBy = QString::fromStdString(akyuu::settings.torrentDownloadSortBy());
  m_comboSortBy->setCurrentIndex(std::max(m_comboSortBy->findData(sortBy), 0));
  m_comboSortOrder->setCurrentIndex(std::max(
      m_comboSortOrder->findData(static_cast<int>(akyuu::settings.torrentDownloadSortOrder())), 0));

  m_checkUseAnimeFolder->setChecked(akyuu::settings.torrentDownloadUseAnimeFolder());
  m_checkFallback->setChecked(akyuu::settings.torrentDownloadFallbackOnFolder());
  m_editLocation->setText(QString::fromStdString(akyuu::settings.torrentDownloadLocation()));
  m_checkCreateSubfolder->setChecked(akyuu::settings.torrentDownloadCreateSubfolder());

  m_checkOpen->setChecked(akyuu::settings.torrentDownloadOpen());

  const bool custom = akyuu::settings.torrentDownloadAppMode() != "default";
  m_radioCustomApp->setChecked(custom);
  m_radioDefaultApp->setChecked(!custom);
  m_editAppPath->setText(QString::fromStdString(akyuu::settings.torrentDownloadAppPath()));

  refreshState();
}

void TorrentDownloadsPage::save() {
  akyuu::settings.setTorrentDownloadSortBy(m_comboSortBy->currentData().toString().toStdString());
  akyuu::settings.setTorrentDownloadSortOrder(
      static_cast<Qt::SortOrder>(m_comboSortOrder->currentData().toInt()));
  akyuu::settings.setTorrentDownloadUseAnimeFolder(m_checkUseAnimeFolder->isChecked());
  akyuu::settings.setTorrentDownloadFallbackOnFolder(m_checkFallback->isChecked());
  akyuu::settings.setTorrentDownloadLocation(m_editLocation->text().trimmed().toStdString());
  akyuu::settings.setTorrentDownloadCreateSubfolder(m_checkCreateSubfolder->isChecked());
  akyuu::settings.setTorrentDownloadOpen(m_checkOpen->isChecked());
  akyuu::settings.setTorrentDownloadAppMode(m_radioCustomApp->isChecked() ? "custom" : "default");
  akyuu::settings.setTorrentDownloadAppPath(m_editAppPath->text().trimmed().toStdString());
}

void TorrentDownloadsPage::refreshState() {
  // As in v1, a folder is only handed to the client with the first option on.
  const bool useFolder = m_checkUseAnimeFolder->isChecked();
  const bool fallback = useFolder && m_checkFallback->isChecked();
  m_checkFallback->setEnabled(useFolder);
  m_editLocation->setEnabled(fallback);
  m_buttonBrowseLocation->setEnabled(fallback);
  m_checkCreateSubfolder->setEnabled(fallback && !m_editLocation->text().trimmed().isEmpty());

  const bool open = m_checkOpen->isChecked();

  m_radioDefaultApp->setEnabled(open);
  m_radioCustomApp->setEnabled(open);
  m_editAppPath->setEnabled(open && m_radioCustomApp->isChecked());
}

}  // namespace gui
