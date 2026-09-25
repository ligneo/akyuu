/**
 * Akyuu
 * Copyright (C) 2010-2024, Eren Okka
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

#pragma once

#include <QDialog>
#include <map>
#include <utility>
#include <vector>

class QTabWidget;
class QTreeWidgetItem;

namespace Ui {
class SettingsDialog;
}

namespace gui {

class SettingsPage;

// Pages that something outside the dialog needs to open directly.
enum class SettingsPageId {
  Accounts,
  Library,
  TorrentFilters,
};

class SettingsDialog final : public QDialog {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(SettingsDialog)

public:
  SettingsDialog(QWidget* parent);
  ~SettingsDialog() = default;

  static void show(QWidget* parent, const SettingsPageId page = SettingsPageId::Accounts);

public slots:
  void accept() override;

private:
  int addPage(QTabWidget* tabs, const QString& title, SettingsPage* page);
  void setCurrentPage(const SettingsPageId page);

  Ui::SettingsDialog* ui_ = nullptr;
  std::vector<SettingsPage*> pages_;
  std::map<SettingsPageId, std::pair<QTreeWidgetItem*, int>> items_;
};

}  // namespace gui
