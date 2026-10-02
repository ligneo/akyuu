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

#include <QUrl>
#include <optional>
#include <vector>

namespace akyuu {

enum class UpdatePlatform { Linux, Windows, MacOS, Unknown };
enum class PackageFormat { AppImage, Arch, WindowsInstaller, MacDiskImage };

struct UpdateTarget {
  UpdatePlatform platform = UpdatePlatform::Unknown;
  QString architecture;
  PackageFormat format = PackageFormat::AppImage;
};

struct ReleaseAsset {
  QString name;
  QUrl url;
  qint64 size = 0;
  QByteArray sha256;
  UpdateTarget target;
};

UpdateTarget updateTarget(const QString& kernel, const QString& architecture,
                          const QString& packageFormat);
UpdateTarget updateTarget();
std::optional<ReleaseAsset> selectPackage(const std::vector<ReleaseAsset>& assets,
                                          const UpdateTarget& target);
bool validReleaseAsset(const ReleaseAsset& asset, const QString& tag);

}  // namespace akyuu
