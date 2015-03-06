#include <QDir>
#include <QTime>
#include <QHash>
#include <QString>
#include <QTextCodec>
#include <QTextStream>
#include <QDomElement>
#include <QStringList>
#include <iostream>
using namespace std;

QString consoleOutput;

void toLog(QString input)
{
  consoleOutput += "\r\n" + input;
}

void toConsole(QString input)
{
  cerr << qPrintable(input) << endl;
  toLog(input);
}

QString colorSplit(QString color)
{
  QString ret = color;
  ret.insert(2, " ");
  ret.insert(5, " ");
  ret.insert(8, " ");
  return ret;
}

void adjustTime(QString &time)
{
  if (time.indexOf(":") == 1) {
    time = "0" + time;
  }
  int size = time.size();
  int index = time.indexOf(".");
  if (index != size - 4) {
    if (index == -1) {
      time += ".000";
    } else if (index == size - 3) {
      time += "0";
    } else if (index == size - 2) {
      time += "00";
    } else if (index == size - 1) {
      time += "000";
    }
  }
}
//AABBGGRR to RRGGBBAA
QString convertColorForTtxt(const QString value)
{
  QString rr = value.at(6);
  rr += value.at(7);
  QString gg = value.at(4);
  gg += value.at(5);
  QString bb = value.at(2);
  bb += value.at(3);
  QString aa = value.at(0);
  aa += value.at(1);

  if (aa == "00") {
    aa = "FF";
  }

  return rr + gg + bb + aa;
}

QDomElement createStyleNode(QDomDocument &dom, const QStringList &style, QString styleName)
{
  QDomElement child = dom.createElement("Style");
  //styleID, fontName, Styles, fontsize, fontColor, backColor
  child.setAttribute("styles", style.at(2));
  //child.setAttribute("fontID", style.at(0));
  child.setAttribute("fontSize", style.at(3).toInt());
  child.setAttribute("color", colorSplit(style.at(4)));
  return child;
}

void removeLineStyling(QString &line)
{
  int index1, index2;
  while (line.contains("{") && line.contains("}")) { //removes additionals Styling infos
    index1 = line.indexOf("{");
    index2 = line.indexOf("}") - index1;
    line = line.remove(index1, index2 + 1);
  }
}

QString timeDistanceError(QString line, double s, double e)
{
  QString timeDistanceError = " -" + QObject::tr("Ignoring: %1").arg(line);
  timeDistanceError += "\r\n  -";
  timeDistanceError += QObject::tr("current start < last end time (s): %1 < %2").arg(s).arg(e);
  timeDistanceError += "\r\n  -";
  timeDistanceError += QObject::tr("afaik mp4box does not allow overlapping times.");
  return timeDistanceError;
}

double timeToSeconds(QTime time)
{
  return ((time.hour() * 60.0 + time.minute()) * 60.0 + time.second()) + (time.msec() / 1000.0);
}

double timeToSeconds(QString value)
{
  QString tmp = value.trimmed();
  double dtime = tmp.toDouble();
  int index = tmp.indexOf(":");
  int colonCount = tmp.count(":");
  if (index != -1) {
    if (index == 1) {
      tmp = "0" + tmp;
    }
    index = tmp.indexOf(".");
    QTime time;
    if (index == -1) {
      switch (colonCount) {
        case 1 :
          time = QTime::fromString(tmp, "mm:ss");
          break;
        case 2 :
          time = QTime::fromString(tmp, "hh:mm:ss");
          break;
      }
      return (time.hour() * 60.0 + time.minute()) * 60.0 + time.second();
    }
    time = QTime::fromString(tmp, "hh:mm:ss.z");
    switch (colonCount) {
      case 1 :
        time = QTime::fromString(tmp, "mm:ss.z");
        break;
      case 2 :
        time = QTime::fromString(tmp, "hh:mm:ss.z");
        break;
    }
    dtime = timeToSeconds(time);
  }
  return dtime;
}

