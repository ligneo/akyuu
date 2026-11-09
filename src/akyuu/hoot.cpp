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

#include "hoot.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

#ifdef Q_OS_WINDOWS
#include <windows.h>
#else
#include <QByteArray>
#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>
#endif

namespace {

// Akyuu's own tune, in D minor pentatonic (D F G A C), after the mood of KOKIA's "Fukurou" (owl)
// but a little slower, and without borrowing its notes. The owl calls twice, a falling minor
// third; a phrase rises into the night and a lower one answers; one last call, and it settles on
// D. Every phrase starts on a half bar, six bars in all.
// clang-format off
constexpr std::array<std::pair<int, float>, 28> notes{{
  {84, 1/8.f}, {81, 3/8.f}, {84, 1/8.f}, {81, 3/8.f},
  {74, 1/8.f}, {77, 1/8.f}, {79, 1/8.f}, {81, 1/4.f}, {84, 1/8.f},
  {81, 1/8.f}, {79, 1/4.f}, {77, 1/8.f}, {74, 1/4.f},
  {72, 1/8.f}, {74, 1/8.f}, {77, 1/4.f}, {79, 1/8.f}, {77, 1/8.f},
  {74, 1/4.f}, {69, 1/8.f}, {72, 1/8.f}, {74, 1/4.f},
  {84, 1/8.f}, {81, 3/8.f}, {79, 1/8.f}, {81, 1/8.f}, {77, 1/4.f}, {74, 1.f},
}};
// clang-format on

constexpr float get_frequency(const int note) {
  if (note < 0 || note > 119) return -1.0f;
  return 440.0f * std::pow(2.0f, static_cast<float>(note - 57) / 12.0f);
};

constexpr float get_duration(const float duration) {
  return 2220 * duration;  // a whole note, at about 108 bpm
};

#ifndef Q_OS_WINDOWS
constexpr int kSampleRate = 44100;
constexpr int kChannelCount = 2;

// `Beep` takes a frequency and a length in milliseconds and plays a square wave; there is no such
// call here, so the same notes are rendered into a WAV file for the system's own player. A sine
// wave is used instead of a square wave because it is the same pitch without the harshness, and a
// few milliseconds of fade on both ends keep the notes from clicking.
QByteArray renderNotes() {
  constexpr float kPi = std::numbers::pi_v<float>;
  constexpr int kAttackSamples = kSampleRate * 12 / 1000;
  constexpr int kReleaseSamples = kSampleRate * 35 / 1000;
  constexpr int kLastReleaseSamples = kSampleRate * 600 / 1000;  // the last note rings out

  std::vector<float> wave;

  for (size_t n = 0; n < notes.size(); ++n) {
    const auto& [note, duration] = notes[n];
    const float frequency = get_frequency(note);
    const int samples = static_cast<int>(kSampleRate * get_duration(duration) / 1000.0f);
    const int release = n + 1 == notes.size() ? kLastReleaseSamples : kReleaseSamples;
    const bool longNote = duration >= 3 / 8.f;

    float phase = 0.0f;

    for (int i = 0; i < samples; ++i) {
      const float time = static_cast<float>(i) / kSampleRate;

      // A quick attack, a gentle decay to 70% and a soft release, so that the notes sound
      // played rather than switched on and off.
      float envelope = std::min(1.0f, static_cast<float>(i) / kAttackSamples);
      envelope *= 0.7f + 0.3f * std::exp(-3.0f * time);
      if (i > samples - release) {
        envelope *= static_cast<float>(samples - i) / release;
      }

      // A slight vibrato on the long notes, once they have settled
      float vibrato = 1.0f;
      if (longNote) {
        const float depth = std::clamp((time - 0.18f) / 0.2f, 0.0f, 1.0f);
        vibrato += 0.004f * depth * std::sin(2.0f * kPi * 5.2f * time);
      }

      phase += 2.0f * kPi * frequency * vibrato / kSampleRate;

      // Two quiet overtones make it warmer than a plain sine
      const float tone =
          std::sin(phase) + 0.22f * std::sin(2.0f * phase) + 0.07f * std::sin(3.0f * phase);
      wave.push_back(frequency > 0.0f ? 0.17f * envelope * tone : 0.0f);
    }
  }

  // A soft echo, as if the forest answered: two quieter repeats, 230 and 460 ms later
  constexpr int kEchoSamples = kSampleRate * 230 / 1000;
  wave.resize(wave.size() + kSampleRate * 600 / 1000, 0.0f);
  std::vector<float> mixed = wave;
  for (size_t i = kEchoSamples; i < wave.size(); ++i) {
    mixed[i] += 0.22f * wave[i - kEchoSamples];
    if (i >= 2 * kEchoSamples) mixed[i] += 0.09f * wave[i - 2 * kEchoSamples];
  }

  QByteArray data;
  data.reserve(static_cast<qsizetype>(mixed.size()) * kChannelCount * 2);

  for (const float value : mixed) {
    const auto sample =
        static_cast<qint16>(std::clamp(value, -1.0f, 1.0f) * std::numeric_limits<qint16>::max());

    // The same sample for the left and the right channel: a mono stream with no channel map
    // can end up in the left ear only.
    for (int channel = 0; channel < kChannelCount; ++channel) {
      data.append(static_cast<char>(sample & 0xff));
      data.append(static_cast<char>((sample >> 8) & 0xff));
    }
  }

  return data;
}

void appendLittleEndian(QByteArray& data, const quint32 value, const int size) {
  for (int i = 0; i < size; ++i) {
    data.append(static_cast<char>((value >> (8 * i)) & 0xff));
  }
}

QByteArray makeWav(const QByteArray& samples) {
  constexpr int kBytesPerSample = 2;
  const auto size = static_cast<quint32>(samples.size());

  QByteArray wav;
  wav.reserve(44 + samples.size());
  wav.append("RIFF");
  appendLittleEndian(wav, 36 + size, 4);
  wav.append("WAVEfmt ");
  appendLittleEndian(wav, 16, 4);  // size of the format chunk
  appendLittleEndian(wav, 1, 2);   // PCM
  appendLittleEndian(wav, kChannelCount, 2);
  appendLittleEndian(wav, kSampleRate, 4);
  appendLittleEndian(wav, kSampleRate * kChannelCount * kBytesPerSample, 4);
  appendLittleEndian(wav, kChannelCount * kBytesPerSample, 2);
  appendLittleEndian(wav, 8 * kBytesPerSample, 2);
  wav.append("data");
  appendLittleEndian(wav, size, 4);
  wav.append(samples);
  return wav;
}
#endif

}  // namespace

