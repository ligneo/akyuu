/**
 * Taiga
 * Copyright (C) 2010-2024, Eren Okka
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

#include "orange.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <numbers>

#ifdef Q_OS_WINDOWS
#include <windows.h>
#endif

#ifdef TAIGA_HAS_MULTIMEDIA
#include <QAudioFormat>
#include <QAudioSink>
#include <QBuffer>
#include <QEventLoop>
#include <QMediaDevices>
#include <QTimer>
#endif

namespace {

// clang-format off
constexpr std::array<std::pair<int, float>, 32> notes{{
  {84, 1/2.f}, {84, 1/4.f}, {86, 1/8.f}, {84, 1/4.f},
  {82, 1/4.f}, {81, 1/4.f}, {77, 1/8.f}, {79, 1/8.f},
  {72, 1/8.f}, {77, 1/2.f}, {76, 1/8.f}, {77, 1/8.f},
  {79, 1/8.f}, {81, 1/4.f}, {79, 1/4.f}, {77, 1/4.f},
  {79, 1/4.f}, {81, 1/8.f}, {84, 1/2.f}, {84, 1/4.f},
  {86, 1/8.f}, {84, 1/4.f}, {82, 1/4.f}, {81, 1/4.f},
  {77, 1/8.f}, {79, 1/8.f}, {72, 1/8.f}, {77, 1/2.f},
  {76, 1/8.f}, {77, 1/8.f}, {76, 1/8.f}, {74, 1/2.f},
}};
// clang-format on

constexpr float get_frequency(const int note) {
  if (note < 0 || note > 119) return -1.0f;
  return 440.0f * std::pow(2.0f, static_cast<float>(note - 57) / 12.0f);
};

constexpr float get_duration(const float duration) {
  return 1600 * duration;
};

#ifdef TAIGA_HAS_MULTIMEDIA
constexpr int kSampleRate = 44100;

// `Beep` takes a frequency and a length in milliseconds and plays a square wave; there is no Qt
// equivalent, so the same notes are rendered into one buffer and handed to an audio sink. A sine
// wave is used instead of a square wave because it is the same pitch without the harshness, and a
// few milliseconds of fade on both ends keep the notes from clicking.
QByteArray renderNotes() {
  constexpr int kFadeSamples = kSampleRate / 200;  // 5 ms

  QByteArray data;

  for (const auto& [note, duration] : notes) {
    const float frequency = get_frequency(note);
    const int samples = static_cast<int>(kSampleRate * get_duration(duration) / 1000.0f);

    for (int i = 0; i < samples; ++i) {
      float amplitude = frequency > 0.0f ? 0.2f : 0.0f;
      if (i < kFadeSamples) {
        amplitude *= static_cast<float>(i) / kFadeSamples;
      } else if (i > samples - kFadeSamples) {
        amplitude *= static_cast<float>(samples - i) / kFadeSamples;
      }

      const float time = static_cast<float>(i) / kSampleRate;
      const float value = amplitude * std::sin(2.0f * std::numbers::pi_v<float> * frequency * time);
      const auto sample = static_cast<qint16>(value * std::numeric_limits<qint16>::max());

      data.append(static_cast<char>(sample & 0xff));
      data.append(static_cast<char>((sample >> 8) & 0xff));
    }
  }

  return data;
}
#endif

}  // namespace

namespace taiga {

Orange::Orange(QObject* parent) : QThread(parent) {}

Orange::~Orange() {
  requestInterruption();
  wait();
}

void Orange::run() {
#ifdef Q_OS_WINDOWS
  for (const auto& [note, duration] : notes) {
    if (isInterruptionRequested()) break;
    ::Beep(static_cast<DWORD>(get_frequency(note)), static_cast<DWORD>(get_duration(duration)));
  }

#elif defined(TAIGA_HAS_MULTIMEDIA)
  QAudioFormat format;
  format.setSampleRate(kSampleRate);
  format.setChannelCount(1);
  format.setSampleFormat(QAudioFormat::Int16);

  const auto device = QMediaDevices::defaultAudioOutput();
  if (device.isNull() || !device.isFormatSupported(format)) return;

  QBuffer buffer;
  buffer.setData(renderNotes());

  if (!buffer.open(QIODevice::ReadOnly)) return;

  QAudioSink sink{device, format};
  QEventLoop loop;

  connect(&sink, &QAudioSink::stateChanged, &loop, [&loop](QAudio::State state) {
    if (state == QAudio::IdleState || state == QAudio::StoppedState) loop.quit();
  });

  // The thread has no other way out of the loop; closing the dialog asks it to stop.
  QTimer timer;
  connect(&timer, &QTimer::timeout, &loop, [this, &sink, &loop]() {
    if (!isInterruptionRequested()) return;
    sink.stop();
    loop.quit();
  });
  timer.start(std::chrono::milliseconds{50});

  sink.start(&buffer);
  loop.exec();
#endif
}

}  // namespace taiga
