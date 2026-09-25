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

#include "settings_stream_list.hpp"

#include <algorithm>

#include "akyuu/settings.hpp"
#include "track/media_stream.hpp"

namespace gui {

StreamListWidget::StreamListWidget(QWidget* parent) : QListWidget(parent) {
  for (const auto& stream : track::recognition::streamData()) {
    const auto item = new QListWidgetItem(stream.name, this);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
  }

  sortItems();
}

void StreamListWidget::load() {
  const auto disabledProviders = taiga::settings.disabledStreamingProviders();

  for (int i = 0; i < count(); ++i) {
    const auto name = item(i)->text().toStdString();
    const bool disabled = std::ranges::contains(disabledProviders, name);
    item(i)->setCheckState(disabled ? Qt::Unchecked : Qt::Checked);
  }
}

void StreamListWidget::save() const {
  std::vector<std::string> disabledProviders;

  for (int i = 0; i < count(); ++i) {
    if (item(i)->checkState() == Qt::Unchecked) {
      disabledProviders.push_back(item(i)->text().toStdString());
    }
  }

  std::ranges::sort(disabledProviders);
  taiga::settings.setDisabledStreamingProviders(disabledProviders);
}

}  // namespace gui
