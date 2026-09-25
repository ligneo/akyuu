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
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>
#include <initializer_list>

#include "akyuu/settings.hpp"
#include "link/irc.hpp"

namespace gui {

using namespace Qt::StringLiterals;

IrcPage::IrcPage(QWidget* parent)
    : SettingsPage(parent),
      m_checkEnabled(new QCheckBox(tr("Send message"), this)),
      m_radioAllChannels(new QRadioButton(tr("All open channels"), this)),
      m_radioCustomChannels(new QRadioButton(tr("Custom: (separate by a comma)"), this)),
      m_editChannels(new QLineEdit(this)),
      m_checkUseAction(new QCheckBox(tr("Use \"/me\" action"), this)),
      m_buttonFormat(new QPushButton(tr("Edit format string..."), this)),
      m_labelStatus(new QLabel(this)) {
  const auto layout = new QVBoxLayout(this);
  layout->addWidget(m_checkEnabled);

  // Laid out as v1's mIRC page: options, the client, then where to send
  {
    const auto group = new QGroupBox(tr("Options"), this);
    const auto groupLayout = new QHBoxLayout(group);
    groupLayout->addWidget(m_buttonFormat);
    groupLayout->addStretch();
    groupLayout->addWidget(m_checkUseAction);
    layout->addWidget(group);
  }

  {
    const auto group = new QGroupBox(tr("Service"), this);
    const auto groupLayout = new QHBoxLayout(group);
    m_labelStatus->setWordWrap(true);
    groupLayout->addWidget(m_labelStatus, 1);
    const auto buttonTest = new QPushButton(tr("Test connection"), group);
    groupLayout->addWidget(buttonTest, 0, Qt::AlignTop);
    connect(buttonTest, &QPushButton::clicked, this, &IrcPage::updateStatus);
    layout->addWidget(group);
  }

  {
    const auto group = new QGroupBox(tr("Channels to send message"), this);
    const auto groupLayout = new QVBoxLayout(group);
    m_editChannels->setPlaceholderText(tr("#kitsu, #myanimelist, #taiga"));
    groupLayout->addWidget(m_radioAllChannels);
    groupLayout->addWidget(m_radioCustomChannels);
    const auto nested = new QVBoxLayout();
    nested->setContentsMargins(20, 0, 0, 0);
    nested->addWidget(m_editChannels);
    groupLayout->addLayout(nested);
    layout->addWidget(group);
  }

  // The colour codes in the default message are the ones v1 uses, and mIRC before it.
  const auto note = new QLabel(
      tr("Note: Messages are sent through Konversation, to channels you have joined."), this);
  note->setWordWrap(true);
  note->setForegroundRole(QPalette::PlaceholderText);
  layout->addWidget(note);
  layout->addStretch();

  connect(m_buttonFormat, &QPushButton::clicked, this, [this]() {
    bool ok = false;
    const auto text = QInputDialog::getMultiLineText(
        this, tr("Format string"),
        tr("The message understands the same variables and functions as the notification."),
        m_format, &ok);
    if (ok) m_format = text.trimmed();
  });

  const std::initializer_list<QWidget*> dependents{m_radioAllChannels, m_radioCustomChannels,
                                                   m_checkUseAction, m_buttonFormat};
  for (const auto widget : dependents) {
    connect(m_checkEnabled, &QCheckBox::toggled, widget, &QWidget::setEnabled);
  }

  connect(m_checkEnabled, &QCheckBox::toggled, this, [this]() {
    m_editChannels->setEnabled(m_checkEnabled->isChecked() && m_radioCustomChannels->isChecked());
  });
  connect(m_radioCustomChannels, &QRadioButton::toggled, m_editChannels, &QWidget::setEnabled);
}

void IrcPage::load() {
  m_checkEnabled->setChecked(akyuu::settings.ircShareEnabled());
  m_editChannels->setText(QString::fromStdString(akyuu::settings.ircShareChannels()));
  m_checkUseAction->setChecked(akyuu::settings.ircShareUseAction());
  m_format = QString::fromStdString(akyuu::settings.ircShareFormat());

  if (akyuu::settings.ircShareAllChannels()) {
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
  m_buttonFormat->setEnabled(enabled);
}

void IrcPage::save() {
  akyuu::settings.setIrcShareEnabled(m_checkEnabled->isChecked());
  akyuu::settings.setIrcShareAllChannels(m_radioAllChannels->isChecked());
  akyuu::settings.setIrcShareChannels(m_editChannels->text().trimmed().toStdString());
  akyuu::settings.setIrcShareUseAction(m_checkUseAction->isChecked());
  akyuu::settings.setIrcShareFormat(m_format.toStdString());
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
