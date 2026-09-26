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

#include "format_dialog.hpp"

#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QLabel>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QVBoxLayout>

#include "akyuu/script.hpp"
#include "akyuu/settings.hpp"
#include "media/anime_db.hpp"
#include "media/anime_list.hpp"
#include "media/anime_utils.hpp"
#include "track/media.hpp"
#include "track/recognition.hpp"

namespace gui {

using namespace Qt::StringLiterals;

namespace {

// v1 colors functions and variables in the editor (`dlg_format.cpp` `ColorizeText`). The colors
// come from the palette, so that they read on a dark theme as well.
class FormatHighlighter final : public QSyntaxHighlighter {
public:
  FormatHighlighter(QTextDocument* document, const QPalette& palette)
      : QSyntaxHighlighter(document) {
    m_functionFormat.setForeground(palette.link());
    m_functionFormat.setFontWeight(QFont::Bold);
    m_variableFormat.setForeground(palette.linkVisited());
  }

protected:
  void highlightBlock(const QString& text) override {
    static const QRegularExpression function{u"\\$[a-z0-9]+(?=\\()"_s};
    static const QRegularExpression variable{u"%[a-z]+%"_s};

    for (auto it = function.globalMatch(text); it.hasNext();) {
      const auto match = it.next();
      setFormat(match.capturedStart(), match.capturedLength(), m_functionFormat);
    }
    for (auto it = variable.globalMatch(text); it.hasNext();) {
      const auto match = it.next();
      setFormat(match.capturedStart(), match.capturedLength(), m_variableFormat);
    }
  }

private:
  QTextCharFormat m_functionFormat;
  QTextCharFormat m_variableFormat;
};

// v1 previews with the episode being watched, or with a made-up one (`taiga/dummy.cpp`). The
// made-up one is built from the anime updated last, so that the list variables have values too.
track::Episode previewEpisode() {
  if (const auto episode = track::media::detection()->getCurrentEpisode()) return *episode;

  const anime::list::Entry* latest = nullptr;
  for (const auto& entry : anime::db.entries()) {
    if (!latest || entry.last_updated > latest->last_updated) latest = &entry;
  }

  const auto item = latest ? anime::db.item(latest->anime_id) : nullptr;
  const auto title = item ? anime::preferredTitle(*item) : std::string{"Toradora!"};
  const auto number = latest ? latest->watched_episodes + 1 : 1;

  auto episode = track::recognition::parse(
      std::format("[SubsPlease] {} - {:02} (1080p) [ABCD1234].mkv", title, number));
  if (item) episode.setAnimeId(item->id);
  return episode;
}

// IRC formatting codes change how the message looks, but they are not characters to show.
QString stripIrcCodes(QString text) {
  static const QRegularExpression color{u"\\x03(\\d{1,2}(,\\d{1,2})?)?"_s};
  static const QRegularExpression other{u"[\\x02\\x0f\\x16\\x1d\\x1f]"_s};
  return text.remove(color).remove(other);
}

}  // namespace

FormatDialog::FormatDialog(QWidget* parent, FormatDialogMode mode, const QString& format)
    : QDialog(parent),
      m_mode(mode),
      m_editFormat(new QPlainTextEdit(this)),
      m_editPreview(new QPlainTextEdit(this)) {
  setWindowTitle(tr("Format string"));
  resize(560, 380);

  const auto fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  m_editFormat->setFont(fixedFont);
  m_editFormat->setPlainText(format);
  new FormatHighlighter(m_editFormat->document(), palette());

  m_editPreview->setReadOnly(true);
  m_editPreview->setMaximumHeight(96);

  const auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  const auto buttonAdd = buttons->addButton(tr("Add"), QDialogButtonBox::ActionRole);
  buttonAdd->setMenu(createAddMenu());
  const auto buttonReset = buttons->addButton(QDialogButtonBox::RestoreDefaults);

  const auto layout = new QVBoxLayout(this);
  layout->addWidget(m_editFormat, 1);
  layout->addWidget(new QLabel(tr("Preview:"), this));
  layout->addWidget(m_editPreview);
  layout->addWidget(buttons);

  connect(m_editFormat, &QPlainTextEdit::textChanged, this, &FormatDialog::refreshPreview);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(buttonReset, &QPushButton::clicked, this, [this]() {
    switch (m_mode) {
      case FormatDialogMode::Http:
        m_editFormat->setPlainText(akyuu::Settings::defaultHttpShareFormat());
        break;
      case FormatDialogMode::Irc:
        m_editFormat->setPlainText(akyuu::Settings::defaultIrcShareFormat());
        break;
      case FormatDialogMode::Notification:
        m_editFormat->setPlainText(akyuu::Settings::defaultSyncNotifyFormat());
        break;
    }
  });

  refreshPreview();
}

QString FormatDialog::format() const {
  return m_editFormat->toPlainText().trimmed();
}

std::optional<QString> FormatDialog::edit(QWidget* parent, FormatDialogMode mode,
                                          const QString& format) {
  FormatDialog dialog(parent, mode, format);
  if (dialog.exec() != QDialog::Accepted) return std::nullopt;
  return dialog.format();
}

// v1's `ScriptAdd` menu (`menu.xml`), with the IRC codes only where they mean something.
QMenu* FormatDialog::createAddMenu() {
  const auto menu = new QMenu(this);

  const auto addItem = [this](QMenu* menu, const QString& text, const QString& insert) {
    menu->addAction(text, this, [this, insert]() { insertText(insert); });
  };

  {
    const auto characters = menu->addMenu(tr("Character"));

    if (m_mode == FormatDialogMode::Irc) {
      const auto irc = characters->addMenu(tr("IRC characters"));
      addItem(irc, tr("Bold"), u"\x02"_s);
      const auto colors = irc->addMenu(tr("Color"));
      static const QStringList colorNames{
          tr("White"),      tr("Black"),       tr("Blue"),   tr("Green"),
          tr("Light red"),  tr("Brown"),       tr("Purple"), tr("Orange"),
          tr("Yellow"),     tr("Light green"), tr("Cyan"),   tr("Light cyan"),
          tr("Light blue"), tr("Pink"),        tr("Grey"),   tr("Light grey"),
      };
      for (qsizetype i = 0; i < colorNames.size(); ++i) {
        const auto code = u"%1"_s.arg(i, 2, 10, u'0');
        addItem(colors, u"%1 - %2"_s.arg(code, colorNames.at(i)), u"\x03"_s + code);
      }
      addItem(irc, tr("Italic"), u"\x1d"_s);
      addItem(irc, tr("Reverse"), u"\x16"_s);
      addItem(irc, tr("Underline"), u"\x1f"_s);
      irc->addSeparator();
      addItem(irc, tr("Disable all"), u"\x0f"_s);
    }

    addItem(characters, tr("New line (\\n)"), u"\\n"_s);
    addItem(characters, tr("Horizontal tab (\\t)"), u"\\t"_s);
  }

  {
    const auto functions = menu->addMenu(tr("Function"));
    static const QList<QPair<QString, QString>> items{
        {u"and()"_s, u"$and(x,y)"_s},
        {u"cut()"_s, u"$cut(string,len)"_s},
        {u"equal()"_s, u"$equal(x,y)"_s},
        {u"gequal()"_s, u"$gequal(x,y)"_s},
        {u"greater()"_s, u"$greater(x,y)"_s},
        {u"if()"_s, u"$if(cond,then,else)"_s},
        {u"if2()"_s, u"$if(a,else)"_s},
        {u"ifequal()"_s, u"$ifequal(n1,n2,then,else)"_s},
        {u"lequal()"_s, u"$lequal(x,y)"_s},
        {u"len()"_s, u"$len(string)"_s},
        {u"less()"_s, u"$less(x,y)"_s},
        {u"lower()"_s, u"$lower(string)"_s},
        {u"not()"_s, u"$not(x)"_s},
        {u"num()"_s, u"$num(n,len)"_s},
        {u"or()"_s, u"$or(x,y)"_s},
        {u"pad()"_s, u"$pad(s,len,chars)"_s},
        {u"replace()"_s, u"$replace(a,b,c)"_s},
        {u"substr()"_s, u"$substr(s,pos,n)"_s},
        {u"triml()"_s, u"$triml(s,c)"_s},
        {u"trimr()"_s, u"$trimr(s,c)"_s},
        {u"upper()"_s, u"$upper(string)"_s},
    };
    for (const auto& [text, insert] : items) addItem(functions, text, insert);
  }

  {
    const auto variables = menu->addMenu(tr("Variable"));
    // An empty name is a separator
    static const QList<QPair<QString, QString>> items{
        {tr("Anime ID"), u"%id%"_s},
        {tr("Anime title"), u"%title%"_s},
        {tr("Anime season"), u"%season%"_s},
        {tr("Anime URL"), u"%animeurl%"_s},
        {tr("Image URL"), u"%image%"_s},
        {tr("Total episodes"), u"%total%"_s},
        {{}, {}},
        {tr("Watched episodes"), u"%watched%"_s},
        {tr("Score"), u"%score%"_s},
        {tr("Watching status"), u"%status%"_s},
        {tr("Rewatching"), u"%rewatching%"_s},
        {tr("Notes"), u"%notes%"_s},
        {{}, {}},
        {tr("Filename"), u"%file%"_s},
        {tr("Episode number"), u"%episode%"_s},
        {tr("Episode title"), u"%name%"_s},
        {tr("Release group"), u"%group%"_s},
        {tr("Release version"), u"%version%"_s},
        {tr("Audio terms"), u"%audio%"_s},
        {tr("Video resolution"), u"%resolution%"_s},
        {tr("Video terms"), u"%video%"_s},
        {tr("Checksum"), u"%checksum%"_s},
        {{}, {}},
        {tr("Folder"), u"%folder%"_s},
        {tr("Manual"), u"%manual%"_s},
        {tr("Play status"), u"%playstatus%"_s},
        {tr("Username"), u"%user%"_s},
    };
    for (const auto& [text, insert] : items) {
      if (text.isEmpty()) {
        variables->addSeparator();
      } else {
        addItem(variables, text, insert);
      }
    }
  }

  return menu;
}

void FormatDialog::insertText(const QString& text) {
  m_editFormat->insertPlainText(text);
  m_editFormat->setFocus();
}

void FormatDialog::refreshPreview() {
  auto text = akyuu::replaceVariables(m_editFormat->toPlainText(), previewEpisode());
  if (m_mode == FormatDialogMode::Irc) text = stripIrcCodes(text);
  m_editPreview->setPlainText(text);
}

}  // namespace gui
