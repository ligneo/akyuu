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

#include "settings_library_page.hpp"

#include <QCheckBox>
#include <QDir>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMimeData>
#include <QPushButton>
#include <QVBoxLayout>

#include "akyuu/settings.hpp"
#include "track/library.hpp"

namespace gui {

LibraryPage::LibraryPage(QWidget* parent)
    : SettingsPage(parent),
      m_listFolders(new QListWidget(this)),
      m_buttonRemove(new QPushButton(tr("Remove"), this)),
      m_checkWatch(new QCheckBox(tr("Watch library folders for changes"), this)),
      m_checkScanOnStartup(new QCheckBox(tr("Scan available episodes on startup"), this)) {
  const auto layout = new QVBoxLayout(this);

  // Library folders
  {
    const auto group = new QGroupBox(tr("Library folders"), this);
    const auto groupLayout = new QVBoxLayout(group);

    groupLayout->addWidget(new QLabel(
        tr("These folders will be scanned for available episodes. You can drag and drop folders "
           "here."),
        group));

    m_listFolders->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_listFolders->setAcceptDrops(true);
    m_listFolders->installEventFilter(this);
    groupLayout->addWidget(m_listFolders);

    const auto buttonLayout = new QHBoxLayout();
    const auto buttonAdd = new QPushButton(tr("Add new..."), group);
    buttonLayout->addWidget(buttonAdd);
    buttonLayout->addWidget(m_buttonRemove);
    buttonLayout->addStretch();
    groupLayout->addLayout(buttonLayout);

    connect(buttonAdd, &QPushButton::clicked, this, &LibraryPage::addFolder);
    connect(m_buttonRemove, &QPushButton::clicked, this, &LibraryPage::removeFolder);
    connect(m_listFolders, &QListWidget::itemSelectionChanged, this,
            [this]() { m_buttonRemove->setEnabled(!m_listFolders->selectedItems().isEmpty()); });

    layout->addWidget(group);
  }

  // Behavior
  {
    const auto group = new QGroupBox(tr("Behavior"), this);
    const auto groupLayout = new QVBoxLayout(group);

    groupLayout->addWidget(m_checkScanOnStartup);
    groupLayout->addWidget(m_checkWatch);

    const auto note = new QLabel(
        tr("Taiga is told which folder changed, not which file, so it scans the library again "
           "after a change."),
        group);
    note->setWordWrap(true);
    groupLayout->addWidget(note);

    layout->addWidget(group);
  }

  layout->addStretch();
}

// Qt's list widget does not take folders on its own, so the drops are handled here. v1 offers the
// same shortcut ("Tip: You can drag and drop folders here").
bool LibraryPage::eventFilter(QObject* watched, QEvent* event) {
  if (watched != m_listFolders) return SettingsPage::eventFilter(watched, event);

  switch (event->type()) {
    case QEvent::DragEnter:
    case QEvent::DragMove: {
      const auto dragEvent = static_cast<QDragMoveEvent*>(event);
      if (dragEvent->mimeData()->hasUrls()) dragEvent->acceptProposedAction();
      return true;
    }

    case QEvent::Drop: {
      const auto dropEvent = static_cast<QDropEvent*>(event);
      for (const auto& url : dropEvent->mimeData()->urls()) {
        if (!url.isLocalFile()) continue;
        const auto path = QDir::toNativeSeparators(QDir::cleanPath(url.toLocalFile()));
        if (!QFileInfo{path}.isDir()) continue;
        if (!m_listFolders->findItems(path, Qt::MatchExactly).isEmpty()) continue;
        m_listFolders->addItem(path);
      }
      dropEvent->acceptProposedAction();
      return true;
    }

    default:
      return SettingsPage::eventFilter(watched, event);
  }
}

void LibraryPage::load() {
  m_listFolders->clear();
  for (const auto& folder : akyuu::settings.libraryFolders()) {
    m_listFolders->addItem(QString::fromStdString(folder));
  }
  m_buttonRemove->setEnabled(false);
  m_checkWatch->setChecked(akyuu::settings.libraryWatchFolders());
  m_checkScanOnStartup->setChecked(akyuu::settings.libraryScanOnStartup());
}

void LibraryPage::save() {
  std::vector<std::string> folders;
  for (int i = 0; i < m_listFolders->count(); ++i) {
    folders.push_back(m_listFolders->item(i)->text().toStdString());
  }
  akyuu::settings.setLibraryFolders(folders);
  akyuu::settings.setLibraryWatchFolders(m_checkWatch->isChecked());
  akyuu::settings.setLibraryScanOnStartup(m_checkScanOnStartup->isChecked());

  track::library()->applyWatchSettings();
}

void LibraryPage::addFolder() {
  constexpr auto options =
      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks | QFileDialog::ReadOnly;

  const auto directory =
      QFileDialog::getExistingDirectory(this, tr("Add New Folder"), QDir::homePath(), options);
  if (directory.isEmpty()) return;

  const auto path = QDir::toNativeSeparators(QDir::cleanPath(directory));
  if (!m_listFolders->findItems(path, Qt::MatchExactly).isEmpty()) return;

  m_listFolders->addItem(path);
}

void LibraryPage::removeFolder() {
  qDeleteAll(m_listFolders->selectedItems());
}

}  // namespace gui
