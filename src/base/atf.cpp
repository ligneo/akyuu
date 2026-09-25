/**
 * Akyuu
 * Copyright (C) 2010-2026, Eren Okka
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

#include "base/atf.hpp"

#include <QList>

namespace atf {

using namespace Qt::StringLiterals;

namespace {

bool isNumericString(const QString& str) {
  if (str.isEmpty()) return false;
  for (const auto c : str) {
    if (c < u'0' || c > u'9') return false;
  }
  return true;
}

// Reads the leading integer and ignores whatever follows, the way v1's ToInt does.
int toInt(const QString& str) {
  qsizetype pos = 0;
  while (pos < str.size() && str.at(pos).isSpace()) ++pos;
  const auto begin = pos;
  if (pos < str.size() && (str.at(pos) == u'+' || str.at(pos) == u'-')) ++pos;
  while (pos < str.size() && str.at(pos) >= u'0' && str.at(pos) <= u'9') ++pos;
  bool ok = false;
  const auto value = str.mid(begin, pos - begin).toInt(&ok);
  return ok ? value : 0;
}

// Comparisons are case insensitive, as they are in v1.
int compareStrings(const QString& a, const QString& b) {
  return a.compare(b, Qt::CaseInsensitive);
}

qsizetype indexOfAny(const QString& str, const QString& chars, const qsizetype pos) {
  for (auto i = qMax<qsizetype>(pos, 0); i < str.size(); ++i) {
    if (chars.contains(str.at(i))) return i;
  }
  return -1;
}

// Returns whether anything was replaced, so that callers can repeat until the string settles.
bool replaceString(QString& str, const QString& before, const QString& after) {
  if (before.isEmpty() || before == after || str.size() < before.size()) return false;
  if (!str.contains(before)) return false;
  str.replace(before, after);
  return true;
}

void trimString(QString& str, const QString& chars, const bool left, const bool right) {
  if (str.isEmpty()) return;

  qsizetype begin = 0;
  if (left) {
    while (begin < str.size() && chars.contains(str.at(begin))) ++begin;
    if (begin == str.size()) {
      str.clear();
      return;
    }
  }

  auto end = str.size() - 1;
  if (right) {
    while (end >= 0 && chars.contains(str.at(end))) --end;
    if (end < 0) {
      str.clear();
      return;
    }
  }

  str = str.mid(begin, end - begin + 1);
}

QList<QString> functionParams(const QString& body) {
  QList<QString> params;

  qsizetype begin = 0;
  qsizetype end = -1;

  do {  // Split by unescaped comma
    do {
      end = body.indexOf(u',', end + 1);
    } while (end > 0 && end < body.size() - 1 && body.at(end - 1) == u'\\');
    if (end == -1) end = body.size();

    params.append(body.mid(begin, end - begin));
    begin = end + 1;
  } while (begin <= body.size());

  return params;
}

QString evaluateFunction(const QString& name, const QString& body) {
  QString str;

  auto params = functionParams(body);

  // All functions should have parameters
  if (params.isEmpty()) return str;

  const auto compare_params = [&params](const auto predicate) {
    if (params.size() < 2) return false;
    if (isNumericString(params[0]) && isNumericString(params[1])) {
      return predicate(toInt(params[0]), toInt(params[1]));
    }
    return predicate(compareStrings(params[0], params[1]), 0);
  };

  // $and(x,y)
  //   Returns true, if all arguments evaluate to true.
  if (name == u"and"_s) {
    for (const auto& param : params) {
      if (param.isEmpty()) return {};
    }
    return u"true"_s;
    // $not(x)
    //   Returns true, if x is false.
  } else if (name == u"not"_s) {
    if (params[0].isEmpty()) return u"true"_s;
    // $or(x,y)
    //   Returns true, if at least one argument evaluates to true.
  } else if (name == u"or"_s) {
    for (const auto& param : params) {
      if (!param.isEmpty()) return u"true"_s;
    }

    // $cut(string,len)
    //   Returns first len characters of string.
  } else if (name == u"cut"_s) {
    if (params.size() > 1) {
      const auto length = toInt(params[1]);
      if (length >= 0 && length < params[0].size()) params[0].resize(length);
      str = params[0];
    }

    // $equal(x,y)
    //   Returns true, if x is equal to y.
  } else if (name == u"equal"_s) {
    if (compare_params([](const auto a, const auto b) { return a == b; })) return u"true"_s;
    // $gequal(x,y)
    //   Returns true, if x is greater as or equal to y.
  } else if (name == u"gequal"_s) {
    if (compare_params([](const auto a, const auto b) { return a >= b; })) return u"true"_s;
    // $greater(x,y)
    //   Returns true, if x is greater than y.
  } else if (name == u"greater"_s) {
    if (compare_params([](const auto a, const auto b) { return a > b; })) return u"true"_s;
    // $lequal(x,y)
    //   Returns true, if x is less than or equal to y.
  } else if (name == u"lequal"_s) {
    if (compare_params([](const auto a, const auto b) { return a <= b; })) return u"true"_s;
    // $less(x,y)
    //   Returns true, if x is less than y.
  } else if (name == u"less"_s) {
    if (compare_params([](const auto a, const auto b) { return a < b; })) return u"true"_s;

    // $if()
  } else if (name == u"if"_s) {
    switch (params.size()) {
      // $if(cond)
      case 1:
        str = params[0];
        break;
      // $if(cond,then)
      case 2:
        if (!params[0].isEmpty()) str = params[1];
        break;
      // $if(cond,then,else)
      case 3:
        str = !params[0].isEmpty() ? params[1] : params[2];
        break;
    }
    // $if2(a,else)
  } else if (name == u"if2"_s) {
    if (params.size() > 1) str = !params[0].isEmpty() ? params[0] : params[1];
    // $ifequal()
  } else if (name == u"ifequal"_s) {
    switch (params.size()) {
      // $ifequal(n1,n2,then)
      case 3:
        if (params[0] == params[1]) str = params[2];
        break;
      // $ifequal(n1,n2,then,else)
      case 4:
        str = params[0] == params[1] ? params[2] : params[3];
        break;
    }

    // $len(string)
    //   Returns length of string in characters.
  } else if (name == u"len"_s) {
    str = QString::number(params[0].size());

    // $lower(string)
    //   Converts string to lowercase.
  } else if (name == u"lower"_s) {
    str = params[0].toLower();
    // $upper(string)
    //   Converts string to uppercase.
  } else if (name == u"upper"_s) {
    str = params[0].toUpper();

    // $num(n,len)
    //   Formats the integer number n in decimal notation with len characters.
    //   Pads with zeros from the left if necessary.
  } else if (name == u"num"_s) {
    if (params.size() > 1) {
      const auto length = toInt(params[1]);
      if (length > params[0].size()) str.append(QString(length - params[0].size(), u'0'));
    }
    str += params[0];
    // $pad(s,len,chars)
    //   Pads string from the left with chars to len characters.
    //   If length of chars is smaller than len, padding will repeat.
  } else if (name == u"pad"_s) {
    if (params.size() == 2) params.append(u" "_s);
    if (params.size() > 2) {
      if (params[2].isEmpty()) params[2] = u" "_s;
      const auto length = toInt(params[1]);
      for (qsizetype i = 0; i < length - params[0].size(); ++i) {
        str += params[2].at(i % params[2].size());
      }
    }
    str += params[0];

    // $replace(a,b,c)
    //   Replaces all occurrences of string b in string a with string c.
  } else if (name == u"replace"_s) {
    if (params.size() == 2) params.append(QString{});
    if (params.size() > 2) {
      str = params[0];
      while (replaceString(str, params[1], params[2]));
    }

    // $substr(s,pos,n)
    //   Returns substring of string s, starting from pos with a length of n characters.
  } else if (name == u"substr"_s) {
    if (params.size() > 2) {
      if (toInt(params[1]) <= params[0].size()) {
        str = params[0].mid(toInt(params[1]), toInt(params[2]));
      }
    }

    // $triml()
    //   Removes leading characters from string.
    //   v1 trims the parameter but returns an empty string; the trimmed string is returned here.
  } else if (name == u"triml"_s) {
    // $triml(s,c)
    if (params.size() > 1) {
      trimString(params[0], params[1], true, false);
      // $triml(s)
    } else {
      trimString(params[0], u" "_s, true, false);
    }
    str = params[0];
    // $trimr()
    //   Removes trailing characters from string.
  } else if (name == u"trimr"_s) {
    // $trimr(s,c)
    if (params.size() > 1) {
      trimString(params[0], params[1], false, true);
      // $trimr(s)
    } else {
      trimString(params[0], u" "_s, false, true);
    }
    str = params[0];
  }

  return str;
}

QString escapeEntities(const QString& str) {
  QString escaped;

  for (qsizetype pos = 0; pos <= str.size();) {
    auto entityPos = indexOfAny(str, u"$,()%\\"_s, pos);
    if (entityPos != -1) {
      escaped.append(str.mid(pos, entityPos - pos));
      escaped.append(u'\\');
      escaped.append(str.mid(entityPos, 1));
    } else {
      entityPos = str.size();
      escaped.append(str.mid(pos, entityPos - pos));
    }
    pos = entityPos + 1;
  }

  return escaped;
}

QString unescapeEntities(const QString& str) {
  QString unescaped;
  unescaped.reserve(str.size());

  for (qsizetype pos = 0; pos < str.size(); ++pos) {
    if (str.at(pos) == u'\\') {
      if (pos + 1 < str.size() && str.at(pos + 1) == u'\\') unescaped.append(u'\\');
      continue;
    }
    unescaped.append(str.at(pos));
  }

  return unescaped;
}

QString replaceFunctions(QString str) {
  qsizetype posFunc = 0;
  qsizetype posLeft = 0;
  qsizetype posRight = 0;
  int openBrackets = 0;

  do {
    // Find non-escaped dollar sign
    posFunc = 0;
    while (true) {
      posFunc = str.indexOf(u'$', posFunc);
      if (posFunc > 0 && str.at(posFunc - 1) == u'\\') {
        posFunc += 1;
      } else {
        break;
      }
    }

    if (posFunc > -1) {
      for (auto i = posFunc; i < str.size(); ++i) {
        switch (str.at(i).unicode()) {
          case u'$':
            posFunc = i;
            posLeft = posRight = 0;
            openBrackets = 0;
            break;
          case u'(':
            if (posFunc > -1) {
              if (!openBrackets++) posLeft = i;
              posRight = 0;
            }
            break;
          case u')':
            if (posLeft) {
              if (openBrackets == 1) {
                posRight = i;
                const auto name = str.mid(posFunc + 1, posLeft - (posFunc + 1));
                const auto body = str.mid(posLeft + 1, posRight - (posLeft + 1));
                str = str.first(posFunc) + str.mid(posRight + 1);
                str.insert(posFunc, evaluateFunction(name, body));
                i = str.size();
              }
              if (openBrackets > 0) openBrackets--;
            }
            break;
          case u'\\':
            i++;
            break;
        }
      }
      if (!posLeft || !posRight) break;
    }
  } while (posFunc > -1);

  return str;
}

QString replaceVariables(QString str, const field_map_t& fields) {
  qsizetype posVar = 0;

  do {
    posVar = str.indexOf(u'%', posVar);
    if (posVar > -1) {
      const auto posEnd = str.indexOf(u'%', posVar + 1);
      if (posEnd > -1) {
        const auto name = str.mid(posVar + 1, posEnd - posVar - 1);
        if (const auto it = fields.find(name); it != fields.end()) {
          if (it->second) {
            const auto evaluated = escapeEntities(*it->second);
            str.replace(posVar, name.size() + 2, evaluated);
            posVar += evaluated.size();
          } else {
            str.remove(posVar, name.size() + 2);
          }
          continue;
        }
        posVar = posEnd + 1;
      } else {
        posVar++;
      }
    }
  } while (posVar > -1);

  return str;
}

}  // namespace

QString replace(QString str, const field_map_t& fields) {
  str = replaceVariables(str, fields);
  replaceString(str, u"\\n"_s, u"\n"_s);
  replaceString(str, u"\\t"_s, u"\t"_s);
  str = replaceFunctions(str);
  str = unescapeEntities(str);
  while (replaceString(str, u"\n\n"_s, u"\n"_s));
  while (replaceString(str, u"  "_s, u" "_s));
  trimString(str, u"\t\n\r "_s, true, true);
  return str;
}

}  // namespace atf
