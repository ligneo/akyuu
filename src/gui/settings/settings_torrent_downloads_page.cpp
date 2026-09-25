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

#include "settings_torrent_downloads_page.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
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
      m_checkUseAnimeFolder(
          new QCheckBox(tr("Use the anime's folder as the download location"), this)),
      m_editLocation(new QLineEdit(this)),
      m_checkCreateSubfolder(new QCheckBox(tr("Create a subfolder named after the anime"), this)),
      m_checkOpen(new QCheckBox(tr("Open downloaded .torrent files"), this)),
      m_radioDefaultApp(
          new QRadioButton(tr("Use the default application associated with .torrent files"), this)),
      m_radioCustomApp(new QRadioButton(tr("Use a custom application:"), this)),
      m_editAppPath(new QLineEdit(this)) {
  const auto layout = new QVBoxLayout(this);

  // Download queue
  {
    const auto group = new QGroupBox(tr("Download queue"), this);
    const auto form = new QFormLayout(group);

    m_comboSortBy->addItem(tr("Episode number"), u"episodeNumber"_s);
    m_comboSortBy->addItem(tr("Release date"), u"releaseDate"_s);
    m_comboSortOrder->addItem(tr("In ascending order"),
                              static_cast<int>(Qt::SortOrder::AscendingOrder));
    m_comboSortOrder->addItem(tr("In descending order"),
                              static_cast<int>(Qt::SortOrder::DescendingOrder));

    form->addRow(tr("Sort by:"), m_comboSortBy);
    form->addRow(tr("Order:"), m_comboSortOrder);

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

    m_labelLocation = new QLabel(tr("If the anime has no folder, save downloads to:"), group);

    groupLayout->addWidget(m_checkUseAnimeFolder);
    groupLayout->addWidget(m_labelLocation);
    groupLayout->addLayout(pathLayout);
    groupLayout->addWidget(m_checkCreateSubfolder);

    const auto note = new QLabel(
        tr("Note: This is only supported by %1.").arg(QString::fromUtf8(kSupportedClients)), group);
    note->setWordWrap(true);
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

    groupLayout->addWidget(m_checkOpen);
    groupLayout->addWidget(m_radioDefaultApp);
    groupLayout->addWidget(m_radioCustomApp);
    groupLayout->addLayout(pathLayout);

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
  connect(m_checkOpen, &QCheckBox::toggled, this, &TorrentDownloadsPage::refreshState);
  connect(m_radioCustomApp, &QRadioButton::toggled, this, &TorrentDownloadsPage::refreshState);
}

void TorrentDownloadsPage::load() {
  const auto sortBy = QString::fromStdString(taiga::settings.torrentDownloadSortBy());
  m_comboSortBy->setCurrentIndex(std::max(m_comboSortBy->findData(sortBy), 0));
  m_comboSortOrder->setCurrentIndex(std::max(
      m_comboSortOrder->findData(static_cast<int>(taiga::settings.torrentDownloadSortOrder())), 0));

  m_checkUseAnimeFolder->setChecked(taiga::settings.torrentDownloadUseAnimeFolder());
  m_editLocation->setText(QString::fromStdString(taiga::settings.torrentDownloadLocation()));
  m_checkCreateSubfolder->setChecked(taiga::settings.torrentDownloadCreateSubfolder());

  m_checkOpen->setChecked(taiga::settings.torrentDownloadOpen());

  const bool custom = taiga::settings.torrentDownloadAppMode() != "default";
  m_radioCustomApp->setChecked(custom);
  m_radioDefaultApp->setChecked(!custom);
  m_editAppPath->setText(QString::fromStdString(taiga::settings.torrentDownloadAppPath()));

  refreshState();
}

void TorrentDownloadsPage::save() {
  taiga::settings.setTorrentDownloadSortBy(m_comboSortBy->currentData().toString().toStdString());
  taiga::settings.setTorrentDownloadSortOrder(
      static_cast<Qt::SortOrder>(m_comboSortOrder->currentData().toInt()));
  taiga::settings.setTorrentDownloadUseAnimeFolder(m_checkUseAnimeFolder->isChecked());
  taiga::settings.setTorrentDownloadLocation(m_editLocation->text().trimmed().toStdString());
  taiga::settings.setTorrentDownloadCreateSubfolder(m_checkCreateSubfolder->isChecked());
  taiga::settings.setTorrentDownloadOpen(m_checkOpen->isChecked());
  taiga::settings.setTorrentDownloadAppMode(m_radioCustomApp->isChecked() ? "custom" : "default");
  taiga::settings.setTorrentDownloadAppPath(m_editAppPath->text().trimmed().toStdString());
}

void TorrentDownloadsPage::refreshState() {
  // As in v1, a folder is only handed to the client with the first option on.
  const bool useFolder = m_checkUseAnimeFolder->isChecked();
  m_labelLocation->setEnabled(useFolder);
  m_editLocation->setEnabled(useFolder);
  m_buttonBrowseLocation->setEnabled(useFolder);
  m_checkCreateSubfolder->setEnabled(useFolder && !m_editLocation->text().trimmed().isEmpty());

  const bool open = m_checkOpen->isChecked();

  m_radioDefaultApp->setEnabled(open);
  m_radioCustomApp->setEnabled(open);
  m_editAppPath->setEnabled(open && m_radioCustomApp->isChecked());
}

}  // namespace gui
