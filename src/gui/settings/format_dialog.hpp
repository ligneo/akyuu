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

#pragma once

#include <QDialog>
#include <QString>
#include <optional>

class QMenu;
class QPlainTextEdit;

namespace gui {

// v1's `dlg_format`: which format string is edited decides the preview and the extra characters
// the Add menu offers.
enum class FormatDialogMode {
  Http,
  Irc,
  Notification,
};

class FormatDialog final : public QDialog {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(FormatDialog)

public:
  FormatDialog(QWidget* parent, FormatDialogMode mode, const QString& format);
  ~FormatDialog() = default;

  QString format() const;

  static std::optional<QString> edit(QWidget* parent, FormatDialogMode mode, const QString& format);

private:
  QMenu* createAddMenu();
  void insertText(const QString& text);
  void refreshPreview();

  FormatDialogMode m_mode;
  QPlainTextEdit* m_editFormat = nullptr;
  QPlainTextEdit* m_editPreview = nullptr;
};

}  // namespace gui
