/**********************************************************************
 *  main.cpp
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
#include <unistd.h>
#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QIcon>
#include <QDir>
#include <QFileDialog>
#include <QDebug>

#include <cmd.h>


// return the config file used for the newest conky process
QString getRunningConky()
{
    Cmd cmd;
    return cmd.getOutput("pgrep -xan conky | cut -d' ' -f4");
}

QString openFile(QDir dir)
{
    QFileDialog dialog;
    QString file_name = getRunningConky();

    if (file_name != "") {
        return file_name;
    }

    QString selected = dialog.getOpenFileName(0, QObject::tr("Select Conky Manager config file"), dir.path());
    if (selected != "") {
        return selected;
    }
    return "";
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon("/usr/share/pixmaps/mx-conky.png"));

    QTranslator qtTran;
    qtTran.load(QString("qt_") + QLocale::system().name());
    a.installTranslator(&qtTran);

    QTranslator appTran;
    appTran.load(QString("mx-conky_") + QLocale::system().name(), "/usr/share/mx-conky/locale");
    a.installTranslator(&appTran);

    if (system("dpkg -s conky-manager | grep -q 'Status: install ok installed'") != 0 &&
               system("dpkg -s conky-manager2 | grep -q 'Status: install ok installed'" ) != 0) {
        QMessageBox::critical(0, QObject::tr("Error"),
                              QObject::tr("Could not find conky-manager, please install it before running mx-conky"));
        return 1;
    }

    if (getuid() != 0) {

        QString dir = QDir::homePath() + "/.conky";
        if (!QDir(dir).exists()) {
            QDir().mkdir(dir);
        }

        // copy the mx-conky-data themes to the default folder
        system("cp -rn /usr/share/mx-conky-data/themes/* " + dir.toUtf8());

        QString file = openFile(dir);
        if (file == "") {
            QMessageBox::critical(0, QObject::tr("Error"),
                                  QObject::tr("No file was selected, quiting program"));
            return 1;
        }

        MainWindow w(0, file);
        w.show();
        return a.exec();

    } else {
        QApplication::beep();
        QMessageBox::critical(0, QString::null,
                              QObject::tr("You must run this program as normal user"));
        return 1;
    }
}
