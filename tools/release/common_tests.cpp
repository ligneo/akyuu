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

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <functional>
#include <stdexcept>

#include "archive.hpp"
#include "common.hpp"

namespace release {
int commonTests() {
  QTemporaryDir temporary;
  require(temporary.isValid(), "Cannot create regression directory.");
  const auto root = temporary.path() + "/repository with spaces";
  require(QDir().mkpath(root), "Cannot create fixture repository.");
  const auto git = [&](const QStringList& args) {
    return run("git", QStringList{"-C", root} + args);
  };
  git({"init", "--quiet"});
  git({"config", "user.name", "Release test"});
  git({"config", "user.email", "release-test@example.invalid"});
  writeFile(root + "/src/akyuu/config.h",
            "#define AKYUU_VERSION_MAJOR 0\n#define AKYUU_VERSION_MINOR 1\n#define "
            "AKYUU_VERSION_PATCH 0\n#define AKYUU_VERSION_PRE \"beta.5\"\n");
  writeFile(root + "/payload/space file.txt", "Source fixture\n");
  git({"add", "."});
  git({"commit", "--quiet", "-m", "Add source fixture"});
  int checks = 0;
  const auto rejects = [&](const std::function<void()>& action) {
    bool failed = false;
    try {
      action();
    } catch (const std::runtime_error&) {
      failed = true;
    }
    require(failed, "Expected regression rejection.");
    ++checks;
  };
  require(version(root) == "0.1.0-beta.5", "Incorrect fixture version.");
  ++checks;
  rejects([&] { bumpVersion(root, "0.1.0-beta.01"); });
  rejects([&] { bumpVersion(root, "01.1.0"); });
  rejects([&] { validateVersion("0.1.0-x/../../escape"); });
  rejects([&] { validateVersion("0.1.0-beta.01"); });
  rejects([&] { run("git", {"--invalid-release-test-flag"}); });
  writeFile(root + "/untracked", "Dirty fixture\n");
  rejects([&] { sourceBundle(root, temporary.path() + "/out"); });
  QFile::remove(root + "/untracked");
  bumpVersion(root, "0.1.0-beta.6");
  require(version(root) == "0.1.0-beta.6" && git({"tag", "--list"}).trimmed() == "v0.1.0-beta.6",
          "Version commit/tag regression failed.");
  ++checks;
  rejects([&] { bumpVersion(root, "0.1.0-beta.6"); });
#ifdef Q_OS_UNIX
  const auto original = temporary.path() + "/symlink-source";
  const auto copied = temporary.path() + "/symlink-destination";
  writeFile(original + "/payload", "Link fixture\n");
  require(QFile::link("payload", original + "/link"), "Cannot create relative symlink fixture.");
  copyTree(original, copied);
  QDir(original).removeRecursively();
  require(readFile(copied + "/link") == "Link fixture\n",
          "Copied link retained its old directory.");
  ++checks;
  const auto output = temporary.path() + "/output with spaces";
  sourceBundle(root, output);
  const auto archive = output + "/akyuu-0.1.0-beta.6-source.tar.xz";
  const auto first = sha256(archive);
  sourceBundle(root, output);
  require(first == sha256(archive), "Repeated source archives are not deterministic.");
  ++checks;
  call("xz", {"--decompress", "--keep", archive});
  TarArchive exported(archive.left(archive.size() - 3));
  require(exported.read("akyuu-0.1.0-beta.6/payload/space file.txt") == "Source fixture\n",
          "Source export changed committed file content.");
  ++checks;
#endif
  QTextStream(stdout) << "Passed " << checks << " release utility regressions.\n";
  return 0;
}
}  // namespace release
