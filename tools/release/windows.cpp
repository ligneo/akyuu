/*
 * Akyuu, an anime tracker application for Windows and Linux.
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
#include "windows.hpp"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <shlobj.h>
#include <softpub.h>
#include <wincrypt.h>
#include <windows.h>
#include <wintrust.h>
#endif

#include "common.hpp"

using namespace Qt::StringLiterals;

namespace release {
namespace {

#ifdef Q_OS_WIN
QString absolute(const QString& path) {
  return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

QString native(const QString& path) {
  return QDir::toNativeSeparators(absolute(path));
}

void copyFile(const QString& source, const QString& destination) {
  require(QFile::copy(source, destination), u"Cannot copy %1 to %2"_s.arg(source, destination));
}

QStringList payloadEntries(const QString& root, bool directories) {
  QStringList files;
  const auto visit = [&](auto&& self, const QString& path) -> void {
    const auto entries = QDir(path).entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);
    for (const auto& entry : entries) {
      const auto wide = native(entry.absoluteFilePath()).toStdWString();
      const auto attributes = GetFileAttributesW(wide.c_str());
      require(attributes != INVALID_FILE_ATTRIBUTES, u"Cannot inspect %1"_s.arg(entry.filePath()));
      require(!(attributes & FILE_ATTRIBUTE_REPARSE_POINT),
              u"The Windows payload must not contain reparse points: %1"_s.arg(entry.filePath()));
      if (entry.isDir()) {
        if (directories) files.append(entry.absoluteFilePath());
        self(self, entry.absoluteFilePath());
      } else {
        require(entry.isFile(), u"Unsupported Windows payload entry: %1"_s.arg(entry.filePath()));
        if (!directories) files.append(entry.absoluteFilePath());
      }
    }
  };
  visit(visit, root);
  return files;
}

QString nsisLiteral(QString text) {
  text.replace(u"$"_s, u"$$"_s);
  text.replace(u"\""_s, u"$\\\""_s);
  return text;
}

void verifyMicrosoftSignature(const QString& path) {
  const auto wide = native(path).toStdWString();
  WINTRUST_FILE_INFO file{};
  file.cbStruct = sizeof(file);
  file.pcwszFilePath = wide.c_str();
  GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
  WINTRUST_DATA trust{};
  trust.cbStruct = sizeof(trust);
  trust.dwUIChoice = WTD_UI_NONE;
  trust.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
  trust.dwUnionChoice = WTD_CHOICE_FILE;
  trust.pFile = &file;
  trust.dwStateAction = WTD_STATEACTION_VERIFY;
  trust.dwProvFlags = WTD_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT | WTD_DISABLE_MD2_MD4;
  trust.dwUIContext = WTD_UICONTEXT_INSTALL;
  struct State final {
    GUID& action;
    WINTRUST_DATA& trust;
    ~State() {
      trust.dwStateAction = WTD_STATEACTION_CLOSE;
      WinVerifyTrust(INVALID_HANDLE_VALUE, &action, &trust);
    }
  } state{action, trust};
  require(WinVerifyTrust(INVALID_HANDLE_VALUE, &action, &trust) == ERROR_SUCCESS,
          u"The Visual C++ Redistributable must have a valid Authenticode signature."_s);
  const auto library = LoadLibraryExW(L"wintrust.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  require(library != nullptr, u"Cannot load the Windows trust provider."_s);
  struct Library final {
    HMODULE handle;
    ~Library() {
      FreeLibrary(handle);
    }
  } loaded{library};
  const auto getProvider = reinterpret_cast<decltype(&WTHelperProvDataFromStateData)>(
      GetProcAddress(library, "WTHelperProvDataFromStateData"));
  const auto getSigner = reinterpret_cast<decltype(&WTHelperGetProvSignerFromChain)>(
      GetProcAddress(library, "WTHelperGetProvSignerFromChain"));
  require(getProvider && getSigner, u"Cannot inspect the Windows trust provider."_s);
  auto* provider = getProvider(trust.hWVTStateData);
  require(provider != nullptr, u"Cannot inspect the verified Authenticode signature."_s);
  auto* signer = getSigner(provider, 0, FALSE, 0);
  require(signer && signer->csCertChain > 0 && signer->pasCertChain[0].pCert,
          u"The verified Authenticode signature has no signing certificate."_s);
  const auto certificate = signer->pasCertChain[0].pCert;
  const auto count = CertGetNameStringW(certificate, CERT_NAME_ATTR_TYPE, 0,
                                        const_cast<char*>(szOID_ORGANIZATION_NAME), nullptr, 0);
  require(count > 1 && count <= 4096, u"The signing certificate has no organization name."_s);
  std::vector<wchar_t> organization(count);
  require(CertGetNameStringW(certificate, CERT_NAME_ATTR_TYPE, 0,
                             const_cast<char*>(szOID_ORGANIZATION_NAME), organization.data(),
                             count) == count,
          u"Cannot inspect the signing certificate organization."_s);
  require(QString::fromWCharArray(organization.data()) == u"Microsoft Corporation"_s,
          u"The Visual C++ Redistributable must be signed by Microsoft Corporation."_s);
}

void installerProcess(const QString& executable, const QString& arguments,
                      const QProcessEnvironment& environment) {
  QProcess process;
  process.setProgram(absolute(executable));
  process.setNativeArguments(arguments);
  process.setProcessEnvironment(environment);
  process.setProcessChannelMode(QProcess::ForwardedChannels);
  process.start();
  require(process.waitForStarted(30000),
          u"Cannot start installation process: %1"_s.arg(process.errorString()));
  if (!process.waitForFinished(180000)) {
    process.kill();
    process.waitForFinished(5000);
    require(false,
            u"The installation process did not finish within three minutes: %1"_s.arg(executable));
  }
  require(process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0,
          u"Installation process failed: %1 (exit %2)"_s.arg(executable).arg(process.exitCode()));
}

void runtimeProbe(const QString& executable, const QProcessEnvironment& environment) {
  QProcess process;
  process.setProgram(executable);
  process.setArguments({u"--network"_s});
  process.setProcessEnvironment(environment);
  process.setProcessChannelMode(QProcess::ForwardedChannels);
  process.start();
  require(process.waitForStarted(30000),
          u"Cannot start the installed runtime probe: %1"_s.arg(process.errorString()));
  if (!process.waitForFinished(180000)) {
    process.kill();
    process.waitForFinished(5000);
    require(false, u"The installed runtime probe timed out."_s);
  }
  require(process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0,
          u"Installed runtime verification failed (exit %1)."_s.arg(process.exitCode()));
}

void installedStartup(const QString& executable, const QProcessEnvironment& environment) {
  QProcess process;
  process.setProgram(executable);
  process.setArguments({u"--debug"_s});
  process.setProcessEnvironment(environment);
  process.setProcessChannelMode(QProcess::ForwardedChannels);
  process.start();
  require(process.waitForStarted(30000),
          u"Cannot start the installed app: %1"_s.arg(process.errorString()));
  if (process.waitForFinished(8000)) {
    require(false, u"Installed app exited early (exit %1)."_s.arg(process.exitCode()));
  }
  require(process.state() == QProcess::Running, u"The installed app is no longer running."_s);
  process.kill();
  require(process.waitForFinished(10000), u"Cannot stop the installed app test process."_s);
}

class StartupKey final {
public:
  StartupKey() {
    require(RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                            0, nullptr, 0, KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY,
                            nullptr, &key_, nullptr) == ERROR_SUCCESS,
            u"Cannot open the user startup registry key."_s);
    try {
      require(!value().has_value(),
              u"The disposable runner already has an Akyuu startup command."_s);
    } catch (...) {
      RegCloseKey(key_);
      throw;
    }
  }
  ~StartupKey() {
    RegCloseKey(key_);
  }
  StartupKey(const StartupKey&) = delete;
  StartupKey& operator=(const StartupKey&) = delete;

  std::optional<QString> value() const {
    DWORD size = 0;
    DWORD type = 0;
    const auto status = RegQueryValueExW(key_, L"Akyuu", nullptr, &type, nullptr, &size);
    if (status == ERROR_FILE_NOT_FOUND) return std::nullopt;
    require(status == ERROR_SUCCESS && type == REG_SZ && size >= sizeof(wchar_t) && size <= 65536 &&
                size % sizeof(wchar_t) == 0,
            u"Cannot inspect the Akyuu startup registry value."_s);
    std::vector<wchar_t> data(size / sizeof(wchar_t));
    require(RegQueryValueExW(key_, L"Akyuu", nullptr, &type, reinterpret_cast<BYTE*>(data.data()),
                             &size) == ERROR_SUCCESS &&
                type == REG_SZ && data.back() == L'\0',
            u"Cannot read the Akyuu startup registry value."_s);
    return QString::fromWCharArray(data.data());
  }

  void set(const QString& command) {
    const auto data = command.toStdWString();
    require(data.size() < (std::numeric_limits<DWORD>::max() / sizeof(wchar_t)) - 1,
            u"The startup command is too long."_s);
    require(
        RegSetValueExW(key_, L"Akyuu", 0, REG_SZ, reinterpret_cast<const BYTE*>(data.c_str()),
                       static_cast<DWORD>((data.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS,
        u"Cannot set the startup registry fixture."_s);
  }

  void remove() {
    require(RegDeleteValueW(key_, L"Akyuu") == ERROR_SUCCESS,
            u"Cannot remove the startup registry fixture."_s);
  }

private:
  HKEY key_ = nullptr;
};

QString roamingDirectory() {
  PWSTR path = nullptr;
  const auto status = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path);
  struct Folder final {
    PWSTR path;
    ~Folder() {
      CoTaskMemFree(path);
    }
  } folder{path};
  require(SUCCEEDED(status) && path && *path,
          u"The runner has no roaming application data directory."_s);
  return QString::fromWCharArray(path);
}
#endif

}  // namespace

void prepareWindowsLicenses(const QString& output) {
  require(!QFileInfo::exists(output), u"The Windows license output directory must not exist."_s);
  QTemporaryDir temporary;
  require(temporary.isValid(), u"Cannot create the Windows license staging directory."_s);
  const auto archive = temporary.filePath(u"sources.tar"_s);
  call(u"curl"_s,
       {u"--fail"_s, u"--location"_s, u"--proto"_s, u"=https"_s, u"--proto-redir"_s, u"=https"_s,
        u"--output"_s, archive,
        u"https://github.com/ligneo/akyuu/releases/download/v0.1.0-beta.3/"
        u"akyuu-0.1.0-beta.3-third-party-sources.tar"_s},
       {}, 300000);
  require(sha256(archive) == "f8f33db2e8dd807cc094b9820a34acc21ac8c88c14987a98e6318f21e22a7d95",
          u"Source archive checksum mismatch."_s);
  call(u"tar"_s, {u"-xf"_s, archive, u"-C"_s, temporary.path(), u"licenses"_s});
  require(QFileInfo::exists(temporary.filePath(u"licenses/NOTICE"_s)),
          u"Missing license payload."_s);
  require(!QFileInfo::exists(output), u"The Windows license output directory must not exist."_s);
  copyTree(temporary.filePath(u"licenses"_s), output);
  writeFile(QDir(output).filePath(u"NOTICE"_s),
            "Akyuu uses Qt 6.11.2. Qt and its third-party sources and full licenses accompany the "
            "release. The Windows compiler runtime is the official Microsoft Visual C++ "
            "Redistributable, distributed under its included license.\n");
}

void packageWindows(const QString& root, const QString& build, const QString& qt,
                    const QString& licenses, const QString& output) {
#ifdef Q_OS_WIN
  const auto releaseVersion = version(root);
  const QRegularExpression expression(u"^([0-9]+\\.[0-9]+\\.[0-9]+)(?:-[0-9A-Za-z.-]+)?$"_s);
  const auto match = expression.match(releaseVersion);
  require(match.hasMatch(),
          u"The application version must be numeric with an optional prerelease suffix."_s);
  const auto stage = QDir(absolute(output)).filePath(u"windows-stage"_s);
  require(!QFileInfo::exists(stage), u"The Windows staging directory must not exist."_s);
  const auto cache = QString::fromUtf8(readFile(QDir(build).filePath(u"CMakeCache.txt"_s)));
  require(
      QRegularExpression(u"^AKYUU_PORTABLE:BOOL=OFF\\r?$"_s, QRegularExpression::MultilineOption)
          .match(cache)
          .hasMatch(),
      u"Installer builds must keep user data outside the application directory."_s);
  require(QDir().mkpath(stage), u"Cannot create the Windows staging directory."_s);
  copyFile(QDir(build).filePath(u"bin/Akyuu.exe"_s), QDir(stage).filePath(u"Akyuu.exe"_s));
  call(QDir(qt).filePath(u"bin/windeployqt.exe"_s),
       {u"--release"_s, u"--no-compiler-runtime"_s, u"--include-plugins"_s, u"qoffscreen"_s,
        u"--no-opengl-sw"_s, u"--no-system-d3d-compiler"_s, u"--no-system-dxc-compiler"_s,
        u"--dir"_s, stage, QDir(stage).filePath(u"Akyuu.exe"_s)});
  copyTree(licenses, QDir(stage).filePath(u"licenses"_s));
  copyFile(QDir(root).filePath(u"LICENSE"_s), QDir(stage).filePath(u"LICENSE"_s));
  const auto runtime = QDir(stage).filePath(u"vc_redist.x64.exe"_s);
  call(u"curl"_s,
       {u"--fail"_s, u"--location"_s, u"--proto"_s, u"=https"_s, u"--proto-redir"_s, u"=https"_s,
        u"--output"_s, runtime, u"https://aka.ms/vs/17/release/vc_redist.x64.exe"_s});
  verifyMicrosoftSignature(runtime);
  require(!QFileInfo::exists(QDir(stage).filePath(u"Uninstall.exe"_s)),
          u"Uninstall.exe is reserved for the generated uninstaller."_s);
  auto files = payloadEntries(stage, false);
  auto directories = payloadEntries(stage, true);
  std::sort(files.begin(), files.end());
  std::sort(directories.begin(), directories.end(), [](const QString& a, const QString& b) {
    return a.size() != b.size() ? a.size() > b.size() : a < b;
  });
  QString removal = u"; Generated from the deployed payload. Do not edit.\n"_s;
  const auto payloadPath = [&](const QString& path) {
    return nsisLiteral(QDir::toNativeSeparators(QDir(stage).relativeFilePath(path)));
  };
  for (const auto& file : files) removal += u"Delete \"$INSTDIR\\%1\"\n"_s.arg(payloadPath(file));
  for (const auto& directory : directories) {
    removal += u"RMDir \"$INSTDIR\\%1\"\n"_s.arg(payloadPath(directory));
  }
  const auto manifest = QDir(absolute(output)).filePath(u"windows-uninstall.nsh"_s);
  writeFile(manifest, removal.toUtf8());
  const auto installer =
      QDir(absolute(output)).filePath(u"akyuu-%1-x86_64-setup.exe"_s.arg(releaseVersion));
  const auto programFiles = qEnvironmentVariable("ProgramFiles(x86)");
  require(!programFiles.isEmpty(), u"The Windows build image has no ProgramFiles(x86) path."_s);
  call(QDir(programFiles).filePath(u"NSIS/makensis.exe"_s),
       {u"/DSTAGING_DIR=%1"_s.arg(native(stage)), u"/DOUTPUT_FILE=%1"_s.arg(native(installer)),
        u"/DPRODUCT_VERSION=%1"_s.arg(match.captured(1)),
        u"/DDISPLAY_VERSION=%1"_s.arg(releaseVersion),
        u"/DUNINSTALL_MANIFEST=%1"_s.arg(native(manifest)),
        QDir(root).filePath(u"setup/Akyuu.nsi"_s)});
  require(QFileInfo::exists(installer), u"The Windows installer was not produced."_s);
  QTextStream(stdout) << installer << Qt::endl;
#else
  Q_UNUSED(root)
  Q_UNUSED(build)
  Q_UNUSED(qt)
  Q_UNUSED(licenses)
  Q_UNUSED(output)
  require(false, u"Windows installer packaging requires Windows."_s);
#endif
}

void testWindows(const QString& installer, const QString& probe) {
#ifdef Q_OS_WIN
  require(qEnvironmentVariable("GITHUB_ACTIONS") == u"true"_s &&
              !qEnvironmentVariable("RUNNER_TEMP").isEmpty(),
          u"Run this installation test only on a disposable GitHub Actions runner."_s);
  const auto work = QDir(qEnvironmentVariable("RUNNER_TEMP")).filePath(u"akyuu installed test"_s);
  require(!QFileInfo::exists(work), u"The installation test directory must not exist."_s);
  require(QFileInfo::exists(installer) && QFileInfo::exists(probe),
          u"The installer and deployment probe must exist."_s);
  StartupKey startup;
  require(QDir().mkpath(work), u"Cannot create the installation test directory."_s);
  auto environment = QProcessEnvironment::systemEnvironment();
  environment.insert(u"QT_QPA_PLATFORM"_s, u"offscreen"_s);
  const auto systemRoot = qEnvironmentVariable("SystemRoot");
  const auto programFiles = qEnvironmentVariable("ProgramFiles");
  require(!systemRoot.isEmpty() && !programFiles.isEmpty(),
          u"The Windows runner paths are missing."_s);
  environment.insert(u"PATH"_s, u"%1/system32;%1;%2/Git/cmd"_s.arg(systemRoot, programFiles));
  environment.remove(u"QT_PLUGIN_PATH"_s);
  environment.remove(u"QML_IMPORT_PATH"_s);
  const auto marker = QDir(roamingDirectory()).filePath(u"akyuu/data/packaging-test-marker"_s);
  require(!QFileInfo::exists(marker), u"The user-data test marker must not exist."_s);
  require(QDir().mkpath(QFileInfo(marker).absolutePath()),
          u"Cannot create the user-data test directory."_s);
  writeFile(marker, "Preserve user data\n");
  const auto expected = sha256(marker);
  const auto install = QDir(work).filePath(u"application"_s);
  const auto unrelated = QDir(install).filePath(u"unrelated/subdirectory/keep.txt"_s);
  require(QDir().mkpath(QFileInfo(unrelated).absolutePath()),
          u"Cannot create the unrelated-file fixture."_s);
  writeFile(unrelated, "Preserve unrelated installation-directory files\n");
  const auto unrelatedHash = sha256(unrelated);
  const auto installArguments = u"/S /D=%1"_s.arg(native(install));
  for (int attempt = 0; attempt < 2; ++attempt) {
    // NSIS consumes the unquoted /D tail; QProcess must not quote it as a normal argument.
    installerProcess(installer, installArguments, environment);
    const auto installedProbe = QDir(install).filePath(u"akyuu-deployment-tests.exe"_s);
    copyFile(probe, installedProbe);
    runtimeProbe(installedProbe, environment);
    require(QFile::remove(installedProbe), u"Cannot remove the installed runtime probe."_s);
    installedStartup(QDir(install).filePath(u"Akyuu.exe"_s), environment);
  }
  auto packagedFiles = payloadEntries(install, false);
  packagedFiles.removeAll(absolute(unrelated));
  require(!packagedFiles.isEmpty(), u"The installed payload is empty."_s);
  const auto removePayload = [&] {
    installerProcess(QDir(install).filePath(u"Uninstall.exe"_s), u"/S"_s, environment);
    QElapsedTimer deadline;
    deadline.start();
    QStringList remaining;
    do {
      remaining.clear();
      for (const auto& file : packagedFiles) {
        if (QFileInfo::exists(file)) remaining.append(file);
      }
      if (remaining.isEmpty()) break;
      QThread::msleep(200);
    } while (deadline.elapsed() < 30000);
    require(remaining.isEmpty(),
            u"Uninstaller left packaged files: %1"_s.arg(remaining.join(u", "_s)));
  };
  const auto ownedCommand =
      u"\"%1\" --minimized"_s.arg(native(QDir(install).filePath(u"Akyuu.exe"_s)));
  startup.set(ownedCommand);
  removePayload();
  require(!startup.value().has_value(), u"Uninstaller left the owned startup command."_s);
  installerProcess(installer, installArguments, environment);
  const auto otherCommand = u"\"C:\\Unrelated Application\\startup.exe\" --keep"_s;
  startup.set(otherCommand);
  removePayload();
  require(startup.value() == std::optional<QString>(otherCommand),
          u"Uninstaller changed an unrelated startup command."_s);
  startup.remove();
  require(sha256(unrelated) == unrelatedHash,
          u"Uninstallation changed unrelated installation-directory files."_s);
  require(sha256(marker) == expected, u"Installation or removal changed user data."_s);
  QTextStream(stdout) << u"Passed installed startup, runtime, reinstall, user-data, unrelated-file "
                         u"preservation and owned-startup cleanup checks."_s
                      << Qt::endl;
#else
  Q_UNUSED(installer)
  Q_UNUSED(probe)
  require(false, u"Windows installation tests require Windows."_s);
#endif
}

}  // namespace release
