/*
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "linux.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QTextStream>

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

#include "common.hpp"
#include "sources.hpp"

namespace release {
namespace {

class Environment final {
public:
  ~Environment() {
    for (auto it = values_.cbegin(); it != values_.cend(); ++it) {
      if (it.value().isNull()) {
        qunsetenv(it.key().constData());
      } else {
        qputenv(it.key().constData(), it.value());
      }
    }
  }

  void set(const QByteArray& name, const QString& value) {
    remember(name);
    qputenv(name.constData(), value.toUtf8());
  }

  void unset(const QByteArray& name) {
    remember(name);
    qunsetenv(name.constData());
  }

private:
  void remember(const QByteArray& name) {
    if (!values_.contains(name)) values_.insert(name, qgetenv(name.constData()));
  }
  QMap<QByteArray, QByteArray> values_;
};

class Child final {
public:
  ~Child() {
    if (process.state() != QProcess::NotRunning) {
      process.terminate();
      if (!process.waitForFinished(3000)) {
        process.kill();
        process.waitForFinished(3000);
      }
    }
  }
  QProcess process;
};

QString absolute(const QString& path) {
  return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

QString existing(const QString& path) {
  const auto resolved = QFileInfo(path).canonicalFilePath();
  require(!resolved.isEmpty(), "Path does not exist: " + path);
  return resolved;
}

void makeDirectory(const QString& path) {
  require(QDir().mkpath(path), "Cannot create directory: " + path);
}

void copyFile(const QString& source, const QString& destination) {
  makeDirectory(QFileInfo(destination).absolutePath());
  if (QFileInfo::exists(destination)) {
    require(QFile::remove(destination), "Cannot replace file: " + destination);
  }
  require(QFile::copy(source, destination), "Cannot copy file: " + source);
}

QString requiredEnvironment(const char* name) {
  const auto value = qEnvironmentVariable(name);
  require(!value.isEmpty(), "Set " + QString::fromLatin1(name));
  return value;
}

QString jobs() {
  const auto value = qEnvironmentVariable("BUILD_JOBS", "2");
  require(QRegularExpression("^[1-9][0-9]*$").match(value).hasMatch(),
          "BUILD_JOBS must be a positive integer");
  return value;
}

void requireLinux() {
#ifndef Q_OS_LINUX
  require(false, "This command requires Linux");
#endif
}

QString cacheValue(const QString& build, const QString& key) {
  const auto text = QString::fromUtf8(readFile(build + "/CMakeCache.txt"));
  const auto match = QRegularExpression("^" + QRegularExpression::escape(key) + ":[^=]*=(.*)$",
                                        QRegularExpression::MultilineOption)
                         .match(text);
  require(match.hasMatch(), "Missing CMake cache property: " + key);
  return match.captured(1).trimmed();
}

void makeReadable(const QString& root) {
  QStringList paths{root};
  QDirIterator iterator(root, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
                        QDirIterator::Subdirectories);
  while (iterator.hasNext()) paths << iterator.next();
  for (const auto& path : paths) {
    const QFileInfo info(path);
    if (info.isSymLink()) continue;
    auto permissions = info.permissions() | QFile::ReadOwner | QFile::ReadGroup | QFile::ReadOther;
    if (info.isDir() || (permissions & (QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther))) {
      permissions |= QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther;
    }
    require(QFile::setPermissions(path, permissions), "Cannot set file permissions: " + path);
  }
}

void makeExecutable(const QString& path) {
  require(QFile::setPermissions(path, QFileInfo(path).permissions() | QFile::ExeOwner |
                                          QFile::ExeGroup | QFile::ExeOther),
          "Cannot set executable permission: " + path);
}

QString onePackage(const QString& directory, const QString& format) {
  const auto files = QDir(directory).entryList({"*." + format}, QDir::Files, QDir::Name);
  require(files.size() == 1, "Expected one " + format + " package in " + directory);
  return directory + "/" + files.first();
}

void print(const QString& value) {
  QTextStream(stdout) << value << Qt::endl;
}

void installPackage(const QString& format, const QString& path) {
  if (format == "deb") {
    call("apt-get", {"install", "-y", "--reinstall", path}, {}, 600000);
  } else if (!QStandardPaths::findExecutable("dnf").isEmpty()) {
    call("dnf", {"install", "-y", path}, {}, 600000);
  } else {
    call("zypper", {"--non-interactive", "--no-gpg-checks", "install", path}, {}, 600000);
  }
}

void fetch(const QString& sdk, const QString& url, const QString& name, const QByteArray& hash) {
  const auto path = sdk + "/cache/" + name;
  if (!QFileInfo::exists(path)) {
    call("curl", {"--fail", "--location", "--retry", "3", url, "-o", path}, {}, 900000);
  }
  require(sha256(path) == hash, "Download checksum mismatch: " + name);
}

}  // namespace

void prepareLinuxSdk(const QString& sdkPath) {
  requireLinux();
  const auto sdk = absolute(sdkPath);
  for (const auto& directory : {"cache", "gcc", "tools"}) makeDirectory(sdk + "/" + directory);
  const QList<QPair<QByteArray, QString>> packages{
      {"1d79371ccf3138e3c172dd83b51560687b65ab824c1ad7e9cb41b7857ed9c55f",
       "gcc-16.2.1+r23+gd564253eb6c8-1-x86_64.pkg.tar.zst"},
      {"7367dad49fc3229bde804816d412ab77306d433b1f92e4126b8b3ba3502c67d6",
       "libgcc-16.2.1+r23+gd564253eb6c8-1-x86_64.pkg.tar.zst"},
      {"15dc6bd2f3a2ee17fcd79a14325e9e0037722a592b2bdc55e1665d92112eaa51",
       "libstdc++-16.2.1+r23+gd564253eb6c8-1-x86_64.pkg.tar.zst"},
      {"dbeca7c4844e3112de98ba95f7c2b2618f4dd2b9c693c34076b7ba51ddf4611e",
       "gcc-libs-16.2.1+r23+gd564253eb6c8-1-x86_64.pkg.tar.zst"},
      {"d9508ad848fd6c31473ac0925cf8c3ec0d2a71bbbb5c7e39274186338a285660",
       "binutils-2.47-4-x86_64.pkg.tar.zst"}};
  for (const auto& [hash, name] : packages) {
    const auto package = name.left(name.indexOf(QRegularExpression("-[0-9]")));
    fetch(sdk,
          "https://archive.archlinux.org/packages/" + package.left(1) + "/" + package + "/" + name,
          name, hash);
    call("tar", {"-xf", sdk + "/cache/" + name, "-C", sdk + "/gcc", "--exclude=.*"});
  }
  fetch(sdk,
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/"
        "linuxdeploy-x86_64.AppImage",
        "linuxdeploy-x86_64.AppImage",
        "8aea8da0f7f7039d2a2cecb14657d752a222a5e1d3825caeef186c82f751cdd1");
  fetch(sdk,
        "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/"
        "linuxdeploy-plugin-qt-x86_64.AppImage",
        "linuxdeploy-plugin-qt-x86_64.AppImage",
        "cfc1055b2b9dbc08412b579f20990b7b41a17b61beaa5847dc9477c96c9e9617");
  fetch(sdk,
        "https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64",
        "runtime-x86_64", "156f4bdbde9c52d01814600013e0a273f0118dc2de98975f3c8c63427ec79074");
  for (const auto& name :
       {"linuxdeploy-x86_64.AppImage", "linuxdeploy-plugin-qt-x86_64.AppImage", "runtime-x86_64"}) {
    copyFile(sdk + "/cache/" + name, sdk + "/tools/" + name);
    makeExecutable(sdk + "/tools/" + name);
  }
  call("python3", {"-m", "venv", sdk + "/aqt"});
  call(sdk + "/aqt/bin/pip", {"install", "aqtinstall==3.3.0"}, {}, 600000);
  call(sdk + "/aqt/bin/aqt",
       {"install-qt", "linux", "desktop", "6.11.2", "linux_gcc_64", "--outputdir", sdk + "/Qt",
        "-m", "qtmultimedia", "qtimageformats"},
       {}, 1800000);
  fetch(sdk,
        "https://github.com/ligneo/akyuu/releases/download/v0.1.0-beta.3/"
        "akyuu-0.1.0-beta.3-x86_64.AppImage",
        "baseline.AppImage", "a2aedc6f9e8dfca886176fd202445044211db7bf73b68d801a3bf3323917b027");
  fetch(sdk,
        "https://github.com/ligneo/akyuu/releases/download/v0.1.0-beta.3/"
        "akyuu-0.1.0-beta.3-third-party-sources.tar",
        "baseline-sources.tar", "f8f33db2e8dd807cc094b9820a34acc21ac8c88c14987a98e6318f21e22a7d95");
  makeDirectory(sdk + "/baseline");
  makeDirectory(sdk + "/sources");
  makeExecutable(sdk + "/cache/baseline.AppImage");
  call(sdk + "/cache/baseline.AppImage", {"--appimage-extract"}, sdk + "/baseline");
  copyTree(sdk + "/baseline/squashfs-root/usr/share/doc/akyuu", sdk + "/licenses");
  call("tar", {"-xf", sdk + "/cache/baseline-sources.tar", "-C", sdk + "/sources"});
  makeDirectory(sdk + "/templates");
  copyFile(sdk + "/licenses/NOTICE", sdk + "/templates/NOTICE");
  copyFile(sdk + "/sources/third-party-sources/README", sdk + "/templates/third-party-README");
}

void buildLinux(const QString& rootPath, const QString& outputPath) {
  requireLinux();
  const auto root = existing(rootPath);
  const auto output = absolute(outputPath);
  const auto sdk = existing(requiredEnvironment("SDK_DIR"));
  makeDirectory(output);
  const auto releaseVersion = version(root);
  require(sourceTests() == 0, "Source companion regression tests failed");
  sourceBundle(root, output);
  for (const auto& [name, path] : QList<QPair<QString, QString>>{
           {"NOTICE", sdk + "/licenses/NOTICE"},
           {"third-party-README", sdk + "/sources/third-party-sources/README"}}) {
    auto text = readFile(sdk + "/templates/" + name);
    text.replace("0.1.0-beta.3", releaseVersion.toUtf8());
    writeFile(path, text);
  }
  Environment environment;
  const auto qt = sdk + "/Qt/6.11.2/gcc_64";
  const auto gcc = sdk + "/gcc/usr";
  environment.set("LINUXDEPLOY", sdk + "/tools/linuxdeploy-x86_64.AppImage");
  environment.set("LINUXDEPLOY_QT", sdk + "/tools/linuxdeploy-plugin-qt-x86_64.AppImage");
  environment.set("LDAI_RUNTIME_FILE", sdk + "/tools/runtime-x86_64");
  environment.set("QMAKE", qt + "/bin/qmake");
  environment.set("GCC_RUNTIME_DIR", gcc + "/lib");
  environment.set("APPIMAGE_EXTRACT_AND_RUN", "1");
  environment.set("CPLUS_INCLUDE_PATH", "/usr/include/x86_64-linux-gnu");
  environment.set("C_INCLUDE_PATH", "/usr/include/x86_64-linux-gnu");
  environment.set("LIBRARY_PATH", "/usr/lib/x86_64-linux-gnu");
  environment.set("LD_LIBRARY_PATH", gcc + "/lib:" + qt + "/lib");
  environment.set("PATH", gcc + "/bin:" + qEnvironmentVariable("PATH"));
  QTemporaryDir temporary;
  const auto requestedWork = qEnvironmentVariable("AKYUU_PACKAGE_WORK");
  require(!requestedWork.isEmpty() || temporary.isValid(), "Cannot create package work directory");
  const auto work = requestedWork.isEmpty() ? temporary.path() : absolute(requestedWork);
  makeDirectory(work);
  for (const auto& format : {QString("appimage"), QString("deb"), QString("rpm")}) {
    const auto build = work + "/build";
    call("cmake",
         {"-S", root, "-B", build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
          "-DCMAKE_CXX_COMPILER=" + gcc + "/bin/g++", "-DCMAKE_C_COMPILER=" + gcc + "/bin/gcc",
          "-DCMAKE_PREFIX_PATH=" + qt, "-DCMAKE_C_FLAGS=-B/usr/lib/x86_64-linux-gnu/",
          "-DCMAKE_CXX_FLAGS=-B/usr/lib/x86_64-linux-gnu/",
          "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=" + build + "/bin", "-DCMAKE_INSTALL_PREFIX=/usr",
          "-DAKYUU_PORTABLE=OFF", "-DAKYUU_PACKAGE_FORMAT=" + format, "-DAKYUU_BUILD_TESTS=ON"});
    call("cmake", {"--build", build, "--parallel", jobs()}, {}, 1800000);
    call("ctest", {"--test-dir", build, "--output-on-failure"}, {}, 600000);
    const auto appdir = work + "/" + format + ".AppDir";
    packageAppImage(root, build, appdir, sdk + "/licenses", output);
    if (format != "appimage") {
      packageLinux(root, format.toUpper(), appdir, releaseVersion, output);
      packageLinux(root, format.toUpper(), sdk + "/baseline/squashfs-root", "0.0.1",
                   output + "/fixtures");
    }
  }
  const auto companion = output + "/akyuu-" + releaseVersion + "-third-party-sources.tar";
  call("tar",
       {"-cf", companion, "-C", sdk, "licenses", "-C", sdk + "/sources", "third-party-sources"}, {},
       600000);
  verifySources(companion, releaseVersion);
  // A separate diagnostic bundle runs the same C++ installation checker on fresh
  // distribution containers without asking them to install a Qt development SDK.
  const auto toolDir = work + "/release-tool.AppDir";
  const auto desktop = work + "/release-tool.desktop";
  writeFile(desktop,
            "[Desktop Entry]\nType=Application\nName=Akyuu release "
            "tooling\nExec=akyuu-release\nIcon=io.github.ligneo.Akyuu\nCategories=Development;\n");
  environment.set("LDAI_OUTPUT", output + "/diagnostics/akyuu-release.AppImage");
  environment.set("LDAI_VERSION", releaseVersion);
  call(sdk + "/tools/linuxdeploy-x86_64.AppImage",
       {"--appdir", toolDir, "--executable", QCoreApplication::applicationFilePath(), "--library",
        gcc + "/lib/libstdc++.so.6", "--library", gcc + "/lib/libgcc_s.so.1", "--desktop-file",
        desktop, "--icon-file", root + "/src/resources/icons/akyuu.png", "--icon-filename",
        "io.github.ligneo.Akyuu", "--output", "appimage"},
       {}, 600000);
  require(QFileInfo(output + "/diagnostics/akyuu-release.AppImage").size() > 0,
          "Release diagnostic tool was not produced");
}

void packageAppImage(const QString& rootPath, const QString& buildPath, const QString& appdirPath,
                     const QString& licensesPath, const QString& outputPath) {
  requireLinux();
  const auto root = existing(rootPath);
  const auto build = existing(buildPath);
  const auto appdir = absolute(appdirPath);
  const auto licenses = existing(licensesPath);
  const auto output = absolute(outputPath);
  const auto deploy = existing(requiredEnvironment("LINUXDEPLOY"));
  const auto plugin = existing(requiredEnvironment("LINUXDEPLOY_QT"));
  const auto qmake = existing(requiredEnvironment("QMAKE"));
  const auto gccRuntime = existing(requiredEnvironment("GCC_RUNTIME_DIR"));
  const auto runtime = existing(requiredEnvironment("LDAI_RUNTIME_FILE"));
  require(!QFileInfo::exists(appdir) && !QFileInfo(appdir).isSymLink(), "APPDIR must not exist");
  const auto format = cacheValue(build, "AKYUU_PACKAGE_FORMAT");
  require(QStringList{"appimage", "deb", "rpm"}.contains(format),
          "Unsupported Linux package format");
  require(cacheValue(build, "AKYUU_PORTABLE") == "OFF", "Expected a nonportable application build");
  require(cacheValue(build, "CMAKE_INSTALL_PREFIX") == "/usr", "Expected /usr installation prefix");
  require(QSysInfo::currentCpuArchitecture() == "x86_64",
          "The prepared Linux SDK supports x86_64 only");
  const auto releaseVersion = version(root);
  Environment environment;
  environment.set("DESTDIR", appdir);
  call("cmake", {"--build", build, "--parallel", jobs()}, {}, 1800000);
  call("cmake", {"--install", build});
  environment.unset("DESTDIR");
  makeDirectory(appdir + "/usr/share/doc/akyuu");
  makeDirectory(output);
  copyFile(root + "/LICENSE", appdir + "/usr/share/doc/akyuu/LICENSE");
  copyTree(licenses, appdir + "/usr/share/doc/akyuu");
  const auto deployment = build + "/tests/akyuu-deployment-tests";
  if (QFileInfo::exists(deployment))
    copyFile(deployment, appdir + "/usr/bin/akyuu-deployment-tests");
  environment.set("QMAKE", qmake);
  environment.set("LDAI_RUNTIME_FILE", runtime);
  environment.set("APPIMAGE_EXTRACT_AND_RUN", "1");
  environment.set("EXTRA_QT_MODULES", "svg");
  environment.set("EXTRA_PLATFORM_PLUGINS", "libqwayland.so;libqoffscreen.so");
  const auto inheritedLibraries = qEnvironmentVariable("LD_LIBRARY_PATH");
  environment.set("LD_LIBRARY_PATH",
                  gccRuntime + ":" + QFileInfo(qmake).absolutePath() + "/../lib" +
                      (inheritedLibraries.isEmpty() ? QString() : ":" + inheritedLibraries));
  environment.set("LDAI_VERSION", releaseVersion);
  const auto artifact = output + "/akyuu-" + releaseVersion + "-x86_64.AppImage";
  environment.set("LDAI_OUTPUT", artifact);
  call(deploy,
       {"--appdir", appdir, "--library", gccRuntime + "/libstdc++.so.6", "--library",
        gccRuntime + "/libgcc_s.so.1", "--desktop-file",
        root + "/src/resources/io.github.ligneo.Akyuu.desktop", "--icon-file",
        root + "/src/resources/icons/akyuu.png", "--icon-filename", "io.github.ligneo.Akyuu"},
       {}, 600000);
  const auto sqlPlugins =
      QString::fromUtf8(run(qmake, {"-query", "QT_INSTALL_PLUGINS"})).trimmed() + "/sqldrivers";
  QStringList qtArguments{"--appdir", appdir};
  for (const auto& driver : QDir(sqlPlugins).entryList({"libqsql*.so"}, QDir::Files, QDir::Name)) {
    if (driver != "libqsqlite.so") qtArguments << "--exclude-library" << driver;
  }
  call(plugin, qtArguments, {}, 600000);
  const auto libraries = QString::fromUtf8(run("ldconfig", {"-p"}));
  for (const auto& name :
       {"libOpenGL.so.0", "libGLdispatch.so.0", "libfontconfig.so.1", "libfreetype.so.6",
        "libexpat.so.1", "libz.so.1", "libcom_err.so.2", "libgpg-error.so.0"}) {
    const auto match = QRegularExpression("^\\s*" + QRegularExpression::escape(name) +
                                              "\\s+[^\\n]*=>\\s+(\\S+)\\s*$",
                                          QRegularExpression::MultilineOption)
                           .match(libraries);
    require(match.hasMatch(), "Missing desktop runtime library: " + QString::fromLatin1(name));
    const auto library = appdir + "/usr/lib/" + name;
    copyFile(existing(match.captured(1)), library);
    call("patchelf", {"--set-rpath", "$ORIGIN", library});
  }
  const auto sdk = qEnvironmentVariable("SDK_DIR");
  if (!sdk.isEmpty()) {
    collectSources(appdir, sdk);
    copyTree(licenses, appdir + "/usr/share/doc/akyuu");
  }
  const auto bundledTest = appdir + "/usr/bin/akyuu-deployment-tests";
  if (QFileInfo::exists(bundledTest)) {
    copyFile(bundledTest, output + "/diagnostics/deployment-" + format);
    require(QFile::remove(bundledTest), "Cannot remove bundled deployment test");
  }
  if (format == "appimage") {
    call(deploy, {"--appdir", appdir, "--output", "appimage"}, {}, 600000);
    require(QFileInfo(artifact).size() > 0, "AppImage output is missing");
    print(artifact);
  } else {
    print(appdir);
  }
}

void packageLinux(const QString& rootPath, const QString& format, const QString& bundlePath,
                  const QString& releaseVersion, const QString& outputPath) {
  requireLinux();
  require(format == "DEB" || format == "RPM", "Expected DEB or RPM");
  const auto root = existing(rootPath);
  const auto bundle = existing(bundlePath);
  const auto output = absolute(outputPath);
  QTemporaryDir work;
  require(work.isValid(), "Cannot create native package staging directory");
  copyTree(bundle, work.path() + "/bundle");
  makeReadable(work.path() + "/bundle");
  call("cmake",
       {"-S", root + "/setup/linux", "-B", work.path(), "-G", "Ninja", "-DAKYUU_FORMAT=" + format,
        "-DAKYUU_BUNDLE=" + work.path() + "/bundle", "-DAKYUU_VERSION=" + releaseVersion,
        "-DAKYUU_OUTPUT=" + work.path() + "/packages"});
  call("cmake", {"--build", work.path(), "--parallel", jobs()});
  call("cpack", {"--config", "CPackConfig.cmake"}, work.path(), 600000);
  const auto name = "akyuu-" + releaseVersion + "-x86_64." + format.toLower();
  copyFile(work.path() + "/packages/" + name, output + "/" + name);
}

void packageArch(const QString& rootPath, const QString& archivePath, const QString& outputPath) {
  requireLinux();
#ifdef Q_OS_LINUX
  require(geteuid() != 0, "Arch packages must be built as an ordinary user");
#endif
  const auto root = existing(rootPath);
  const auto archive = existing(archivePath);
  const auto output = absolute(outputPath);
  const auto releaseVersion = version(root);
  const auto name = "akyuu-" + releaseVersion + "-source.tar.xz";
  require(QFileInfo(archive).fileName() == name, "Source archive name does not match the version");
  QTemporaryDir work;
  require(work.isValid(), "Cannot create Arch package staging directory");
  copyFile(archive, work.path() + "/" + name);
  auto pkgver = releaseVersion;
  pkgver.remove('-');
  const auto recipe = QString(R"(pkgname=akyuu
pkgver=%1
pkgrel=1
pkgdesc='Anime list tracker with player and browser detection'
arch=('x86_64')
url='https://github.com/ligneo/akyuu'
license=('GPL-3.0-or-later')
depends=('qt6-base' 'qt6-multimedia' 'qt6-svg' 'systemd-libs' 'hicolor-icon-theme')
makedepends=('cmake' 'ninja' 'gcc' 'qt6-tools')
source=('https://github.com/ligneo/akyuu/releases/download/v%2/%3')
sha256sums=('%4')
build() {
  cmake -S "akyuu-%2" -B build -G Ninja -DCMAKE_BUILD_TYPE=None \
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$srcdir/build/bin" -DCMAKE_INSTALL_PREFIX=/usr \
    -DAKYUU_PORTABLE=OFF -DAKYUU_PACKAGE_FORMAT=arch -DAKYUU_BUILD_TESTS=ON
  cmake --build build --parallel "${BUILD_JOBS:-2}"
}
check() { ctest --test-dir build --output-on-failure; }
package() {
  DESTDIR="$pkgdir" cmake --install build
  install -Dm644 "akyuu-%2/LICENSE" "$pkgdir/usr/share/licenses/akyuu/LICENSE"
}
)")
                          .arg(pkgver, releaseVersion, name, QString::fromLatin1(sha256(archive)));
  writeFile(work.path() + "/PKGBUILD", recipe.toUtf8());
  call("makepkg", {"--cleanbuild", "--force", "--noconfirm"}, work.path(), 1800000);
  const auto packages = QDir(work.path()).entryList({"*.pkg.tar.zst"}, QDir::Files, QDir::Name);
  require(!packages.isEmpty(), "Arch package output is missing");
  for (const auto& package : packages)
    copyFile(work.path() + "/" + package, output + "/" + package);
  copyFile(work.path() + "/PKGBUILD", output + "/PKGBUILD");
}

void testLinux(const QString& format, const QString& packagesPath) {
  requireLinux();
  require(format == "deb" || format == "rpm", "Expected deb or rpm");
  require(qEnvironmentVariable("GITHUB_ACTIONS") == "true" && QFileInfo::exists("/.dockerenv"),
          "Package installation checks require a disposable CI container");
#ifdef Q_OS_LINUX
  require(geteuid() == 0, "Package installation checks require the container's root user");
#endif
  const auto packages = existing(packagesPath);
  const auto current = onePackage(packages, format);
  const auto previous = onePackage(packages + "/fixtures", format);
  Environment environment;
  environment.set("DEBIAN_FRONTEND", "noninteractive");
  installPackage(format, previous);
  QTemporaryDir work;
  require(work.isValid(), "Cannot create package test directory");
  environment.set("XDG_CONFIG_HOME", work.path() + "/config");
  environment.set("XDG_DATA_HOME", work.path() + "/data");
  environment.set("TMPDIR", work.path() + "/tmp");
  makeDirectory(work.path() + "/tmp");
  const auto marker = work.path() + "/data/akyuu/data/packaging-test-marker";
  makeDirectory(QFileInfo(marker).absolutePath());
  writeFile(marker, "Preserve user data\n");
  const auto expected = sha256(marker);
  call("chown", {"-R", "nobody", work.path()});
  installPackage(format, current);
  const auto probe = "/opt/akyuu/bin/packaging-probe";
  require(!QFileInfo::exists(probe), "The package test probe path is already occupied");
  copyFile(packages + "/diagnostics/deployment-" + format, probe);
  makeExecutable(probe);
  for (const auto& variable :
       {"LD_LIBRARY_PATH", "QT_PLUGIN_PATH", "QML_IMPORT_PATH", "QML2_IMPORT_PATH"}) {
    environment.unset(variable);
  }
  call("runuser", {"-u", "nobody", "--", "env", "QT_QPA_PLATFORM=offscreen", probe, "--network"},
       {}, 180000);
  require(QFile::remove(probe), "Cannot remove package test probe");
  Child display;
  display.process.setProgram("Xvfb");
  display.process.setArguments(
      {"-displayfd", "1", "-screen", "0", "1280x720x24", "-nolisten", "tcp"});
  display.process.setStandardErrorFile(work.path() + "/xvfb.log");
  display.process.start();
  require(display.process.waitForStarted(10000), "Cannot start the package test display");
  QElapsedTimer timer;
  timer.start();
  QByteArray number;
  while (!number.contains('\n') && timer.elapsed() < 10000) {
    display.process.waitForReadyRead(100);
    number += display.process.readAllStandardOutput();
    require(display.process.state() != QProcess::NotRunning,
            "The package test display exited: " +
                QString::fromUtf8(readFile(work.path() + "/xvfb.log")));
  }
  const auto displayNumber = QString::fromLatin1(number).trimmed();
  require(QRegularExpression("^[0-9]+$").match(displayNumber).hasMatch(),
          "The package test display did not become ready");
  environment.set("DISPLAY", ":" + displayNumber);
  Child application;
  application.process.setProcessChannelMode(QProcess::MergedChannels);
  application.process.start("runuser", {"-u", "nobody", "--", "timeout", "--kill-after=3s", "10s",
                                        "/usr/bin/akyuu", "--debug"});
  require(application.process.waitForStarted(10000), "Cannot start the installed application");
  require(application.process.waitForFinished(20000),
          "Installed application startup check timed out");
  require(
      application.process.exitStatus() == QProcess::NormalExit &&
          application.process.exitCode() == 124,
      "Installed application exited early: " + QString::fromUtf8(application.process.readAll()));
  if (format == "deb") {
    call("dpkg-query", {"-W", "-f=${Version}\n", "akyuu"});
    call("apt-get", {"remove", "-y", "akyuu"}, {}, 600000);
  } else {
    call("rpm", {"-q", "akyuu"});
    call("rpm", {"-e", "akyuu"});
  }
  require(!QFileInfo::exists("/usr/bin/akyuu") && !QFileInfo::exists("/opt/akyuu/bin/akyuu"),
          "Application files remained after uninstall");
  require(sha256(marker) == expected, "Uninstall changed the user's data");
  print(
      "Passed package install, fixture upgrade, startup, runtime and user-data preservation "
      "checks.");
}

}  // namespace release