QStringList cleanUpAssForTimes(QStringList lines, bool output = false)
{
  if (output) {
    toConsole(" " + QObject::tr("Start cleaning ass file for ttxt conversion,.."));
  }
  lines.sort();
  QStringList tmp = lines, elems, cleaned;
  double dEnd = 0, dStart, lStart = -1;
  QString startTime, endTime, line;

  for (int i = 0, c = tmp.count(); i < c; ++i) {
    line = tmp.at(i);
    if (line.isEmpty()) {
      continue;
    }
    elems = line.split(",");
    startTime = elems.at(1).trimmed();
    dStart = timeToSeconds(startTime);
    if (dStart <= lStart) {
      if (output) {
        toLog(" " + QObject::tr("Ignoring %1 since start time <= last start time!").arg(line));
        toLog(" " + QObject::tr("afaik mp4box does not allow overlapping times "));
      }
      continue;
    }
    endTime = elems.at(2).trimmed();
    dEnd = timeToSeconds(endTime);
    if (dStart == dEnd) {
      if (output) {
        toLog(" " + QObject::tr("Ignoring %1 since start and end time are equal!").arg(line));
      }
      continue;
    }
    lStart = dStart;
    cleaned << line;
  }
  if (output) {
    toConsole(" " + QObject::tr("Finished cleaning ass file for ttxt conversion."));
  }
  return cleaned;
}

QHash<QString, QStringList> grabbingStylingInfos(QString content)
{
  QHash<QString, QStringList> styles;
  QString cleanContent, styleName, backColor, fontSize, fontStyles, fontColor, fontName, tmp;
  int index = content.indexOf("Styles]"), styleIndex = 0;
  QStringList style, elems;
  if (index != -1) {
    index = content.indexOf("Style:", index);
    QString line, color, styleName;
    toConsole("\r\n--- Style detection start ---");
    while (index != -1) {
      style.clear(); //styleID, fontName, Styles, fontsize, fontColor, backColor
      line = content;
      line = line.remove(0, index + 7).trimmed();
      line = line.remove(line.indexOf("\n"), line.size());
      elems = line.split(",");
      styleName = elems.at(0); //StyleName
      style << QString::number(++styleIndex);
      fontName = elems.at(1);
      if (fontName.startsWith("@")) {
        fontName = fontName.remove("@");
        toConsole(QObject::tr(" -Warning:\r\n %1").arg(line));
        toConsole("  -" + QObject::tr("refers to an 'attached' font (%1)").arg(fontName));
        tmp = QObject::tr("font will be needed during playback or rendering will be off.");
        toConsole("  -" + tmp);
      }
      style << fontName; //FontName

      if (elems.at(7) == "1") {
        if (fontStyles == "Normal") {
          fontStyles = "Bold";
        } else {
          fontStyles += " Bold";
        }
      }
      if (elems.at(8) == "1") {
        if (fontStyles == "Normal") {
          fontStyles = "Italic";
        } else {
          fontStyles += " Italic";
        }
      }
      if (elems.at(9) == "1") {
        if (fontStyles == "Normal") {
          fontStyles = "Underlined";
        } else {
          fontStyles += " Underlined";
        }
      }
      if (fontStyles.isEmpty()) {
        fontStyles = "Normal";
      }
      style << fontStyles;  // FontStyles

      style << elems.at(2); //FontSize

      color = elems.at(3);
      color = color.remove(0, 2);
      fontColor = convertColorForTtxt(color);  //convert AABBGGRR to RRGGBBAA
      style << fontColor;

      color = elems.at(6);
      color = color.remove(0, 2);
      backColor = convertColorForTtxt(color);  //convert AABBGGRR to RRGGBBAA
      style << backColor;
      styles.insert(styleName, style);
      index = content.indexOf("Style:", index + 7);
      toConsole(" - " + styleName + ": " + style.join(","));
    }
  }
  toConsole("--- Style detection end ---\r\n");
  if (styles.isEmpty()) {
    toConsole("Styles is empty, using defaults,..");
    QStringList style;
    style << "1";
    style << "Normal";
    style << "36";
    style << "ff ff ff ff"; //RRGGBBAA
    style << "00 00 00 00";
    styles.insert("Default", style);
  }
  return styles;
}

