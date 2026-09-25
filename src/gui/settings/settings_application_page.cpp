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

#include "settings_application_page.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QStyleFactory>
#include <QVBoxLayout>
#include <algorithm>

#include "akyuu/autostart.hpp"
#include "akyuu/settings.hpp"
#include "gui/utils/theme.hpp"

namespace gui {

ApplicationPage::ApplicationPage(QWidget* parent)
    : SettingsPage(parent),
      m_comboStyle(new QComboBox(this)),
      m_comboColorScheme(new QComboBox(this)),
      m_checkAutoStart(new QCheckBox(tr("Start Akyuu when the session begins"), this)),
      m_checkStartMinimized(new QCheckBox(tr("Start minimized to the tray"), this)),
      m_checkCloseToTray(new QCheckBox(tr("Minimize to tray when closed"), this)),
      m_checkMinimizeToTray(new QCheckBox(tr("Minimize to tray when minimized"), this)) {
  const auto layout = new QVBoxLayout(this);

  // Appearance
  {
    const auto group = new QGroupBox(tr("Appearance"), this);
    const auto form = new QFormLayout(group);

    {
      const QString system{akyuu::Settings::kAppStyleSystem};
      auto keys = QStyleFactory::keys();
      const auto style = QString::fromStdString(akyuu::settings.appStyle()).toLower();
      if (style != system && !keys.contains(style, Qt::CaseInsensitive)) {
        keys.append(style);  // keep unavailable style
      }
      keys.sort(Qt::CaseInsensitive);
      m_comboStyle->addItem(tr("System"), system);
      for (const auto& key : keys) {
        m_comboStyle->addItem(key, key.toLower());
      }
      form->addRow(tr("Style:"), m_comboStyle);
    }
    m_comboColorScheme->addItem(tr("System default"), static_cast<int>(Qt::ColorScheme::Unknown));
    m_comboColorScheme->addItem(tr("Light"), static_cast<int>(Qt::ColorScheme::Light));
    m_comboColorScheme->addItem(tr("Dark"), static_cast<int>(Qt::ColorScheme::Dark));
    form->addRow(tr("Color scheme:"), m_comboColorScheme);

    layout->addWidget(group);
  }

  // Startup
  {
    const auto group = new QGroupBox(tr("Startup"), this);
    const auto groupLayout = new QVBoxLayout(group);
    groupLayout->addWidget(m_checkAutoStart);
    groupLayout->addWidget(m_checkStartMinimized);
    layout->addWidget(group);
  }

  // System tray
  {
    const auto group = new QGroupBox(tr("System tray"), this);
    const auto groupLayout = new QVBoxLayout(group);
    groupLayout->addWidget(m_checkCloseToTray);
    groupLayout->addWidget(m_checkMinimizeToTray);
    layout->addWidget(group);
  }

  layout->addStretch();
}

void ApplicationPage::load() {
  const auto style = QString::fromStdString(akyuu::settings.appStyle()).toLower();
  m_comboStyle->setCurrentIndex(std::max(0, m_comboStyle->findData(style)));

  const auto scheme = static_cast<int>(akyuu::settings.appColorScheme());
  m_comboColorScheme->setCurrentIndex(m_comboColorScheme->findData(scheme));

  m_checkAutoStart->setChecked(akyuu::settings.appAutoStart());
  m_checkStartMinimized->setChecked(akyuu::settings.appStartMinimized());
  m_checkCloseToTray->setChecked(akyuu::settings.appCloseToTray());
  m_checkMinimizeToTray->setChecked(akyuu::settings.appMinimizeToTray());
}

void ApplicationPage::save() {
  akyuu::settings.setAppAutoStart(m_checkAutoStart->isChecked());
  akyuu::settings.setAppStartMinimized(m_checkStartMinimized->isChecked());
  akyuu::settings.setAppCloseToTray(m_checkCloseToTray->isChecked());
  akyuu::settings.setAppMinimizeToTray(m_checkMinimizeToTray->isChecked());
  akyuu::applyAutoStart();

  const auto style = m_comboStyle->currentData().toString().toStdString();
  const auto scheme = m_comboColorScheme->currentData().value<Qt::ColorScheme>();
  if (style == akyuu::settings.appStyle() && scheme == akyuu::settings.appColorScheme()) return;

  akyuu::settings.setAppStyle(style);
  akyuu::settings.setAppColorScheme(scheme);
  theme.applyStyle();
}

}  // namespace gui
