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

#include "update_package.hpp"

#include <QRegularExpression>
#include <QSysInfo>

namespace akyuu {

UpdateTarget updateTarget() {
  UpdateTarget target;
  const auto kernel = QSysInfo::kernelType();
  if (kernel == u"linux") target.platform = UpdatePlatform::Linux;
  if (kernel == u"winnt") target.platform = UpdatePlatform::Windows;
  if (kernel == u"darwin") target.platform = UpdatePlatform::MacOS;
  target.architecture = QSysInfo::currentCpuArchitecture();
  if (target.architecture == u"aarch64") target.architecture = QStringLiteral("arm64");

  if (target.platform == UpdatePlatform::Windows) target.format = PackageFormat::WindowsInstaller;
  if (target.platform == UpdatePlatform::MacOS) target.format = PackageFormat::MacDiskImage;
  if (target.platform == UpdatePlatform::Linux &&
      QString::fromLatin1(AKYUU_PACKAGE_FORMAT) == u"arch" &&
      qEnvironmentVariableIsEmpty("APPIMAGE")) {
    target.format = PackageFormat::Arch;
  }
  return target;
}

bool validReleaseAsset(const ReleaseAsset& asset, const QString& tag) {
  const auto& url = asset.url;
  return asset.size > 0 && asset.size <= 512LL * 1024 * 1024 && asset.sha256.size() == 32 &&
         !asset.name.isEmpty() && !asset.name.contains(u'/') && !asset.name.contains(u'\\') &&
         url.isValid() && url.scheme() == u"https" && url.host() == u"github.com" &&
         url.userInfo().isEmpty() && url.port(-1) == -1 && url.query().isEmpty() &&
         url.fragment().isEmpty() &&
         url.path(QUrl::FullyDecoded) ==
             QStringLiteral("/ligneo/akyuu/releases/download/") + tag + u'/' + asset.name;
}

std::optional<ReleaseAsset> selectPackage(const std::vector<ReleaseAsset>& assets,
                                          const UpdateTarget& target) {
  if (target.platform == UpdatePlatform::Unknown || target.architecture.isEmpty()) return {};
  std::optional<ReleaseAsset> selected;
  for (const auto& asset : assets) {
    if (asset.target.platform != target.platform ||
        asset.target.architecture != target.architecture || asset.target.format != target.format)
      continue;
    if (selected) return {};  // A release must have one package for each target.
    selected = asset;
  }
  return selected;
}

}  // namespace akyuu