QDomElement getXmlHeader(QDomDocument &dom)
{
  dom.appendChild(dom.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\""));
  dom.appendChild(dom.createComment("GPAC 3GPP Text Stream"));
  QDomElement root = dom.createElement("TextStream");
  root.setAttribute("version", "1.1");
  dom.appendChild(root);
  return root;
}

QString readInputDeleteOutput(QString inputFile, QString outputFile)
{
  QString errorMessage;
  if (!QFile::exists(inputFile)) {
    errorMessage = QObject::tr("ERROR: There exists no file named: %1").arg(inputFile);
    toConsole(errorMessage);
    return QString();
  }
  QFile file(inputFile);
  if (!file.open(QIODevice::ReadOnly)) {
    errorMessage = QObject::tr("ERROR: Couldn't open: %1").arg(inputFile);
    toConsole(errorMessage);
    return QString();
  }

  if (QFile::exists(outputFile)) {
    if (!QFile::remove(outputFile)) {
      toConsole(
          QObject::tr("Output file already exists an couldn't be deleted: %1").arg(outputFile));
      return QString();
    }
    toConsole(" -" + QObject::tr("Deleted existing %1.").arg(outputFile));
  }
  QString content = file.readAll();
  file.close();
  content = content.replace("\r\n", "\n");
  return content;
}

void getPlayRes(int &width, int &height, const QString content)
{

  //width,height,translation_x,layer,translation_y
  QString tmp = content;
  width = 720, height = 576;
  int index = tmp.indexOf("PlayResX:");
  if (index != -1) {
    tmp = tmp.remove(0, index + 10);
    tmp = tmp.remove(tmp.indexOf("\n"), tmp.size());
    index = tmp.toInt();
    if (index > 0) {
      width = index;
    }
  }
  tmp = content;
  index = tmp.indexOf("PlayResY:");
  if (index != -1) {
    tmp = tmp.remove(0, index + 10);
    tmp = tmp.remove(tmp.indexOf("\n"), tmp.size());
    index = tmp.toInt();
    if (index > 0) {
      height = index;
    }
  }
}

QDomElement getTextStreamHeader(QDomElement &parent, QDomDocument &dom, int width, int height)
{
  QDomElement textHead = dom.createElement("TextStreamHeader");
  textHead.setAttribute("width", QString::number(width));
  textHead.setAttribute("height", QString::number(height));
  textHead.setAttribute("layer", "65535");
  textHead.setAttribute("translation_x", "0");
  textHead.setAttribute("translation_y", "0");
  parent.appendChild(textHead);
  return textHead;
}

QDomElement getTextSampleDescription(QDomElement &parent, QDomDocument &dom,
                                     QString horizontalJustification, QString verticalJustification,
                                     QString name)
{
  QDomElement textDescription = dom.createElement("TextSampleDescription");
  QStringList list = QString("center,bottom,00 00 00 00,no,no,no,None").split(",");
  textDescription.setAttribute("name", name);
  textDescription.setAttribute("horizontalJustification", horizontalJustification);
  textDescription.setAttribute("verticalJustification", verticalJustification);
  textDescription.setAttribute("backColor", "00 00 00 00");
  textDescription.setAttribute("verticalText", list.at(3));
  textDescription.setAttribute("fillTextRegion", list.at(4));
  textDescription.setAttribute("continuousKaraoke", list.at(5));
  textDescription.setAttribute("scroll", list.at(6));
  textDescription.setAttribute("wrap", "Automatic");
  parent.appendChild(textDescription);
  return textDescription;
}

QDomElement getFontTable(QDomDocument &dom, QDomElement &textDescription)
{
  QDomElement fontTable = dom.createElement("FontTable");
  textDescription.appendChild(fontTable);
  return fontTable;
}