using namespace Qt::StringLiterals;

namespace akyuu {

Hoot::Hoot(QObject* parent) : QThread(parent) {}

Hoot::~Hoot() {
  requestInterruption();
  wait();
}

void Hoot::run() {
#ifdef Q_OS_WINDOWS
  for (const auto& [note, duration] : notes) {
    if (isInterruptionRequested()) break;
    ::Beep(static_cast<DWORD>(get_frequency(note)), static_cast<DWORD>(get_duration(duration)));
  }

#else
  QTemporaryFile file{QDir::tempPath() + u"/akyuu-hoot-XXXXXX.wav"_s};
  if (!file.open() || file.write(makeWav(renderNotes())) < 0 || !file.flush()) return;

  // One of these comes with every usual sound setup. Without any of them the tune stays silent.
  for (const auto& name : {u"pw-play"_s, u"paplay"_s, u"aplay"_s}) {
    const auto program = QStandardPaths::findExecutable(name);
    if (program.isEmpty()) continue;

    QProcess player;
    player.setStandardOutputFile(QProcess::nullDevice());
    player.setStandardErrorFile(QProcess::nullDevice());
    player.start(program, {file.fileName()});
    if (!player.waitForStarted()) continue;

    while (!player.waitForFinished(50) && player.state() != QProcess::NotRunning) {
      if (isInterruptionRequested()) {
        player.kill();
        player.waitForFinished();
        return;
      }
    }

    if (player.exitStatus() == QProcess::NormalExit && player.exitCode() == 0) return;
  }
#endif
}

}  // namespace akyuu
