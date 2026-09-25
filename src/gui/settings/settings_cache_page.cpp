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

#include "settings_cache_page.hpp"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLocale>
#include <QPalette>
#include <QPushButton>
#include <QVBoxLayout>

#include "base/string.hpp"
#include "gui/utils/image_provider.hpp"
#include "gui/utils/widgets.hpp"
#include "media/anime_history.hpp"
#include "akyuu/path.hpp"
#include "akyuu/settings.hpp"
#include "track/feed_archive.hpp"

namespace gui {

namespace {

QString formatItemCount(const int count) {
  return QCoreApplication::translate("gui::CachePage", "%n item(s)", nullptr, count);
}

void dim(QLabel* label) {
  auto palette = label->palette();
  palette.setColor(QPalette::WindowText, palette.color(QPalette::Disabled, QPalette::WindowText));
  label->setPalette(palette);
}

}  // namespace

CachePage::CachePage(QWidget* parent)
    : SettingsPage(parent),
      m_checkHistory(new QCheckBox(tr("History"), this)),
      m_labelHistory(new QLabel(this)),
      m_checkImages(new QCheckBox(tr("Image files"), this)),
      m_labelImages(new QLabel(this)),
      m_buttonClear(new QPushButton(tr("Clear"), this)) {
  const auto layout = new QVBoxLayout(this);

  // Clear data
  {
    const auto group = new QGroupBox(tr("Clear data"), this);
    const auto grid = new QGridLayout(group);

    dim(m_labelHistory);
    dim(m_labelImages);

    grid->addWidget(m_checkHistory, 0, 0);
    grid->addWidget(m_labelHistory, 0, 1);
    grid->addWidget(m_checkImages, 1, 0);
    grid->addWidget(m_labelImages, 1, 1);
    grid->addWidget(m_buttonClear, 2, 2, Qt::AlignRight);
    grid->setColumnStretch(1, 1);

    layout->addWidget(group);
  }

  layout->addStretch();

  const auto updateButton = [this]() {
    m_buttonClear->setEnabled(m_checkHistory->isChecked() || m_checkImages->isChecked());
  };
  connect(m_checkHistory, &QCheckBox::toggled, this, updateButton);
  connect(m_checkImages, &QCheckBox::toggled, this, updateButton);
  connect(m_buttonClear, &QPushButton::clicked, this, &CachePage::clear);

  m_buttonClear->setEnabled(false);
}

void CachePage::load() {
  refresh();
}

void CachePage::save() {}

void CachePage::showEvent(QShowEvent* event) {
  SettingsPage::showEvent(event);
  refresh();
}

void CachePage::clear() {
  if (!confirm(this, tr("Do you want to clear the selected data?"),
               tr("All selected items will be permanently removed."), tr("Clear"))) {
    return;
  }

  if (m_checkHistory->isChecked()) {
    anime::history.clear();
    m_checkHistory->setChecked(false);
  }
  if (m_checkImages->isChecked()) {
    imageProvider.clearCache();
    m_checkImages->setChecked(false);
  }

  refresh();
}

void CachePage::refresh() {
  m_labelHistory->setText(formatItemCount(static_cast<int>(anime::history.items().size())));

  const QDir dir{imageProvider.cachePath()};
  const auto files = dir.entryInfoList(QDir::Files);
  qint64 size = 0;
  for (const auto& file : files) {
    size += file.size();
  }
  m_labelImages->setText(u"%1, %2"_s.arg(formatItemCount(static_cast<int>(files.size())),
                                         QLocale::system().formattedDataSize(size, 1)));
}

}  // namespace gui