void fillFontTable(QDomDocument &dom, QDomElement &fontTable, QStringList style)
{
  QDomElement child;
  child = dom.createElement("FontTableEntry");
  child.setAttribute("fontID", style.at(0));
  child.setAttribute("fontName", style.at(1));
  fontTable.appendChild(child);
}

void fillFontTable(QDomDocument &dom, QDomElement &fontTable, QHash<QString, QStringList> styles)
{
  QDomElement child;
  QStringList style, elems = styles.keys();
  elems.sort();
  foreach(QString name, elems) {
    style = styles.value(name);
    child = dom.createElement("FontTableEntry");
    child.setAttribute("fontName", style.at(1));
    child.setAttribute("fontID", style.at(0));
    fontTable.appendChild(child);
  }
}

int saveTextTo(QString text, QString to)
{
  if (text.isEmpty()) {
    toConsole(QObject::tr("Failed: saveTextTo %1 called with empty text").arg(to));
    return -1;
  }
  QFile file(to);
  file.remove();
  if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QTextStream out(&file);
    out.setCodec(QTextCodec::codecForUtfText(text.toUtf8()));
    out << text;
    if (file.exists()) {
      file.close();
      toConsole("-> " + QObject::tr("Saved content to %1").arg(to));
      return 0;
    }
  }
  toConsole(QObject::tr("Failed to saveTextTo %1").arg(to));
  return -1;
}

void setStyleForTextDescription(QDomDocument &dom, QDomElement &textDescription, QStringList style,
                                QString name)
{
  QDomElement child;
  //styleID, fontName, Styles, fontsize, fontColor, backColor
  child = createStyleNode(dom, style, name);
  textDescription.appendChild(child);
}

/**
 * top : specifies vertical offset from stream main display rectangle top edge (type: signed integer). Default value: 0 pixels.
 * left : specifies horizontal offset from stream main display rectangle left edge (type: signed integer). Default value: 0 pixels.
 * bottom : specifies vertical extend of the text box (type: signed integer). Default value: 0 pixels.
 * right : specifies horizontal extend of the text box (type: signed integer). Default value: 0 pixels.
 */
void setTextBox(QDomDocument &dom, QDomElement &textDescription, int width, int height)
{
  QDomElement child = dom.createElement("TextBox");
  child.setAttribute("top", "0");
  child.setAttribute("left", "0");
  child.setAttribute("bottom", QString::number(height));
  child.setAttribute("right", QString::number(width));
  textDescription.appendChild(child);
}

double getScriptVersion(QString content, int index)
{
  QString line = content.remove(0, index + 13);
  line = line.remove(line.indexOf("\n"), line.size());
  line = line.trimmed();
  index = line.indexOf("+");
  if (index != -1) {
    line = line.remove(index, line.size());
  }
  return line.toDouble();
}
int lineCount = 0;

QString getTextFromLine(QString line)
{
  QStringList elements = line.split(",");
  QString text = elements.at(9).trimmed();
  int count = elements.count();
  if (count > 10) {
    for (int i = 10; i < count; ++i) {
      text += ", " + elements.at(i).trimmed();
    }
  }

  return text;
  //return "testing " + QString::number(++lineCount);
}

//horizontalOriantations << "center" << "left" << "right";
//verticalOriantations << "bottom" << "center" << "top" ;
void setHorizontalAndVerticalForIndex(int index, QString &vertical, QString &horizontal)
{
  switch (index) {
    case 1 :
      vertical = "bottom";
      horizontal = "left";
      break;
    case 2 :
      vertical = "bottom";
      horizontal = "center";
      break;
    case 3 :
      vertical = "bottom";
      horizontal = "right";
      break;
    case 4 :
      vertical = "center";
      horizontal = "left";
      break;
    case 5 :
      vertical = "center";
      horizontal = "center";
      break;
    case 6 :
      vertical = "center";
      horizontal = "right";
      break;
    case 7 :
      vertical = "top";
      horizontal = "left";
      break;
    case 8 :
      vertical = "top";
      horizontal = "center";
      break;
    case 9 :
      vertical = "top";
      horizontal = "right";
      break;
    default :
      vertical = "bottom";
      horizontal = "center";
      break;
  }
}

