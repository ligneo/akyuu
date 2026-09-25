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

#include "application.hpp"

#include <QDir>
#include <QFileInfo>
#include <QLocalSocket>
#include <QMouseEvent>
#include <QStandardPaths>
#include <QTimer>
#include <QTranslator>
#include <chrono>
#include <format>

#include "akyuu/autostart.hpp"
#include "akyuu/config.h"
#include "akyuu/path.hpp"
#include "akyuu/settings.hpp"
#include "akyuu/version.hpp"
#include "base/log.hpp"
#include "base/string.hpp"
#include "gui/main/main_window.hpp"
#include "gui/utils/image_provider.hpp"
#include "gui/utils/theme.hpp"
#include "media/anime_db.hpp"
#include "media/anime_history.hpp"
#include "sync/queue.hpp"
#include "track/feed_archive.hpp"
#include "track/library.hpp"
#include "track/media.hpp"
#include "track/update_session.hpp"

namespace akyuu {

Application::Application(int argc, char* argv[])
    : QApplication(argc, argv),
      lock_file_(
          u"%1/%2.lock"_s.arg(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation))
              .arg(AKYUU_APP_NAME)) {
  setApplicationName("akyuu");
  setApplicationDisplayName("Akyuu");
  setDesktopFileName("io.github.ligneo.Akyuu");
  setApplicationVersion(QString::fromStdString(akyuu::version().to_string()));

  uptime_.start();
}

Application::~Application() {
  if (window_) {
    window_->hide();
  }
}

int Application::run() {
  parseCommandLine();

  migrate_taiga_data();
  initLogger();

  const auto version = akyuu::version().to_string();
  const auto fileInfo = QFileInfo{QCoreApplication::applicationFilePath()};
  const auto lastModified = fileInfo.lastModified().toString(Qt::DateFormat::ISODate);
  qDebug() << u"Version %1 (%2)"_s.arg(version).arg(lastModified);
  if (!parser_.optionNames().isEmpty()) {
    qDebug() << "Options:" << parser_.optionNames().join(", ");
  }

  if (hasPreviousInstance()) {
    activatePreviousInstance();
    qDebug() << "Another instance of Akyuu is running.";
    return 0;
  }

  // Remove the socket file left behind by an instance that did not exit cleanly
  QLocalServer::removeServer(AKYUU_APP_NAME);
  connect(&local_server_, &QLocalServer::newConnection, this, &Application::onNewConnection);
  local_server_.listen(AKYUU_APP_NAME);

  akyuu::settings.init();
  akyuu::applyAutoStart();
  anime::db.init();
  anime::history.init();
  sync::queue.init();
  track::archive.init();
  track::library()->applyWatchSettings();
  if (akyuu::settings.libraryScanOnStartup()) track::library()->scan();
  track::media::detection()->init();
  track::updateSession()->init();
  gui::imageProvider.init();

  gui::theme.initStyle();
  setWindowIcon(gui::theme.getIcon("akyuu", "png"));

  QTranslator translator;
  if (translator.load(QLocale::system(), "akyuu", "_", ":/i18n")) {
    installTranslator(&translator);
  }

  window_ = new gui::MainWindow();
  window_->init();

  // v1's `program/startup/minimize`: Akyuu starts in the tray and detects from there.
  // `--minimized` does the same for one launch, e.g. from a compositor's startup list.
  if (options_.minimized || akyuu::settings.appStartMinimized()) {
    return QApplication::exec();
  }

#ifdef Q_OS_WINDOWS
  // Delay showing the window to avoid a white flash.
  window_->setWindowOpacity(0.0);
  window_->show();
  QTimer::singleShot(std::chrono::milliseconds(50), window_, [this]() {
    if (window_) {
      window_->setWindowOpacity(1.0);
    }
  });
#else
  window_->show();
#endif

  return QApplication::exec();
}

// v1 shows this on its statistics page (`stats.uptime`).
base::Duration Application::uptime() const {
  return base::Duration{std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::milliseconds{uptime_.elapsed()})};
}

bool Application::isDebug() const {
  return options_.debug;
}

bool Application::isVerbose() const {
  return options_.verbose;
}

gui::MainWindow* Application::mainWindow() const {
  return window_.get();
}

bool Application::notify(QObject* receiver, QEvent* event) {
  // Restrict double-click events to left mouse button.
  if (event->type() == QEvent::MouseButtonDblClick &&
      static_cast<QMouseEvent*>(event)->button() != Qt::LeftButton) {
    return true;
  }

  return QApplication::notify(receiver, event);
}

bool Application::hasPreviousInstance() {
  // The lock is held for the lifetime of the application, so it must not become stale over time.
  // A lock file left behind by a process that is no longer running is removed by `tryLock`.
  lock_file_.setStaleLockTime(std::chrono::milliseconds::zero());
  return !lock_file_.tryLock();
}

void Application::activatePreviousInstance() {
  QLocalSocket socket;
  socket.connectToServer(AKYUU_APP_NAME);
  socket.waitForConnected(std::chrono::milliseconds(1000).count());
}

void Application::initLogger() const {
  const auto directory = u"%1/logs"_s.arg(get_data_path());
  QDir().mkpath(directory);

  const auto date = QDate::currentDate().toString(Qt::DateFormat::ISODate);
  const auto path = u"%1/%2_%3.log"_s.arg(directory).arg(AKYUU_APP_NAME).arg(date);

  base::initLogging(path, options_.debug ? QtDebugMsg : QtWarningMsg);
}

void Application::onNewConnection() {
  while (auto socket = local_server_.nextPendingConnection()) {
    socket->deleteLater();
  }
  if (window_) {
    window_->displayWindow();
  }
}

void Application::parseCommandLine() {
  parser_.addOptions({
      {"debug", QCoreApplication::translate("main", "Enable debug mode")},
      {"minimized", QCoreApplication::translate("main", "Start in the system tray")},
      {"verbose", QCoreApplication::translate("main", "Enable verbose output")},
  });

  // This stops the current process in case of an error (e.g. an unknown option was passed).
  parser_.process(QApplication::arguments());

#ifdef _DEBUG
  options_.debug = true;
#else
  options_.debug = parser_.isSet("debug");
#endif
  options_.minimized = parser_.isSet("minimized");
  options_.verbose = parser_.isSet("verbose");
}

}  // namespace akyuu
