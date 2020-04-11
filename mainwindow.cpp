/**********************************************************************
 *  MainWindow.cpp
 **********************************************************************
 * Copyright (C) 2017 MX Authors
 *
 * Authors: Adrian
 *          MX Linux <http://mxlinux.org>
 *
 * This file is part of mx-conky.
 *
 * mx-conky is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mx-conky is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mx-conky.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/


#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "version.h"

#include <QColorDialog>
#include <QFileDialog>
#include <QTextEdit>
#include <QSettings>
#include <QRegularExpression>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent, QString file) :
    QDialog(parent),
    ui(new Ui::MainWindow)
{
    if (QProcessEnvironment::systemEnvironment().value("DEBUG").length() > 0)
        debug = true;
    else {
        debug = false;
    }

    qDebug().noquote() << QCoreApplication::applicationName() << "version:" << VERSION;
    ui->setupUi(this);
    setWindowFlags(Qt::Window); // for the close, min and max buttons

    connect(qApp, &QApplication::aboutToQuit, this, &MainWindow::cleanup);

    file_name = file;
    this->setWindowTitle(tr("MX Conky"));

    // set regexp pattern
    lua_format = "^\\s*(--|conky.config\\s*=\\s*{|conky.text\\s*=\\s*{)";
    lua_config = "^\\s*(conky.config\\s*=\\s*{";
    old_format = "^\\s*#|^TEXT$";
    lua_comment_line ="^\\s*--";
    lua_comment_start = "^\\s*--\\[\\[";
    lua_comment_end = "^\\s*\\]\\]";
    old_comment_line = "^\\s*#";
    capture_lua_color = "^(?<before>(.*\\]\\])?\\s*(?<color_item>default_color|color\\d)(?:\\s*=\\s*[\\\"\\']))(?:#?)(?<color_value>[[:alnum:]]+)(?<after>(?:[\\\"\\']).*)";
    capture_old_color = "^(?<before>\\s*(?<color_item>default_color|color\\d)(?:\\s+))(?:#?)(?<color_value>[[:alnum:]]+)(?<after>.*)";


    refresh();
    QSettings settings("mx-conky");
    restoreGeometry(settings.value("geometery").toByteArray());
}

MainWindow::~MainWindow()
{
    delete ui;
}

// detect conky format old or lua
void MainWindow::detectConkyFormat()
{
    QRegularExpression lua_format_regexp(lua_format);
    QRegularExpression old_format_regexp(old_format);

    const QStringList list = file_content.split("\n");
    if (debug) qDebug() << "Detecting conky format: " + file_name ;

    conky_format_detected = false;

    for (const QString &row : list) {
        QRegularExpressionMatch lua_format_match = lua_format_regexp.match(row);
        if (lua_format_match.hasMatch()) {
            is_lua_format = true;
            conky_format_detected = true;
            if (debug) qDebug() << "Conky format detected 'lua-format' :" + file_name ;
            break;
        }
        QRegularExpressionMatch old_format_match = old_format_regexp.match(row);
        if (old_format_match.hasMatch()) {
            is_lua_format = false;
            conky_format_detected = true;
            if (debug) qDebug() << "Conky format detected 'old-format' :" + file_name ;
            break;
        }
    }
}


// find defined colors in the config file
void MainWindow::parseContent()
{

    QRegularExpression regexp_old_comment_line(old_comment_line);
    QRegularExpression regexp_lua_comment_line(lua_comment_line);
    QRegularExpression regexp_lua_comment_start(lua_comment_start);
    QRegularExpression regexp_lua_comment_end(lua_comment_end);

    QRegularExpression regexp_lua_color(capture_lua_color);
    QRegularExpression regexp_old_color(capture_old_color);

    QRegularExpressionMatch match_color;
    QRegularExpression regexp_color;
    QRegularExpression regexp_comment_line;
    bool own_window_hints_found = false;

    QString comment_sep;
//    QString block_comment_start = "--[[";
//    QString block_comment_end = "]]";

    if (is_lua_format) {
        regexp_color = regexp_lua_color;
        regexp_comment_line = regexp_lua_comment_line;
        comment_sep = "--";
    } else {
        regexp_color = regexp_old_color;
        regexp_comment_line = regexp_old_comment_line;
        comment_sep = "#";
    }


    const QStringList list = file_content.split("\n");

    if (debug) qDebug() << "Parsing content: " + file_name ;

    bool lua_block_comment = false;
//  bool lua_config = false;
    for (const QString &row : list) {
        // lua comment block
        QString trow = row.trimmed();
        if (is_lua_format) {
            if (lua_block_comment) {
                if (trow.endsWith(block_comment_end)) {
                    lua_block_comment = false;
                    if (debug) qDebug() << "Lua block comment end 'ENDS WITH LINE' found" ;
                    continue;
                }
                if (trow.contains(block_comment_end)) {
                    lua_block_comment = false;
                    QStringList ltrow = trow.split(block_comment_end);
                    ltrow.removeFirst();
                    trow = ltrow.join(block_comment_end);
                    trow = trow.trimmed();
                    if (debug)  qDebug() << "Lua block comment end CONTAINS line found: after ]]: " << trow ;
                } else {
                    continue;
                }
            }
            if (not lua_block_comment) {
                if (trow.startsWith(block_comment_start)) {
                   if (debug)  qDebug() << "Lua block comment 'STARTS WITH LINE' found" ;
                    lua_block_comment = true;
                    continue;
                }
                if (trow.contains(block_comment_start)) {
                    lua_block_comment = true;
                    QStringList ltrow = trow.split(block_comment_start);
                    trow = ltrow[0];
                    trow = trow.trimmed();
                    if (debug) qDebug() << "Lua block comment start CONTAINS line found: before start --[[: " << trow ;
                }
            }

        }
        // comment line
        if (trow.startsWith(comment_sep)) {
            continue;
        }
        if (trow.contains(comment_sep)) {
            trow = trow.split(comment_sep)[0];
        }

        // color line
        match_color = regexp_color.match(trow);
        if (match_color.hasMatch()) {
            QString color_item = match_color.captured("color_item");
            QString color_value = match_color.captured("color_value");

            if (color_item == "default_color") {
                setColor(ui->widgetDefaultColor, strToColor(color_value));
            } else if (color_item == "color0") {
                setColor(ui->widgetColor0, strToColor(color_value));
            } else if (color_item == "color1") {
                setColor(ui->widgetColor1, strToColor(color_value));
            } else if (color_item == "color2") {
                setColor(ui->widgetColor2, strToColor(color_value));
            } else if (color_item == "color3") {
                setColor(ui->widgetColor3, strToColor(color_value));
            } else if (color_item == "color4") {
                setColor(ui->widgetColor4, strToColor(color_value));
            } else if (color_item == "color5") {
                ui->labelColor5->setText(ui->labelColor0->text().replace('0','5'));
                setColor(ui->widgetColor5, strToColor(color_value));
            } else if (color_item == "color6") {
                ui->labelColor6->setText(ui->labelColor0->text().replace('0','6'));
                setColor(ui->widgetColor6, strToColor(color_value));
            } else if (color_item == "color7") {
                ui->labelColor7->setText(ui->labelColor0->text().replace('0','7'));
                setColor(ui->widgetColor7, strToColor(color_value));
            } else if (color_item == "color8") {
                ui->labelColor8->setText(ui->labelColor0->text().replace('0','8'));
                setColor(ui->widgetColor8, strToColor(color_value));
            } else if (color_item == "color9") {
                ui->labelColor9->setText(ui->labelColor0->text().replace('0','9'));
                setColor(ui->widgetColor9, strToColor(color_value));
            }
            continue;
        }

        // Desktop config
        if (trow.startsWith("own_window_hints")) {
            own_window_hints_found = true;
            if (debug) qDebug() << "own_window_hints line found: " << trow ;
            if (trow.contains("sticky")) {
                ui->radioAllDesktops->setChecked(true);
            } else {
                ui->radioDesktop1->setChecked(true);
            }
            continue;
        }

        // Day/Month format
        if (trow.contains("%A")) {
            ui->radioButtonDayLong->setChecked(true);
        } else if (row.contains("%a")) {
            ui->radioButtonDayShort->setChecked(true);
        }
        if (row.contains("%B")) {
            ui->radioButtonMonthLong->setChecked(true);
        } else if (row.contains("%b")) {
            ui->radioButtonMonthShort->setChecked(true);
        }
    }
    if(not own_window_hints_found) {
        ui->radioDesktop1->setChecked(true);
    }
}

bool MainWindow::checkConkyRunning()
{
    int ret = system("sleep 0.3; pgrep -u $USER -x conky >/dev/null 2>&1");
    if (debug) qDebug() << system("echo pgrep -u $USER -x conky : ") << ret;
    //if (system("pgrep -u $USER -x conky >/dev/null 2>&1 ") == 0) {
    if (ret == 0) {
        ui->buttonToggleOn->setText("Stop");
        ui->buttonToggleOn->setIcon(QIcon::fromTheme("stop"));
        return true;
    } else {
        ui->buttonToggleOn->setText("Run");
        ui->buttonToggleOn->setIcon(QIcon::fromTheme("start"));
        return false;
    }
}

// read config file
bool MainWindow::readFile(QString file_name)
{
    QFile file(file_name);
    if(!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open file: " << file.fileName();
        return false;
    }
    file_content = file.readAll().trimmed();
    file.close();
    return true;
}

// convert string to QColor
QColor MainWindow::strToColor(QString colorstr)
{
    QColor color(colorstr);
    if (!color.isValid()) { // if color is invalid assume RGB values and add a # in front of the string
        color.setNamedColor("#" + colorstr);
    }
    return color;
}

// refresh windows content
void MainWindow::refresh()
{
    modified = false;

    // hide all color frames by default, display only the ones in the config file
    const QList<QWidget *> frames({ui->frameDefault, ui->frame0, ui->frame1, ui->frame2,
                                   ui->frame3, ui->frame4, ui->frame5, ui->frame6,
                                   ui->frame7, ui->frame8, ui->frame9});
    for (QWidget *w : frames) {
        w->hide();
    }
    // draw borders around color widgets
    const QList<QWidget *> widgets({ui->widgetDefaultColor, ui->widgetColor0, ui->widgetColor1, ui->widgetColor2,
                                    ui->widgetColor3, ui->widgetColor4, ui->widgetColor5, ui->widgetColor6,
                                    ui->widgetColor7, ui->widgetColor8, ui->widgetColor9});
    for (QWidget *w : widgets) {
        w->setStyleSheet("border: 1px solid black");
    }

    QString conky_name = QFileInfo(file_name).fileName();
    ui->buttonChange->setText(conky_name);

    checkConkyRunning();

    QFile(file_name + ".bak").remove();
    QFile::copy(file_name, file_name + ".bak");

    if (readFile(file_name)) {
        detectConkyFormat();
        parseContent();
    }
    this->adjustSize();
}

void MainWindow::saveBackup()
{
    if (modified) {
        int ans = QMessageBox::question(this, "Backup config file" , "Do you want to preserve the original file?");
        if (ans == QMessageBox::Yes) {
            QString time_stamp = cmd.getCmdOut("date +%y%m%d_%H%m%S");
            QFileInfo fi(file_name);
            QString new_name = fi.canonicalPath() + "/" + fi.baseName() + "_" + time_stamp;
            if (fi.completeSuffix().length() > 0) new_name += "." + fi.completeSuffix();
            QFile::copy(file_name + ".bak", new_name);
            QMessageBox::information(this, "Backed up config file", "The original configuration was backed up to " + new_name);
        }
    }
    QFile(file_name + ".bak").remove();
}

// write color change back to the file
void MainWindow::writeColor(QWidget *widget, QColor color)
{
    QRegularExpression regexp_old_comment_line(old_comment_line);
    QRegularExpression regexp_lua_comment_line(lua_comment_line);
    QRegularExpression regexp_lua_comment_start(lua_comment_start);
    QRegularExpression regexp_lua_comment_end(lua_comment_end);

    QRegularExpression regexp_lua_color(capture_lua_color);
    QRegularExpression regexp_old_color(capture_old_color);

    QRegularExpressionMatch match_color;
    QRegularExpression regexp_color;
    QRegularExpression regexp_comment_line;

    QString comment_sep;
//    QString block_comment_start = "--[[";
//    QString block_comment_end = "]]";

    if (is_lua_format) {
        regexp_color = regexp_lua_color;
        regexp_comment_line = regexp_lua_comment_line;
        comment_sep = "--";
    } else {
        regexp_color = regexp_old_color;
        regexp_comment_line = regexp_old_comment_line;
        comment_sep = "#";
    }

    QString color_name;

    QString item_name;
    if (widget->objectName() == "widgetDefaultColor") {
        item_name = "default_color";
    } else if (widget->objectName() == "widgetColor0") {
        item_name = "color0";
    } else if (widget->objectName() == "widgetColor1") {
        item_name = "color1";
    } else if (widget->objectName() == "widgetColor2") {
        item_name = "color2";
    } else if (widget->objectName() == "widgetColor3") {
        item_name = "color3";
    } else if (widget->objectName() == "widgetColor4") {
        item_name = "color4";
    } else if (widget->objectName() == "widgetColor5") {
        item_name = "color5";
    } else if (widget->objectName() == "widgetColor6") {
        item_name = "color6";
    } else if (widget->objectName() == "widgetColor7") {
        item_name = "color7";
    } else if (widget->objectName() == "widgetColor8") {
        item_name = "color8";
    } else if (widget->objectName() == "widgetColor9") {
        item_name = "color9";
    }

    const QStringList list = file_content.split("\n");
    QStringList new_list;
    bool lua_block_comment = false;
    for (const QString &row : list) {
        // lua comment block
        QString trow = row.trimmed();
        if (is_lua_format) {
            if (lua_block_comment) {
                if (trow.endsWith(block_comment_end)) {
                    lua_block_comment = false;
                    if (debug) qDebug() << "Lua block comment end 'ENDS WITH LINE' found" ;
                    new_list << row;
                    continue;
                }
                if (trow.contains(block_comment_end)) {
                    lua_block_comment = false;
                    QStringList ltrow = trow.split(block_comment_end);
                    ltrow.removeFirst();
                    trow = ltrow.join(block_comment_end);
                    trow = trow.trimmed();
                    if (debug) qDebug() << "Lua block comment end CONTAINS line found: after ]]: " << trow ;
                } else {
                    new_list << row;
                    continue;
                }
            }
            if (not lua_block_comment) {
                if (trow.startsWith(block_comment_start)) {
                    if (debug) qDebug() << "Lua block comment 'STARTS WITH LINE' found" ;
                    lua_block_comment = true;
                    new_list << row;
                    continue;
                }
                if (trow.contains(block_comment_start)) {
                    lua_block_comment = true;
                    QStringList ltrow = trow.split(block_comment_start);
                    trow = ltrow[0];
                    trow = trow.trimmed();
                    if (debug) qDebug() << "Lua block comment start CONTAINS line found: before start --[[: " << trow ;
                }
            }

        }
        // comment line
        if (trow.startsWith(comment_sep)) {
            new_list << row;
            continue;
        }

        match_color = regexp_color.match(row);
        if ( match_color.hasMatch() and match_color.captured("color_item") == item_name ) {
            color_name = color.name();
            if (!is_lua_format) {
                color_name = color.name().remove('#');
            }
            new_list << match_color.captured("before") + color_name + match_color.captured("after");
        } else {
            new_list << row;
            continue;
        }
    }
    file_content = new_list.join("\n").append("\n");
    writeFile(file_name, file_content);
}

// write new content to file
void MainWindow::writeFile(QString file_name, QString content)
{
    QFile file(file_name);
    if (file.open(QIODevice::WriteOnly)) {
        QTextStream out(&file);
        out << content;
        file.close();
        modified = true;
    } else {
        qDebug() << "Error opening file " + file_name  + " for output";
    }
}


// cleanup environment when window is closed
void MainWindow::cleanup()
{
    saveBackup();

//    if(!cmd->terminate()) {
//        cmd->kill();
//    }
}

void MainWindow::pickColor(QWidget *widget)
{
    QColorDialog *dialog = new QColorDialog();
    QColor color = dialog->getColor(widget->palette().color(QWidget::backgroundRole()));
    if (color.isValid()) {
        setColor(widget, color);
        writeColor(widget, color);
    }
}

void MainWindow::setColor(QWidget *widget, QColor color)
{
    widget->parentWidget()->show();
    if (color.isValid()) {
        QPalette pal = palette();
        pal.setColor(QPalette::Background, color);
        widget->setAutoFillBackground(true);
        widget->setPalette(pal);
    }
}


void MainWindow::cmdStart()
{
    setCursor(QCursor(Qt::BusyCursor));
}


void MainWindow::cmdDone()
{
 //   ui->progressBar->setValue(ui->progressBar->maximum());
    setCursor(QCursor(Qt::ArrowCursor));
    //cmd->disconnect();
}

// set proc and timer connections
void MainWindow::setConnections()
{
//    connect(cmd, &Cmd::started, this, &MainWindow::cmdStart);
//    connect(cmd, &Cmd::finished, this, &MainWindow::cmdDone);
}

// About button clicked
void MainWindow::on_buttonAbout_clicked()
{
    this->hide();
    QString url = "file:///usr/share/doc/mx-conky/license.html";
    QMessageBox msgBox(QMessageBox::NoIcon,
                       tr("About MX Conky"), "<p align=\"center\"><b><h2>" +
                       tr("MX Conky") + "</h2></b></p><p align=\"center\">" + tr("Version: ") + VERSION + "</p><p align=\"center\"><h3>" +
                       tr("GUI program for configuring Conky in MX Linux") +
                       "</h3></p><p align=\"center\"><a href=\"http://mxlinux.org\">http://mxlinux.org</a><br /></p><p align=\"center\">" +
                       tr("Copyright (c) MX Linux") + "<br /><br /></p>");
    QPushButton *btnLicense = msgBox.addButton(tr("License"), QMessageBox::HelpRole);
    QPushButton *btnChangelog = msgBox.addButton(tr("Changelog"), QMessageBox::HelpRole);
    QPushButton *btnCancel = msgBox.addButton(tr("Cancel"), QMessageBox::NoRole);
    btnCancel->setIcon(QIcon::fromTheme("window-close"));

    msgBox.exec();

    if (msgBox.clickedButton() == btnLicense) {
        if (system("command -v mx-viewer") == 0) {
            system("mx-viewer " + url.toUtf8() + "&");
        } else {
            system("xdg-open " + url.toUtf8());
        }
    } else if (msgBox.clickedButton() == btnChangelog) {
        QDialog *changelog = new QDialog(this);
        changelog->setWindowTitle(tr("Changelog"));
        changelog->resize(600, 500);

        QTextEdit *text = new QTextEdit;
        text->setReadOnly(true);
        Cmd cmd;
        text->setText(cmd.getCmdOut("zless /usr/share/doc/" + QFileInfo(QCoreApplication::applicationFilePath()).fileName()  + "/changelog.gz"));

        QPushButton *btnClose = new QPushButton(tr("&Close"));
        btnClose->setIcon(QIcon::fromTheme("window-close"));
        connect(btnClose, &QPushButton::clicked, changelog, &QDialog::close);

        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget(text);
        layout->addWidget(btnClose);
        changelog->setLayout(layout);
        changelog->exec();
    }
    this->show();
}

// Help button clicked
void MainWindow::on_buttonHelp_clicked()
{
    QString url = "/usr/share/doc/mx-conky/help/mx-conky.html";
    QString cmd;
    if (system("command -v mx-viewer") == 0) {
        cmd = QString("mx-viewer " + url + " " + tr("MX Conky Help") + "&");
    } else {
        cmd = QString("xdg-open " + url);
    }
    system(cmd.toUtf8());
}


void MainWindow::on_buttonDefaultColor_clicked()
{
    pickColor(ui->widgetDefaultColor);
}

void MainWindow::on_buttonColor0_clicked()
{
    pickColor(ui->widgetColor0);
}

void MainWindow::on_buttonColor1_clicked()
{
    pickColor(ui->widgetColor1);
}

void MainWindow::on_buttonColor2_clicked()
{
    pickColor(ui->widgetColor2);
}

void MainWindow::on_buttonColor3_clicked()
{
    pickColor(ui->widgetColor3);
}

void MainWindow::on_buttonColor4_clicked()
{
    pickColor(ui->widgetColor4);
}

void MainWindow::on_buttonColor5_clicked()
{
    pickColor(ui->widgetColor5);
}
void MainWindow::on_buttonColor6_clicked()
{
    pickColor(ui->widgetColor6);
}
void MainWindow::on_buttonColor7_clicked()
{
    pickColor(ui->widgetColor7);
}
void MainWindow::on_buttonColor8_clicked()
{
    pickColor(ui->widgetColor8);
}
void MainWindow::on_buttonColor9_clicked()
{
    pickColor(ui->widgetColor9);
}


// start-stop conky
void MainWindow::on_buttonToggleOn_clicked()
{
    if (checkConkyRunning()) {
        system("pkill -u $USER -x conky");
    } else {
        system("cd $(dirname '" + file_name.toUtf8() + "'); conky -c '" + file_name.toUtf8() + "' & ");
    }
    checkConkyRunning();
}


void MainWindow::on_buttonRestore_clicked()
{
    if (QFile(file_name + ".bak").exists()) {
        QFile(file_name).remove();
    }
    if (QFile::copy(file_name + ".bak", file_name)) {
        refresh();
    }
}

void MainWindow::on_buttonEdit_clicked()
{
    this->hide();
    QByteArray editor;
    Cmd cmd;

    QString run = "set -o pipefail; ";
    run += "XDHD=${XDG_DATA_HOME:-$HOME/.local/share}:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}; ";
    run += "eval grep -sh -m1 ^Exec {${XDHD//://applications/,}/applications/}";
    run += "$(xdg-mime query default text/plain)  2>/dev/null ";
    run += "| head -1 | sed 's/^Exec=//' | tr -d '\"' | tr -s ' ' | sed 's/@@//g; s/%f//I' ";
    bool quiet = true;
    if (debug) qDebug() << "run-cmd: " << run;
    if (debug) quiet = false;
    bool error = cmd.run(run, editor, quiet);
    if (debug) qDebug() << "run:'" + editor + " '" + file_name.toUtf8() + "'";
    if (system(editor + " '" + file_name.toUtf8() + "'") != 0) {
        if (error || (system("which " + editor + " 1>/dev/null") != 0)) {
            if (debug) qDebug() << "no default text editor defined" << editor;
            // try featherpad explicitly
            if (system("which featherpad 1>/dev/null") == 0) {
                if (debug) qDebug() << "try featherpad text editor " ;
                system("featherpad '" + file_name.toUtf8() + "'");
            }
            refresh();
            this->show();
            return;
        }

//    if (system(editor + " '" + file_name.toUtf8() + "'") != 0) {
//       qDebug() << "no default text editor defined";
    }
    refresh();
    this->show();
}

void MainWindow::on_buttonChange_clicked()
{
    QFileDialog dialog;

    saveBackup();

    QString selected = dialog.getOpenFileName(nullptr, QObject::tr("Select Conky Manager config file"), QFileInfo(file_name).path());
    if (selected != "") {
        file_name = selected;
    }
    refresh();
}

void MainWindow::on_radioDesktop1_clicked()
{
    QString comment_sep;
    QRegularExpression regexp_lua_owh(capture_lua_owh);
    QRegularExpression regexp_old_owh(capture_old_owh);
    QRegularExpression regexp_owh;
    QRegularExpressionMatch match_owh;
    QString detect_lua_config_end = "";
    QString detect_old_config_end = "^TEXT$";

    bool lua_block_comment = false;

    if (is_lua_format) {
        regexp_owh = regexp_lua_owh;
        comment_sep = "--";
    } else {
        regexp_owh = regexp_old_owh;
        comment_sep = "#";
    }

    const QStringList list = file_content.split("\n");
    QStringList new_list;
    for (const QString &row : list) {
        // lua comment block
        QString trow = row.trimmed();
        if (is_lua_format) {
            if (lua_block_comment) {
                if (trow.endsWith(block_comment_end)) {
                    lua_block_comment = false;
                    if (debug) qDebug() << "Lua block comment end 'ENDS WITH LINE' found" ;
                    new_list << row;
                    continue;
                }
                if (trow.contains(block_comment_end)) {
                    lua_block_comment = false;
                    QStringList ltrow = trow.split(block_comment_end);
                    ltrow.removeFirst();
                    trow = ltrow.join(block_comment_end);
                    trow = trow.trimmed();
                    if (debug) qDebug() << "Lua block comment end CONTAINS line found: after ]]: " << trow ;
                } else {
                    new_list << row;
                    continue;
                }
            }
            if (not lua_block_comment) {
                if (trow.startsWith(block_comment_start)) {
                    if (debug) qDebug() << "Lua block comment 'STARTS WITH LINE' found" ;
                    lua_block_comment = true;
                    new_list << row;
                    continue;
                }
                if (trow.contains(block_comment_start)) {
                    lua_block_comment = true;
                    QStringList ltrow = trow.split(block_comment_start);
                    trow = ltrow[0];
                    trow = trow.trimmed();
                    if (debug) qDebug() << "Lua block comment start CONTAINS line found: before start --[[: " << trow ;
                }
            }

        }
        // comment line
        if (trow.startsWith(comment_sep)) {
            new_list << row;
            continue;
        }

        if (not trow.startsWith("own_window_hints")) {
            new_list << row;
            continue;
        } else {

            if (debug) qDebug() << "on_radioDesktops1_clicked: own_window_hints found row : " << row ;
            if (debug) qDebug() << "on_radioDesktops1_clicked: own_window_hints found trow: " << trow ;
            match_owh = regexp_owh.match(row);
            if ( match_owh.hasMatch() and match_owh.captured("item") == "own_window_hints" ) {
                QString owh_value = match_owh.captured("value");
                owh_value.replace(",sticky","");
                owh_value.replace("sticky","");
                QString new_row = match_owh.captured("before") + owh_value + match_owh.captured("after");
                if (debug) qDebug() << "Removed sticky: " << new_row ;
                new_list << new_row;
            } else {
                if (debug) qDebug() << "ERROR : " << row ;
                if (is_lua_format) {
                    if (debug) qDebug() << "regexp_owh : " << regexp_lua_owh ;
                    if (debug) qDebug() << "regexp_owh : " << regexp_owh ;
                } else {
                    if (debug) qDebug() << "regexp_owh : " << regexp_old_owh ;
                }
                new_list << row;
            }
        }
    }

    file_content = new_list.join("\n").append("\n");
    writeFile(file_name, file_content);
}

void MainWindow::on_radioAllDesktops_clicked()
{
    QString comment_sep;
    QRegularExpression regexp_lua_owh(capture_lua_owh);
    QRegularExpression regexp_old_owh(capture_old_owh);
    QRegularExpression regexp_owh;
    QRegularExpressionMatch match_owh;
    QString conky_config_start ="conky.confg";
    QString conky_config_lua_end = "}";
    QString conky_config_old_end = "TEXT";
    QString conky_config_end;
    QString conky_sticky;
    bool lua_block_comment = false;
    bool conky_config;

    if (is_lua_format) {
        regexp_owh = regexp_lua_owh;
        comment_sep = "--";
        conky_config = false;
        conky_config_end = conky_config_lua_end;
        conky_sticky = "\town_window_hints = 'sticky',";
    } else {
        regexp_owh = regexp_old_owh;
        comment_sep = "#";
        conky_config = true;
        conky_config_end = conky_config_old_end;
        conky_sticky = "own_window_hints sticky";
    }

    bool found = false;
    const QStringList list = file_content.split("\n");
    QStringList new_list;
    QString trow;
    for (QString row : list) {
        trow = row.trimmed();
        if (is_lua_format) {
            if (lua_block_comment) {
                if (trow.endsWith(block_comment_end)) {
                    lua_block_comment = false;
                    if (debug) qDebug() << "Lua block comment end 'ENDS WITH LINE' found" ;
                    new_list << row;
                    continue;
                }
                if (trow.contains(block_comment_end)) {
                    lua_block_comment = false;
                    QStringList ltrow = trow.split(block_comment_end);
                    ltrow.removeFirst();
                    trow = ltrow.join(block_comment_end);
                    trow = trow.trimmed();
                    if (debug) qDebug() << "Lua block comment end CONTAINS line found: after ]]: " << trow ;
                } else {
                    new_list << row;
                    continue;
                }
            }
            if (not lua_block_comment) {
                if (trow.startsWith(block_comment_start)) {
                    if (debug) qDebug() << "Lua block comment 'STARTS WITH LINE' found" ;
                    lua_block_comment = true;
                    new_list << row;
                    continue;
                }
                if (trow.contains(block_comment_start)) {
                    lua_block_comment = true;
                    QStringList ltrow = trow.split(block_comment_start);
                    trow = ltrow[0];
                    trow = trow.trimmed();
                    if (debug) qDebug() << "Lua block comment start CONTAINS line found: before start --[[: " << trow ;
                }
            }
        }
        // comment line
        if (trow.startsWith(comment_sep)) {
            new_list << row;
            continue;
        }
        if (is_lua_format and trow.startsWith("conky.config")) {
            conky_config = true;
        }

        if (not found and conky_config and trow.startsWith(conky_config_end)) {
            conky_config = false;
            new_list << conky_sticky;
            new_list << row;
            continue;
        }
        if (not conky_config ) {
            new_list << row;
            continue;
        }

        if (not trow.startsWith("own_window_hints ")) {
            new_list << row;
            continue;
        } else {

            if (debug) qDebug() << "on_radioAllDesktops_clicked: own_window_hints found row : " << row ;
            if (debug) qDebug() << "on_radioAllDesktops_clicked: own_window_hints found trow: " << trow ;
            match_owh = regexp_owh.match(row);
            if ( match_owh.hasMatch() and match_owh.captured("item") == "own_window_hints" ) {
                QString owh_value = match_owh.captured("value");
                if (owh_value.length() == 0) {
                    owh_value = "sticky";
                } else {
                    owh_value.append(",sticky");
                }
                QString new_row = match_owh.captured("before") + owh_value + match_owh.captured("after");
                if (debug) qDebug() << "Append sticky: " << new_row ;

                new_list << match_owh.captured("before") + owh_value + match_owh.captured("after");
                found = true;
            } else {
                if (debug) qDebug() << "ERROR : " << row ;
                if (is_lua_format) {
                    if (debug) qDebug() << "regexp_owh : " << regexp_lua_owh ;
                } else {
                    if (debug) qDebug() << "regexp_owh : " << regexp_old_owh ;
                }
                found = false;
                new_list << row;
            }
        }
    }

    file_content = new_list.join("\n").append("\n");
    writeFile(file_name, file_content);
}

void MainWindow::on_radioButtonDayLong_clicked()
{
    file_content.replace("%a", "%A");
    writeFile(file_name, file_content);
}

void MainWindow::on_radioButtonDayShort_clicked()
{
    file_content.replace("%A", "%a");
    writeFile(file_name, file_content);
}

void MainWindow::on_radioButtonMonthLong_clicked()
{
    file_content.replace("%b", "%B");
    writeFile(file_name, file_content);
}

void MainWindow::on_radioButtonMonthShort_clicked()
{
    file_content.replace("%B", "%b");
    writeFile(file_name, file_content);
}

void MainWindow::on_buttonCM_clicked()
{
    this->hide();
    system("command -v conky-manager && conky-manager || command -v conky-manager2  && conky-manager2");

    this->show();
}

void MainWindow::closeEvent(QCloseEvent *)
{
    QSettings settings("mx-conky");
    settings.setValue("geometery", saveGeometry());
}
