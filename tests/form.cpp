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

#include "base/form.hpp"

#include <QUrl>
#include <cstdlib>
#include <iostream>

namespace {

void require(bool condition, const char* message) {
  if (condition) return;
  std::cerr << message << '\n';
  std::exit(EXIT_FAILURE);
}

// What a server reads back from a form body
QString decode(QByteArray value) {
  value.replace('+', ' ');
  return QString::fromUtf8(QByteArray::fromPercentEncoding(value));
}

}  // namespace

int main() {
  require(base::formUrlEncode({}).isEmpty(), "An empty form has an empty body");

  const QString password = QStringLiteral("a+b%41 c&d=e#f \u30A2");
  const auto body = base::formUrlEncode({{QStringLiteral("password"), password},
                                         {QStringLiteral("grant_type"), QStringLiteral("x")}});
  require(body == "password=a%2Bb%2541%20c%26d%3De%23f%20%E3%82%A2&grant_type=x",
          "Each value is encoded from its text");

  const auto value = body.split('&').first().split('=').last();
  require(decode(value) == password, "A plus sign and a percent sequence reach the server");

  return EXIT_SUCCESS;
}
