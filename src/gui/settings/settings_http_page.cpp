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

#include "settings_http_page.hpp"

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QVBoxLayout>

#include "akyuu/settings.hpp"

namespace gui {

HttpPage::HttpPage(QWidget* parent)
    : SettingsPage(parent),
      m_checkEnabled(new QCheckBox(tr("Announce what you are watching over HTTP"), this)),
      m_editUrl(new QLineEdit(this)),
      m_editFormat(new QPlainTextEdit(this)) {
  const auto layout = new QVBoxLayout(this);

  const auto group = new QGroupBox(tr("HTTP"), this);
  const auto groupLayout = new QVBoxLayout(group);

  m_editUrl->setPlaceholderText(tr("https://example.com/akyuu"));
  m_editFormat->setMaximumHeight(80);

  groupLayout->addWidget(m_checkEnabled);
  groupLayout->addWidget(new QLabel(tr("Address:"), group));
  groupLayout->addWidget(m_editUrl);
  groupLayout->addWidget(new QLabel(tr("Body:"), group));
  groupLayout->addWidget(m_editFormat);

  // v1 runs the body through its script language; only the plain variables are read here.
  const auto note = new QLabel(
      tr("Understood variables: %title%, %episode%, %total%, %watched%, %score%, %image%, "
         "%group%. Each value is percent-encoded."),
      group);
  note->setWordWrap(true);
  groupLayout->addWidget(note);

  layout->addWidget(group);
  layout->addStretch();

  connect(m_checkEnabled, &QCheckBox::toggled, m_editUrl, &QWidget::setEnabled);
  connect(m_checkEnabled, &QCheckBox::toggled, m_editFormat, &QWidget::setEnabled);
}

void HttpPage::load() {
  m_checkEnabled->setChecked(akyuu::settings.httpShareEnabled());
  m_editUrl->setText(QString::fromStdString(akyuu::settings.httpShareUrl()));
  m_editFormat->setPlainText(QString::fromStdString(akyuu::settings.httpShareFormat()));
  m_editUrl->setEnabled(m_checkEnabled->isChecked());
  m_editFormat->setEnabled(m_checkEnabled->isChecked());
}

void HttpPage::save() {
  akyuu::settings.setHttpShareEnabled(m_checkEnabled->isChecked());
  akyuu::settings.setHttpShareUrl(m_editUrl->text().trimmed().toStdString());
  akyuu::settings.setHttpShareFormat(m_editFormat->toPlainText().trimmed().toStdString());
}

}  // namespace gui
