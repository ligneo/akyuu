/**
 * Taiga
 * Copyright (C) 2010-2025, Eren Okka
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

#include "about_dialog.hpp"

#include <utf8proc.h>

#include <QEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>

#include "akyuu/orange.hpp"
#include "akyuu/session.hpp"
#include "akyuu/version.hpp"
#include "base/string.hpp"

namespace gui {

QString getAboutDialogText(QWidget* parent) {
  const auto version = QString::fromStdString(akyuu::version().to_string());

  const QStringList links{
      u"<a href='https://github.com/ligneo/akyuu'>GitHub</a>"_s,
  };

  const QStringList taigaLinks{
      u"<a href='https://taiga.moe/'>Website</a>"_s,
      u"<a href='https://github.com/erengy/taiga'>GitHub</a>"_s,
  };

  // clang-format off
  const QStringList contributors{
      u"saka"_s,
      u"Diablofan"_s,
      u"slevir"_s,
      u"LordGravewish"_s,
      u"rr-"_s,
      u"sunjayc"_s,
      u"ConnorKrammer"_s,
      u"Soinou"_s,
      u"Jiyuu"_s,
      u"ryban"_s,
      u"tollyx"_s,
      u"pavelxdd"_s,
      u"gunt3001"_s,
      u"synthtech"_s,
      u"cnguy"_s,
      u"CeruleanSky"_s,
      u"Xabis"_s,
      u"rzumer"_s,
      u"Juplay"_s,
      u"SacredZenpie"_s,
  };
  // clang-format on

  // clang-format off
  const QStringList donators{
      u"Farfie"_s,
      u"snickler"_s,
      u"Nydaleclya"_s,
      u"WizardTim"_s,
      u"Kinzer"_s,
      u"MeGaNeKo"_s,
      u"WhatsCPS"_s,
      u"Jerico64"_s,
      u"Xen the Greedy"_s,
      parent->tr("and other anonymous supporters"),
  };
  // clang-format on

  const QStringList components{
      u"Material Symbols"_s,
      u"Qt %1"_s.arg(QT_VERSION_STR),
      u"utf8proc %1"_s.arg(utf8proc_version()),
  };

  QStringList sections;

  const auto addSection = [&sections](QString title, QString text) {
    sections.append(u"<b>%1:</b><br>%2"_s.arg(title).arg(text));
  };

  sections.append(u"<big><b>Akyuu</b> %1</big>"_s.arg(version));
  sections.append(links.join(" · "));
  addSection(parent->tr("Author"),
             u"cenky (<a href='mailto:cenkkgl@gmail.com'>cenkkgl@gmail.com</a>)"_s);
  addSection(parent->tr("Based on"),
             u"Taiga by Eren Okka (erengy) · %1"_s.arg(taigaLinks.join(" · ")));
  addSection(parent->tr("Taiga contributors"), contributors.join(", "));
  addSection(parent->tr("Taiga donators"), donators.join(", "));
  addSection(parent->tr("Third-party components"), components.join(", "));

  // GPLv3 section 5 asks a modified version to say so, with a date, and section 0 asks an
  // interactive program to show the copyright, the lack of warranty and where to find the license.
  addSection(parent->tr("License"),
             parent->tr("Copyright (C) 2010-2026, Eren Okka<br>"
                        "Copyright (C) 2026, cenky<br>"
                        "Akyuu is a modified version of Taiga, changed since September 2026.<br>"
                        "This program comes with ABSOLUTELY NO WARRANTY. It is free software, "
                        "released under the <a href='https://www.gnu.org/licenses/gpl-3.0.html'>"
                        "GNU General Public License v3</a> or later."));

  return sections.join("<br><br>");
}

void displayAboutDialog(QWidget* parent) {
  const auto msgBox = new QMessageBox(parent);

  // See `QMessageBox::about()`
  msgBox->setAttribute(Qt::WA_DeleteOnClose);
  msgBox->setIconPixmap(parent->windowIcon().pixmap(QSize(64, 64), msgBox->devicePixelRatio()));
  msgBox->setText(getAboutDialogText(parent));
  msgBox->setWindowTitle(parent->tr("About Akyuu"));

  if (const auto iconLabel = msgBox->findChild<QLabel*>()) {
    const auto handler = new AboutDialogHandler(msgBox);
    iconLabel->installEventFilter(handler);
  }

  msgBox->exec();
}

AboutDialogHandler::AboutDialogHandler(QObject* parent)
    : QObject(parent), orange_(akyuu::orange()) {
  connect(orange_, &QThread::finished, this, &AboutDialogHandler::resetWindowTitle);
}

void AboutDialogHandler::resetWindowTitle() {
  messageBox()->setWindowTitle(this->previousWindowTitle_);
}

QMessageBox* AboutDialogHandler::messageBox() const {
  return static_cast<QMessageBox*>(this->parent());
}

bool AboutDialogHandler::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() == QEvent::MouseButtonDblClick) {
    if (!orange_->isRunning()) {
      previousWindowTitle_ = messageBox()->windowTitle();
      messageBox()->setWindowTitle("Orange");
      akyuu::session.setTigersHarmed(akyuu::session.tigersHarmed() + 1);
      orange_->start();
    }
    return true;
  }

  return QObject::eventFilter(watched, event);
}

}  // namespace gui
