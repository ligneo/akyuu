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
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "akyuu/settings.hpp"

namespace gui {

HttpPage::HttpPage(QWidget* parent)
    : SettingsPage(parent),
      m_checkEnabled(new QCheckBox(tr("Send HTTP request"), this)),
      m_editUrl(new QLineEdit(this)),
      m_buttonFormat(new QPushButton(tr("Edit format string..."), this)) {
  const auto layout = new QVBoxLayout(this);

  const auto group = new QGroupBox(tr("Options"), this);
  const auto groupLayout = new QVBoxLayout(group);

  m_editUrl->setPlaceholderText(tr("https://example.com/taiga"));

  layout->addWidget(m_checkEnabled);
  groupLayout->addWidget(new QLabel(tr("URL:"), group));
  groupLayout->addWidget(m_editUrl);
  const auto buttonLayout = new QHBoxLayout();
  buttonLayout->addWidget(m_buttonFormat);
  buttonLayout->addStretch();
  groupLayout->addLayout(buttonLayout);

  layout->addWidget(group);

  const auto note = new QLabel(
      tr("Tip: Formatted string will be sent as the body data of a POST request."), this);
  note->setWordWrap(true);
  note->setForegroundRole(QPalette::PlaceholderText);
  layout->addWidget(note);
  layout->addStretch();

  // v1 runs the body through its script language; only the plain variables are read here.
  connect(m_buttonFormat, &QPushButton::clicked, this, [this]() {
    bool ok = false;
    const auto text = QInputDialog::getMultiLineText(
        this, tr("Format string"),
        tr("Variables: %title%, %episode%, %total%, %watched%, %score%, %image%, %group%. Each "
           "value is percent-encoded."),
        m_format, &ok);
    if (ok) m_format = text.trimmed();
  });

  connect(m_checkEnabled, &QCheckBox::toggled, m_editUrl, &QWidget::setEnabled);
  connect(m_checkEnabled, &QCheckBox::toggled, m_buttonFormat, &QWidget::setEnabled);
}

void HttpPage::load() {
  m_checkEnabled->setChecked(akyuu::settings.httpShareEnabled());
  m_editUrl->setText(QString::fromStdString(akyuu::settings.httpShareUrl()));
  m_format = QString::fromStdString(akyuu::settings.httpShareFormat());
  m_editUrl->setEnabled(m_checkEnabled->isChecked());
  m_buttonFormat->setEnabled(m_checkEnabled->isChecked());
}

void HttpPage::save() {
  akyuu::settings.setHttpShareEnabled(m_checkEnabled->isChecked());
  akyuu::settings.setHttpShareUrl(m_editUrl->text().trimmed().toStdString());
  akyuu::settings.setHttpShareFormat(m_format.toStdString());
}

}  // namespace gui