void addTextSample(QDomDocument &dom, QDomElement &parent, QHash<QString, QStringList> styles
                   , QString time, QString line
                   , const QHash<QString, int> &orientationStyleToIndexMapping)
{

  QString nextStartTime, nextEndTime;
  QDomElement textSample = dom.createElement("TextSample");
  textSample.setAttribute("sampleTime", time);
  int index = -1;
  QString sampleDescriptionIndex, key, sIndex;
  QString horizontal = "center", vertical = "bottom";
  QString withoutStyling;
  if (!line.isEmpty()) {
    QString styleName = line.split(",").at(3);
    QString text = getTextFromLine(line);
    withoutStyling = text;
    removeLineStyling(withoutStyling);
    if (!withoutStyling.isEmpty()) {
      if (withoutStyling != text) {
        index = text.indexOf("\\an");
        if (index != -1) {
          sIndex = text.at(index + 3);
          setHorizontalAndVerticalForIndex(sIndex.toInt(), vertical, horizontal);
        }
      }
      withoutStyling = withoutStyling.replace("\\N", "\n");
      key = styleName;
      key += "_";
      key += horizontal;
      key += "_";
      key += vertical;
      index = orientationStyleToIndexMapping.value(key);
      if (index == 0) {
        index = 1;
      }
      sampleDescriptionIndex = QString::number(index);
      textSample.setAttribute("text", withoutStyling);
      textSample.setAttribute("sampleDescriptionIndex", sampleDescriptionIndex);
    }
  }
  textSample.setAttribute("xml:space", "preserve");
  parent.appendChild(textSample);
}

void addTextSamplesForContent(QDomDocument &dom, QDomElement &parent, QString content,
                              QHash<QString, QStringList> styles
                              , const QHash<QString, int> &orientationStyleToIndexMapping)
{
  QString startTime, endTime = "00:00:00.000", line, text;
  addTextSample(dom, parent, styles, endTime, QString(), orientationStyleToIndexMapping);
  content = content.remove(0, content.indexOf("Dialogue:")).trimmed();
  QStringList elements, lines = content.split("\n");
  lines.sort();

  lines = cleanUpAssForTimes(lines, true);
  for (int j = 0, c = lines.count(); j < c; ++j) { //bei ass Style Namen Herausfiltern
    line = lines.at(j).trimmed();
    elements = line.split(",");
    startTime = elements.at(1).trimmed();
    adjustTime(startTime);
    endTime = elements.at(2).trimmed();
    adjustTime(endTime);
    addTextSample(dom, parent, styles, startTime, line, orientationStyleToIndexMapping);
  }
}

QStringList getNeededDescriptions(const QString &content)
{

  QString dialog = content;
  dialog = dialog.remove(0, dialog.indexOf("Dialogue:")).trimmed();
  QStringList elements, lines = dialog.split("\n"), neededKeys;
  lines.sort();
  lines = cleanUpAssForTimes(lines);
  QString line, horizontal = "center", vertical = "bottom", withoutStyling, usedDescription, sIndex;
  QString key, text, styleName;
  int index = -1;
  for (int j = 0, c = lines.count(); j < c; ++j) { //bei ass Style Namen Herausfiltern
    line = lines.at(j).trimmed();
    if (line.isEmpty()) {
      continue;
    }
    elements = line.split(",");
    styleName = elements.at(3);
    text = getTextFromLine(line);
    withoutStyling = text;
    removeLineStyling(withoutStyling);
    if (!withoutStyling.isEmpty()) {
      if (withoutStyling != text) {
        index = text.indexOf("\\an");
        if (index != -1) {
          sIndex = text.at(index + 3);
          setHorizontalAndVerticalForIndex(sIndex.toInt(), vertical, horizontal);
        }
      }
      key = styleName;
      key += "_";
      key += horizontal;
      key += "_";
      key += vertical;
      neededKeys << key;
    }
  }
  return neededKeys;
}

