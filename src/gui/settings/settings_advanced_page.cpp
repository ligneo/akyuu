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

#include "settings_advanced_page.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkProxy>
#include <QSpinBox>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "base/string.hpp"
#include "akyuu/network.hpp"
#include "akyuu/settings.hpp"

namespace gui {

AdvancedPage::AdvancedPage(QWidget* parent)
    : SettingsPage(parent),
      m_comboProxyType(new QComboBox(this)),
      m_editProxyHost(new QLineEdit(this)),
      m_spinProxyPort(new QSpinBox(this)),
      m_editProxyUsername(new QLineEdit(this)),
      m_editProxyPassword(new QLineEdit(this)),
      m_treeSettings(new QTreeWidget(this)) {
  const auto layout = new QVBoxLayout(this);

  // Proxy
  {
    const auto group = new QGroupBox(tr("Proxy"), this);
    const auto form = new QFormLayout(group);

    m_comboProxyType->addItem(tr("HTTP"), static_cast<int>(QNetworkProxy::HttpProxy));
    m_comboProxyType->addItem(tr("SOCKS5"), static_cast<int>(QNetworkProxy::Socks5Proxy));
    form->addRow(tr("Type:"), m_comboProxyType);

    form->addRow(tr("Host:"), m_editProxyHost);

    m_spinProxyPort->setRange(0, 65535);
    m_spinProxyPort->setSpecialValueText(tr("Default"));
    form->addRow(tr("Port:"), m_spinProxyPort);

    form->addRow(tr("Username:"), m_editProxyUsername);

    m_editProxyPassword->setEchoMode(QLineEdit::Password);
    form->addRow(tr("Password:"), m_editProxyPassword);

    layout->addWidget(group);
  }

  // Settings
  {
    const auto group = new QGroupBox(tr("Settings"), this);
    const auto groupLayout = new QVBoxLayout(group);

    const auto warning = new QLabel(
        tr("Warning: Do not change these settings unless you are sure of what you are doing."),
        group);
    warning->setWordWrap(true);
    groupLayout->addWidget(warning);

    m_treeSettings->setRootIsDecorated(false);
    m_treeSettings->setAllColumnsShowFocus(true);
    m_treeSettings->setAlternatingRowColors(true);
    m_treeSettings->setColumnCount(2);
    m_treeSettings->setHeaderLabels({tr("Name"), tr("Value")});
    m_treeSettings->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    groupLayout->addWidget(m_treeSettings);

    layout->addWidget(group);
  }
}

// v1's Advanced tab is a raw Name/Value table rather than a themed page: it is where settings
// that work but have no box of their own live. These three have no control in v1 either.
void AdvancedPage::initSettingsTable() {
  m_treeSettings->clear();

  const auto addRow = [this](const QString& name, const QVariant& value) {
    const auto item = new QTreeWidgetItem(m_treeSettings);
    item->setText(0, name);
    item->setData(1, Qt::DisplayRole, value);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    return item;
  };

  addRow(tr("Application / Episode notification format"),
         QString::fromStdString(taiga::settings.syncNotifyFormat()));
  addRow(tr("Torrents / Archive limit"), taiga::settings.torrentArchiveMaxCount());
  addRow(tr("Torrents / Download path for .torrent files"),
         QString::fromStdString(taiga::settings.torrentDownloadFileLocation()));
  addRow(tr("Torrents / Use magnet links if available"),
         taiga::settings.torrentDownloadUseMagnet());

  // These have no control in v1 either, and the same is true here.
  addRow(tr("Library / File size threshold"),
         QString::number(taiga::settings.libraryMinimumFileSize()));
  addRow(tr("Recognition / Ignored strings"),
         joinStrings(taiga::settings.recognitionIgnoredStrings(), {}));
  addRow(tr("Recognition / Look up parent directories"),
         taiga::settings.recognitionLookupParentDirectories());
}

void AdvancedPage::load() {
  const auto type = static_cast<int>(taiga::settings.proxyType());
  m_comboProxyType->setCurrentIndex(m_comboProxyType->findData(type));

  m_editProxyHost->setText(QString::fromStdString(taiga::settings.proxyHost()));
  m_spinProxyPort->setValue(std::max(taiga::settings.proxyPort(), 0));
  m_editProxyUsername->setText(QString::fromStdString(taiga::settings.proxyUsername()));
  m_editProxyPassword->setText(QString::fromStdString(taiga::settings.proxyPassword()));

  initSettingsTable();
}

void AdvancedPage::save() {
  if (m_treeSettings->topLevelItemCount() == 7) {
    taiga::settings.setSyncNotifyFormat(
        m_treeSettings->topLevelItem(0)->data(1, Qt::DisplayRole).toString().toStdString());

    taiga::settings.setTorrentArchiveMaxCount(
        m_treeSettings->topLevelItem(1)->data(1, Qt::DisplayRole).toInt());
    taiga::settings.setTorrentDownloadFileLocation(
        m_treeSettings->topLevelItem(2)->data(1, Qt::DisplayRole).toString().toStdString());
    taiga::settings.setTorrentDownloadUseMagnet(
        m_treeSettings->topLevelItem(3)->data(1, Qt::DisplayRole).toBool());

    taiga::settings.setLibraryMinimumFileSize(
        m_treeSettings->topLevelItem(4)->data(1, Qt::DisplayRole).toLongLong());

    const auto ignored = m_treeSettings->topLevelItem(5)->data(1, Qt::DisplayRole).toString();
    taiga::settings.setRecognitionIgnoredStrings(
        toVector(ignored.split(u", "_s, Qt::SkipEmptyParts)));

    taiga::settings.setRecognitionLookupParentDirectories(
        m_treeSettings->topLevelItem(6)->data(1, Qt::DisplayRole).toBool());
  }

  taiga::settings.setProxyType(
      static_cast<QNetworkProxy::ProxyType>(m_comboProxyType->currentData().toInt()));
  taiga::settings.setProxyHost(m_editProxyHost->text().trimmed().toStdString());

  const auto port = m_spinProxyPort->value();
  taiga::settings.setProxyPort(port > 0 ? port : -1);

  taiga::settings.setProxyUsername(m_editProxyUsername->text().toStdString());
  taiga::settings.setProxyPassword(m_editProxyPassword->text().toStdString());

  taiga::network()->applyProxySettings();
}

}  // namespace gui
