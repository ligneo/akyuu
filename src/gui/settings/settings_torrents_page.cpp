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

#include "settings_torrents_page.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "akyuu/settings.hpp"
#include "base/string.hpp"
#include "track/feed_aggregator.hpp"

namespace gui {

namespace {

// The addresses v1 offers, so that a working feed is a click away. AniDex and NyaaPantsu are left
// out: both answered with a server error on 2026-09-25.
const QStringList kSourceUrls{
    u"https://nyaa.si/?page=rss&c=1_2&f=0"_s,
    u"http://tracker.minglong.org/rss.xml"_s,
    u"https://www.shanaproject.com/feeds/site/"_s,
    u"https://subsplease.org/rss/?t&r=1080"_s,
    u"https://www.tokyotosho.info/rss.php?filter=1,11&zwnj=0"_s,
};

const QStringList kSearchUrls{
    u"https://nyaa.si/?page=rss&c=1_2&f=0&q=%title%"_s,
};

}  // namespace

TorrentsPage::TorrentsPage(QWidget* parent)
    : SettingsPage(parent),
      m_comboSource(new QComboBox(this)),
      m_comboSearch(new QComboBox(this)),
      m_checkAutoCheck(new QCheckBox(tr("Check new torrents automatically"), this)),
      m_spinInterval(new QSpinBox(this)),
      m_radioNotify(new QRadioButton(tr("Notify me"), this)),
      m_radioDownload(new QRadioButton(tr("Download immediately"), this)) {
  const auto layout = new QVBoxLayout(this);

  // Sources
  {
    const auto group = new QGroupBox(tr("Sources"), this);
    const auto groupLayout = new QVBoxLayout(group);

    m_comboSource->setEditable(true);
    m_comboSource->addItems(kSourceUrls);
    groupLayout->addWidget(new QLabel(tr("RSS feed for checking new releases:"), group));
    groupLayout->addWidget(m_comboSource);

    m_comboSearch->setEditable(true);
    m_comboSearch->addItems(kSearchUrls);
    groupLayout->addWidget(new QLabel(tr("RSS feed for searching releases for a title:"), group));
    groupLayout->addWidget(m_comboSearch);

    const auto note = new QLabel(tr("Note: %title% is replaced with the anime title."), group);
    note->setWordWrap(true);
    groupLayout->addWidget(note);

    layout->addWidget(group);
  }

  // Automation
  {
    const auto group = new QGroupBox(tr("Automation"), this);
    const auto groupLayout = new QVBoxLayout(group);

    // Everything below depends on the check box, so it sits indented under it, as in v1.
    const auto nested = new QVBoxLayout();
    nested->setContentsMargins(20, 0, 0, 0);

    m_spinInterval->setRange(1, 1440);
    m_spinInterval->setSuffix(tr(" minutes"));
    const auto intervalLayout = new QHBoxLayout();
    intervalLayout->addWidget(new QLabel(tr("Interval:"), group));
    intervalLayout->addWidget(m_spinInterval);
    intervalLayout->addStretch();
    nested->addLayout(intervalLayout);

    nested->addWidget(new QLabel(tr("When there are new torrents:"), group));
    nested->addWidget(m_radioNotify);
    nested->addWidget(m_radioDownload);

    groupLayout->addWidget(m_checkAutoCheck);
    groupLayout->addLayout(nested);
    m_radioDownload->setToolTip(
        tr("Downloads the torrents your filters marked, so it does nothing while filters are "
           "turned off."));

    layout->addWidget(group);
  }

  layout->addStretch();

  connect(m_checkAutoCheck, &QCheckBox::toggled, m_spinInterval, &QWidget::setEnabled);
  connect(m_checkAutoCheck, &QCheckBox::toggled, m_radioNotify, &QWidget::setEnabled);
  connect(m_checkAutoCheck, &QCheckBox::toggled, m_radioDownload, &QWidget::setEnabled);
}

void TorrentsPage::load() {
  m_comboSource->setCurrentText(QString::fromStdString(akyuu::settings.torrentDiscoveryUrl()));
  m_comboSearch->setCurrentText(QString::fromStdString(akyuu::settings.torrentSearchUrl()));
  m_checkAutoCheck->setChecked(akyuu::settings.torrentAutoCheckEnabled());
  m_spinInterval->setValue(static_cast<int>(akyuu::settings.torrentAutoCheckInterval().count()));
  m_radioDownload->setChecked(akyuu::settings.torrentDownloadNewEpisodes());
  m_radioNotify->setChecked(!m_radioDownload->isChecked());
  m_spinInterval->setEnabled(m_checkAutoCheck->isChecked());
  m_radioNotify->setEnabled(m_checkAutoCheck->isChecked());
  m_radioDownload->setEnabled(m_checkAutoCheck->isChecked());
}

void TorrentsPage::save() {
  akyuu::settings.setTorrentDiscoveryUrl(m_comboSource->currentText().trimmed().toStdString());
  akyuu::settings.setTorrentSearchUrl(m_comboSearch->currentText().trimmed().toStdString());
  akyuu::settings.setTorrentAutoCheckEnabled(m_checkAutoCheck->isChecked());
  akyuu::settings.setTorrentAutoCheckInterval(std::chrono::minutes{m_spinInterval->value()});
  if (m_radioDownload->isChecked()) {
    akyuu::settings.setTorrentDownloadNewEpisodes(true);
  } else {
    akyuu::settings.setTorrentNotifyNewEpisodes(true);
  }

  track::aggregator()->applyAutoCheckSettings();
}

}  // namespace gui