int convertTextSubsToTtxt(QString inputFile, QString outputFile)
{
  if (!inputFile.endsWith(".ass", Qt::CaseInsensitive)) {
    toConsole(QObject::tr("Input needs to be .ass!"));
    return -1;
  }
  QString content = readInputDeleteOutput(inputFile, outputFile);
  if (content.isEmpty()) {
    return -1;
  }
  QStringList neededDescriptions = getNeededDescriptions(content);

  QDomDocument dom;
  QDomElement root = getXmlHeader(dom);

  //styleID, fontName, Styles, fontsize, fontColor, backColor
  QHash<QString, QStringList> styles = grabbingStylingInfos(content);

  //TextHeader start
  int width, height;
  getPlayRes(width, height, content);
  QDomElement textHead = getTextStreamHeader(root, dom, width, height);
  QStringList horizontalOriantations;
  horizontalOriantations << "center" << "left" << "right";
  QStringList verticalOriantations, styleElements;
  verticalOriantations << "bottom" << "center" << "top";
  QDomElement textDescription, fontTable;
  QString name;
  int index = 0;
  QHash<QString, int> orientationStyleToIndexMapping;
  foreach(QString styleName, styles.keys()) {
    styleElements = styles.value(styleName);
    foreach(QString horizontal, horizontalOriantations) {
      foreach(QString vertical, verticalOriantations) {
        name = styleName;
        name += "_";
        name += horizontal;
        name += "_";
        name += vertical;
        if (neededDescriptions.contains(name)) {
          textDescription = getTextSampleDescription(textHead, dom, horizontal, vertical, name);
          setTextBox(dom, textDescription, width, height);
          fontTable = getFontTable(dom, textDescription);
          fillFontTable(dom, fontTable, styleElements);
          setStyleForTextDescription(dom, textDescription, styleElements, styleName);
          orientationStyleToIndexMapping.insert(name, ++index);
        }
      }
    }
  }
  //TextHeader finish
  toConsole(QObject::tr("Generating content,.."));

  //TTXT CONTENT SECTION
  index = content.indexOf("ScriptType: v");
  if (index == -1) {
    toConsole(QObject::tr("Couldn't find ScriptType version,.."));
    return -1;
  }
  double version = getScriptVersion(content, index);
  if (version != 4) {
    toConsole(QObject::tr("Unsupported .ass ScriptType: %1").arg(version));
    return -1;
  }
  addTextSamplesForContent(dom, root, content, styles, orientationStyleToIndexMapping);
  cerr << endl;
  toConsole(QObject::tr("Finished generating content."));
  return saveTextTo(dom.toString(0), QDir::toNativeSeparators(outputFile));
}
int main(int argc, char *argv[])
{
  QString version = "2012.01.27.2";
  cerr << "assToTtxt by Selur (http://forum.selur.de), version ";
  cerr << qPrintable(version) << endl;
  if (argc < 2) {
    cerr << "  usage: assToTtxt inputfile <outputfile>" << endl;
    return -1;
  }
  cerr << endl;
  QString inputFile = argv[1];
  QString outputFile;
  int finished = -1;
  if (argc == 2) {
    outputFile = inputFile;
    outputFile.remove(outputFile.lastIndexOf("."), outputFile.size());
    outputFile += ".ttxt";
    finished = convertTextSubsToTtxt(inputFile, outputFile);
  } else {
    finished = convertTextSubsToTtxt(inputFile, argv[2]);
  }
  inputFile = inputFile.remove(inputFile.lastIndexOf("."), inputFile.size());
  inputFile += ".log";
  saveTextTo(consoleOutput, inputFile);
  return finished;
}

