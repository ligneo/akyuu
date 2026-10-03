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

#include <QCoreApplication>
#include <QDir>
#include <QTextStream>
#include <exception>

#include "common.hpp"
#include "linux.hpp"
#include "sources.hpp"
#include "windows.hpp"

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  try {
    auto args = app.arguments();
    args.removeFirst();
    QString root = QDir::currentPath();
    if (args.size() >= 2 && args[0] == "--root") {
      root = QDir(args[1]).absolutePath();
      args.removeFirst();
      args.removeFirst();
    }
    const auto usage = QString(
        "usage: akyuu-release [--root REPOSITORY] COMMAND [ARGUMENTS]\n"
        "Commands: version, source-bundle, collect-sources, verify-sources, source-tests,\n"
        "common-tests, prepare-linux-sdk, build-linux, package-appimage, package-linux, "
        "package-arch,\n"
        "test-linux, prepare-windows-licenses, package-windows, test-windows,\n"
        "bump-version, create-release-draft\n");
    if (args.isEmpty() || args[0] == "--help") {
      QTextStream(stdout) << usage;
      return args.isEmpty() ? 1 : 0;
    }
    const auto command = args.takeFirst();
    const auto arity = [&](int count) { release::require(args.size() == count, usage); };
    using namespace release;
    if (command == "version") {
      arity(0);
      QTextStream(stdout) << version(root) << '\n';
    } else if (command == "source-bundle") {
      arity(1);
      sourceBundle(root, args[0]);
    } else if (command == "collect-sources") {
      arity(2);
      collectSources(args[0], args[1]);
    } else if (command == "verify-sources") {
      arity(2);
      verifySources(args[0], args[1]);
    } else if (command == "source-tests") {
      arity(0);
      return sourceTests();
    } else if (command == "common-tests") {
      arity(0);
      return commonTests();
    } else if (command == "prepare-linux-sdk") {
      arity(1);
      prepareLinuxSdk(args[0]);
    } else if (command == "build-linux") {
      arity(1);
      buildLinux(root, args[0]);
    } else if (command == "package-appimage") {
      arity(4);
      packageAppImage(root, args[0], args[1], args[2], args[3]);
    } else if (command == "package-linux") {
      arity(4);
      packageLinux(root, args[0], args[1], args[2], args[3]);
    } else if (command == "package-arch") {
      arity(2);
      packageArch(root, args[0], args[1]);
    } else if (command == "test-linux") {
      arity(2);
      testLinux(args[0], args[1]);
    } else if (command == "prepare-windows-licenses") {
      arity(1);
      prepareWindowsLicenses(args[0]);
    } else if (command == "package-windows") {
      arity(4);
      packageWindows(root, args[0], args[1], args[2], args[3]);
    } else if (command == "test-windows") {
      arity(2);
      testWindows(args[0], args[1]);
    } else if (command == "bump-version") {
      arity(1);
      bumpVersion(root, args[0]);
    } else if (command == "create-release-draft") {
      arity(2);
      createReleaseDraft(root, args[0], args[1]);
    } else
      require(false, "Unknown command: " + command + '\n' + usage);
    return 0;
  } catch (const std::exception& error) {
    QTextStream(stderr) << "akyuu-release: " << error.what() << '\n';
    return 1;
  }
}
