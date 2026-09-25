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

#include "settings_irc_page.hpp"

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QRadioButton>
#include <QVBoxLayout>
#include <initializer_list>

#include "link/irc.hpp"
#include "akyuu/settings.hpp"

namespace gui {

using namespace Qt::StringLiterals;

IrcPage::IrcPage(QWidget* parent)
    : SettingsPage(parent),
      m_checkEnabled(new QCheckBox(tr("Announce what you are watching to IRC"), this)),
      m_radioAllChannels(new QRadioButton(tr("Every channel you have joined"), this)),
      m_radioCustomChannels(new QRadioButton(tr("These channels only:"), this)),
      m_editChannels(new QLineEdit(this)),
      m_checkUseAction(new QCheckBox(tr("Send as an action (/me)"), this)),
      m_editFormat(new QPlainTextEdit(this)),
      m_labelStatus(new QLabel(this)) {
  const auto layout = new QVBoxLayout(this);

  const auto group = new QGroupBox(tr("IRC"), this);
  const auto groupLayout = new QVBoxLayout(group);

  m_editChannels->setPlaceholderText(tr("#kitsu, #myanimelist, #taiga"));
  m_editFormat->setMaximumHeight(80);
  m_labelStatus->setWordWrap(true);

  groupLayout->addWidget(m_checkEnabled);
  groupLayout->addWidget(m_labelStatus);
  groupLayout->addWidget(m_radioAllChannels);
  groupLayout->addWidget(m_radioCustomChannels);
  groupLayout->addWidget(m_editChannels);
  groupLayout->addWidget(m_checkUseAction);
  groupLayout->addWidget(new QLabel(tr("Message:"), group));
  groupLayout->addWidget(m_editFormat);

  // The colour codes in the default message are the ones v1 uses, and mIRC before it.
  const auto note = new QLabel(
      tr("The message understands the same variables and functions as v1, and a channel is only "
         "written to while you are in it."),
      group);
  note->setWordWrap(true);
  groupLayout->addWidget(note);

  layout->addWidget(group);
  layout->addStretch();

  const std::initializer_list<QWidget*> dependents{m_radioAllChannels, m_radioCustomChannels,
                                                   m_checkUseAction, m_editFormat};
  for (const auto widget : dependents) {
    connect(m_checkEnabled, &QCheckBox::toggled, widget, &QWidget::setEnabled);
  }

  connect(m_checkEnabled, &QCheckBox::toggled, this, [this]() {
    m_editChannels->setEnabled(m_checkEnabled->isChecked() && m_radioCustomChannels->isChecked());
  });
  connect(m_radioCustomChannels, &QRadioButton::toggled, m_editChannels, &QWidget::setEnabled);
}

void IrcPage::load() {
  m_checkEnabled->setChecked(taiga::settings.ircShareEnabled());
  m_editChannels->setText(QString::fromStdString(taiga::settings.ircShareChannels()));
  m_checkUseAction->setChecked(taiga::settings.ircShareUseAction());
  m_editFormat->setPlainText(QString::fromStdString(taiga::settings.ircShareFormat()));

  if (taiga::settings.ircShareAllChannels()) {
    m_radioAllChannels->setChecked(true);
  } else {
    m_radioCustomChannels->setChecked(true);
  }

  updateStatus();

  const auto enabled = m_checkEnabled->isChecked();
  m_radioAllChannels->setEnabled(enabled);
  m_radioCustomChannels->setEnabled(enabled);
  m_editChannels->setEnabled(enabled && m_radioCustomChannels->isChecked());
  m_checkUseAction->setEnabled(enabled);
  m_editFormat->setEnabled(enabled);
}

void IrcPage::save() {
  taiga::settings.setIrcShareEnabled(m_checkEnabled->isChecked());
  taiga::settings.setIrcShareAllChannels(m_radioAllChannels->isChecked());
  taiga::settings.setIrcShareChannels(m_editChannels->text().trimmed().toStdString());
  taiga::settings.setIrcShareUseAction(m_checkUseAction->isChecked());
  taiga::settings.setIrcShareFormat(m_editFormat->toPlainText().trimmed().toStdString());
}

// Nothing is sent to a client that is not running, so the page says whether one was found.
void IrcPage::updateStatus() {
  if (!link::irc::isRunning()) {
    m_labelStatus->setText(
        tr("Konversation is not running. Nothing will be announced until it is."));
    return;
  }

  QStringList channels;
  for (const auto& connection : link::irc::connections()) {
    for (const auto& channel : link::irc::joinedChannels(connection)) {
      channels.append(u"%1/%2"_s.arg(connection, channel));
    }
  }

  m_labelStatus->setText(channels.isEmpty()
                             ? tr("Konversation is running, but you have not joined a channel.")
                             : tr("Joined: %1").arg(channels.join(u", "_s)));
}

}  // namespace gui
