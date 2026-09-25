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

#include "settings_accounts_page.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QFormLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

#include "akyuu/accounts.hpp"
#include "akyuu/settings.hpp"
#include "base/string.hpp"
#include "gui/main/main_window.hpp"
#include "sync/anilist/anilist.hpp"
#include "sync/anilist/anilist_utils.hpp"
#include "sync/myanimelist/myanimelist.hpp"
#include "sync/myanimelist/myanimelist_utils.hpp"
#include "sync/service.hpp"
#include "ui_main_window.h"

namespace gui {

namespace {

QLabel* createLink(const QString& url, const QString& text, QWidget* parent) {
  const auto label = new QLabel(u"<a href=\"%1\">%2</a>"_s.arg(url, text), parent);
  label->setOpenExternalLinks(true);
  return label;
}

QHBoxLayout* createButtonRow(QPushButton* button, QLabel* link) {
  const auto layout = new QHBoxLayout();
  layout->addWidget(button);
  layout->addWidget(link);
  layout->addStretch();
  return layout;
}

// The page shown after authorization may display the token itself, or the address bar may contain
// it as a parameter. Accept both, so that users can paste whichever they have.
QString extractToken(const QString& text, const QString& parameter) {
  const auto input = text.trimmed();
  if (!input.contains("://")) return input;

  const QUrl url{input};
  for (const auto& query : {QUrlQuery{url.fragment()}, QUrlQuery{url.query()}}) {
    if (query.hasQueryItem(parameter)) return query.queryItemValue(parameter);
  }

  return {};
}

bool enterAuthorizationPin(QWidget* parent, const QString& service, QString& pin) {
  bool ok = false;
  const auto text = QInputDialog::getText(
      parent, AccountsPage::tr("Authorize %1").arg(service),
      AccountsPage::tr("Please enter the PIN shown on the page after logging in to %1:")
          .arg(service),
      QLineEdit::Normal, {}, &ok);
  pin = text;
  return ok && !text.trimmed().isEmpty();
}

}  // namespace

AccountsPage::AccountsPage(QWidget* parent)
    : SettingsPage(parent),
      m_comboService(new QComboBox(this)),
      m_anilistStatus(new QLabel(this)),
      m_anilistButton(new QPushButton(this)),
      m_myanimelistStatus(new QLabel(this)),
      m_myanimelistButton(new QPushButton(this)),
      m_kitsuEmail(new QLineEdit(this)),
      m_kitsuPassword(new QLineEdit(this)) {
  // v1's Services section: the synchronization settings, then one tab per service.
  const auto tabs = new QTabWidget(this);
  tabs->setStyleSheet(u"QTabWidget::tab-bar { alignment: left; }"_s);
  const auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(tabs);

  const auto add_tab = [tabs](const QString& title, QGroupBox* group) {
    const auto page = new QWidget(tabs);
    const auto pageLayout = new QVBoxLayout(page);
    pageLayout->addWidget(group);
    pageLayout->addStretch();
    tabs->addTab(page, title);
  };

  // Main
  {
    const auto group = new QGroupBox(tr("Synchronization"), this);
    const auto groupLayout = new QVBoxLayout(group);

    for (const auto id :
         {sync::ServiceId::AniList, sync::ServiceId::Kitsu, sync::ServiceId::MyAnimeList}) {
      m_comboService->addItem(sync::serviceName(id), sync::serviceSlug(id));
    }
    groupLayout->addWidget(new QLabel(tr("Active service and metadata provider:"), group));
    groupLayout->addWidget(m_comboService);

    const auto note = new QLabel(
        tr("Note: Akyuu is unable to synchronize multiple services at the same time."), group);
    note->setWordWrap(true);
    note->setForegroundRole(QPalette::PlaceholderText);
    groupLayout->addWidget(note);

    m_checkSyncOnStartup = new QCheckBox(tr("Synchronize automatically at startup"), group);
    groupLayout->addWidget(m_checkSyncOnStartup);

    add_tab(tr("Main"), group);
  }

  // MyAnimeList
  {
    const auto group = new QGroupBox(tr("Account"), this);
    const auto form = new QFormLayout(group);

    form->addRow(tr("Account:"), m_myanimelistStatus);
    form->addRow(createButtonRow(m_myanimelistButton,
                                 createLink("https://myanimelist.net/register.php",
                                            tr("Create a new MyAnimeList account"), group)));

    connect(m_myanimelistButton, &QPushButton::clicked, this, &AccountsPage::authorizeMyanimelist);

    add_tab(u"MyAnimeList"_s, group);
  }

  // Kitsu
  {
    const auto group = new QGroupBox(tr("Account"), this);
    const auto form = new QFormLayout(group);

    m_kitsuPassword->setEchoMode(QLineEdit::Password);

    form->addRow(tr("Email address:"), m_kitsuEmail);
    form->addRow(tr("Password:"), m_kitsuPassword);
    form->addRow(createLink("https://kitsu.app", tr("Create a new Kitsu account"), group));

    add_tab(u"Kitsu"_s, group);
  }

  // AniList
  {
    const auto group = new QGroupBox(tr("Account"), this);
    const auto form = new QFormLayout(group);

    form->addRow(tr("Account:"), m_anilistStatus);
    form->addRow(createButtonRow(
        m_anilistButton,
        createLink("https://anilist.co", tr("Create a new AniList account"), group)));

    connect(m_anilistButton, &QPushButton::clicked, this, &AccountsPage::authorizeAnilist);

    add_tab(u"AniList"_s, group);
  }

  connect(&akyuu::accounts, &akyuu::Accounts::authenticationChanged, this,
          &AccountsPage::updateStatus);
}

void AccountsPage::load() {
  const auto service = QString::fromStdString(akyuu::settings.service());
  m_comboService->setCurrentIndex(m_comboService->findData(service));
  m_checkSyncOnStartup->setChecked(akyuu::settings.syncOnStartup());

  m_kitsuEmail->setText(QString::fromStdString(akyuu::accounts.kitsuEmail()));
  m_kitsuPassword->setText(QString::fromStdString(akyuu::accounts.kitsuPassword()));

  updateStatus();
}

void AccountsPage::save() {
  const auto previousService = akyuu::settings.service();
  const auto service = m_comboService->currentData().toString().toStdString();
  akyuu::settings.setService(service);
  akyuu::settings.setSyncOnStartup(m_checkSyncOnStartup->isChecked());

  const auto email = m_kitsuEmail->text().trimmed().toStdString();
  const auto password = m_kitsuPassword->text().toStdString();
  const bool kitsuChanged =
      email != akyuu::accounts.kitsuEmail() || password != akyuu::accounts.kitsuPassword();
  akyuu::accounts.setKitsuEmail(email);
  akyuu::accounts.setKitsuPassword(password);

  if (sync::currentServiceId() == sync::ServiceId::Kitsu && kitsuChanged && !email.empty() &&
      !password.empty()) {
    akyuu::accounts.setKitsuAuthenticated(false);
    sync::authenticateUser();
  } else if (service != previousService) {
    mainWindow()->ui()->actionSynchronize->trigger();
  }
}

void AccountsPage::authorizeAnilist() {
  QDesktopServices::openUrl(QUrl{QString::fromStdString(sync::anilist::requestTokenUrl())});

  QString pin;
  if (!enterAuthorizationPin(this, u"AniList"_s, pin)) return;

  const auto token = extractToken(pin, "access_token");
  if (token.isEmpty()) return;

  sync::anilist::Service::instance()->setAccessToken(token);
  akyuu::accounts.setAnilistAuthenticated(false);

  if (sync::currentServiceId() == sync::ServiceId::AniList) {
    mainWindow()->ui()->actionSynchronize->trigger();
  }
}

void AccountsPage::authorizeMyanimelist() {
  std::string codeVerifier;
  const auto url = sync::myanimelist::authorizationCodeUrl(codeVerifier);
  QDesktopServices::openUrl(QUrl{QString::fromStdString(url)});

  QString pin;
  if (!enterAuthorizationPin(this, u"MyAnimeList"_s, pin)) return;

  const auto code = extractToken(pin, "code");
  if (code.isEmpty()) return;

  sync::myanimelist::Service::instance()->requestAccessToken(code,
                                                             QString::fromStdString(codeVerifier));
}

void AccountsPage::updateStatus() {
  const auto status = [](const bool authenticated, const std::string& username) {
    if (!authenticated) return tr("Not authorized");
    return username.empty() ? tr("Authorized") : tr("Authorized as %1").arg(username);
  };

  m_anilistStatus->setText(
      status(akyuu::accounts.anilistAuthenticated(), akyuu::accounts.anilistUsername()));
  m_anilistButton->setText(akyuu::accounts.anilistToken().empty() ? tr("Authorize...")
                                                                  : tr("Re-authorize..."));

  m_myanimelistStatus->setText(
      status(akyuu::accounts.myanimelistAuthenticated(), akyuu::accounts.myanimelistUsername()));
  m_myanimelistButton->setText(akyuu::accounts.myanimelistAccessToken().empty()
                                   ? tr("Authorize...")
                                   : tr("Re-authorize..."));
}

}  // namespace gui
