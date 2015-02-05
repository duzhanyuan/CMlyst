/***************************************************************************
 *   Copyright (C) 2014-2015 Daniel Nicoletti <dantti12@gmail.com>         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; see the file COPYING. If not, write to       *
 *   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,  *
 *   Boston, MA 02110-1301, USA.                                           *
 ***************************************************************************/

#include "fileengine_p.h"

#include "page.h"
#include "menu.h"

#include <QRegularExpression>
#include <QDirIterator>
#include <QSettings>
#include <QDateTime>
#include <QHash>
#include <QStringBuilder>
#include <QDebug>

using namespace CMS;

FileEngine::FileEngine(QObject *parent) :
    Engine(parent),
    d_ptr(new FileEnginePrivate)
{
}

FileEngine::~FileEngine()
{
    delete d_ptr;
}

bool FileEngine::init(const QHash<QString, QString> &settings)
{
    Q_D(FileEngine);

    QString root = settings["root"];
    if (root.isEmpty()) {
        root = QDir::currentPath();
    }
    d->rootPath = root;
    d->pagesPath = root % QLatin1String("/pages");
    d->settingsInfo = d->rootPath.absoluteFilePath("site.conf");
    d->settings = new QSettings(d->settingsInfo.absoluteFilePath(), QSettings::IniFormat);

    if (!d->pagesPath.exists() &&
            !d->pagesPath.mkpath(d->pagesPath.absolutePath())) {
        qWarning() << "Failed to create pages path" << d->pagesPath.absolutePath();
        return false;
    }

    if (!d->settings->isWritable()) {
        qWarning() << "Settings file is not writable!" << d->settingsInfo.absoluteFilePath();
        return false;
    }

    return true;
}

Page *FileEngine::getPage(const QString &path)
{
    Q_D(FileEngine);

    QHash<QString, Page*>::const_iterator it = d->pages.constFind(path);
    if (it != d->pages.constEnd()) {
        QFileInfo fileInfo(d->pagesPath.absoluteFilePath(path));
        if (it.value()->modified() == fileInfo.lastModified()) {
            return it.value();
        }
    }

    Page *page = loadPage(path);
    if (page) {
        d->pages.insert(path, page);
        return page;
    }
    return 0;
}

Page *FileEngine::getPageToEdit(const QString &path) const
{
    Page *page = loadPage(path);
    if (!page) {
        page = new Page;
        page->setPath(path);
    }

    return page;
}

Page *FileEngine::loadPage(const QString &path) const
{
    Q_D(const FileEngine);

    QString normPath = path;
//    qDebug() << "getPageToEdit" << path;
    normPath.remove(QRegularExpression("^/"));
//    qDebug() << "getPageToEdit" << normPath;
    if (normPath.isEmpty()) {
        normPath = QStringLiteral("index.page");
    } else {
        normPath.append(QStringLiteral(".page"));
    }
//    qDebug() << "getPageToEdit" << normPath;


    QString file = d->pagesPath.absoluteFilePath(normPath);
    QFileInfo fileInfo(file);
    if (fileInfo.exists()) {
        QSettings data(file, QSettings::IniFormat);
        Page *page = new Page;

        // TODO save local path
//        QString localPath = path;
//        localPath = localPath.remove(0, d->pagesPath.path().length());
        page->setPath(path);

        page->setName(data.value("Name").toString());
        page->setAuthor(data.value("Author").toString());
        QDateTime modified = data.value("Modified").toDateTime();
        if (modified.isValid()) {
        } else {
            modified = fileInfo.lastModified();
        }
        page->setModified(modified);

        page->setNavigationLabel(data.value("NavigationLabel").toString());
        page->setTags(data.value("Tags").toStringList());
        data.beginGroup("Body");
        page->setContent(data.value("Content").toString());
        data.endGroup();
        return page;
    } else {
        qDebug() << "FileEngine file not found" << file;
    }
    return 0;
}

bool FileEngine::savePage(Page *page)
{
    Q_D(FileEngine);

    QString path = page->path();
    path.remove(QRegularExpression("^/"));
    if (path.isEmpty()) {
        path = QStringLiteral("index.page");
    } else {
        path.append(QStringLiteral(".page"));
    }

    QString file = d->pagesPath.absoluteFilePath(path);
//    qDebug() << "save Page" << page->path() << path;
    QSettings data(file, QSettings::IniFormat);
    data.setValue("Name", page->name());
    data.setValue("Modified", page->modified());
    data.setValue("Author", page->author());
    data.setValue("NavigationLabel", page->navigationLabel());
    data.setValue("Tags", page->tags());
    data.beginGroup("Body");
    data.setValue("Content", page->content());
    data.endGroup();
    data.sync();
//    qDebug() << "save page" << file;
    // if it's not writable we can't save the page
    return data.isWritable();
}

QList<Page *> FileEngine::listPages(int depth)
{
    Q_D(const FileEngine);

    QList<Page *> ret;
//    qDebug() << "listpages:" << d->pagesPath.path();

    QDirIterator it(d->pagesPath.path(),
                    QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString path = it.next();
//        qDebug() << "listpages:" << path;

        QString relpath = d->pagesPath.relativeFilePath(path);
//        qDebug() << "listpages relative:" << relpath << depth << relpath.count(QChar('/'));
        if (depth != -1 && relpath.count(QChar('/')) > depth) {
            continue;
        }

        if (relpath == QLatin1String("index.page")) {
            relpath = QStringLiteral("");
        } else {
            relpath.remove(QRegularExpression(".page$"));
        }

//        qDebug() << "listpages relative ok:" << relpath;
        Page *page = getPage(relpath);
        if (page) {
            ret.append(page);
        }
    }
    return ret;
}

QList<Menu *> FileEngine::menus()
{
    Q_D(FileEngine);

    const QDateTime &settingsDT = d->settingsInfo.lastModified();
    if (settingsDT == d->menusDT) {
        return d->menus;
    }

    d->settings->beginGroup("Menus");

    QList<CMS::Menu *> menus;
    foreach (const QString &menu, d->settings->childGroups()) {
        menus.append(d->createMenu(menu, this));
    }

    d->settings->endGroup();

    // Cache the result
    d->menus = menus;
    d->menusDT = settingsDT;

    return menus;
}

QHash<QString, Menu *> FileEngine::menuLocations()
{
    Q_D(FileEngine);

    const QDateTime &settingsDT = d->settingsInfo.lastModified();
    if (settingsDT == d->menuLocationsDT) {
        return d->menuLocations;
    }

    d->settings->beginGroup(QStringLiteral("Menus"));

    QHash<QString, CMS::Menu *> menus;
    foreach (const QString &menu, d->settings->childGroups()) {
        Menu *obj = d->createMenu(menu, this);

        bool added = false;
        Q_FOREACH (const QString &location, obj->locations()) {
            if (!menus.contains(location)) {
                menus.insert(location, obj);
                added = true;
            }
        }

        if (!added) {
            delete obj;
            continue;
        }

        menus.insert(menu, obj);
    }

    d->settings->endGroup(); // Menus

    // Cache the result
    d->menuLocations = menus;
    d->menuLocationsDT = settingsDT;

    return menus;
}

bool FileEngine::saveMenu(Menu *menu, bool replace)
{
    Q_D(const FileEngine);

    bool ret = false;
    d->settings->beginGroup(QStringLiteral("Menus"));

    if (replace || !d->settings->childGroups().contains(menu->name())) {
        d->settings->beginGroup(menu->name());

        d->settings->setValue("AutoAddPages", menu->autoAddPages());
        d->settings->setValue("Locations", menu->locations());

        QList<QVariantHash> urls = menu->entries();
        d->settings->beginWriteArray("urls", urls.size());
        for (int i = 0; i < urls.size(); ++i) {
            d->settings->setArrayIndex(i);
            // TODO save all values
            d->settings->setValue("text", urls.at(i).value("text"));
            d->settings->setValue("url", urls.at(i).value("url"));
        }
        d->settings->endArray();

        d->settings->endGroup();
        ret = true;
    }

    d->settings->endGroup(); // Menus

    return ret && d->settings->isWritable();
}

bool FileEngine::removeMenu(const QString &name)
{
    Q_D(FileEngine);

    d->settings->beginGroup(QStringLiteral("Menus"));
    if (d->settings->childGroups().contains(name)) {
        d->settings->remove(name);
    }
    d->settings->endGroup();

    return d->settings->isWritable();
}

QDateTime FileEngine::lastModified()
{
    Q_D(FileEngine);
    return d->settingsInfo.lastModified();
}

bool FileEngine::settingsIsWritable()
{
    Q_D(FileEngine);
    return d->settings->isWritable();
}

QHash<QString, QString> FileEngine::settings()
{
    Q_D(FileEngine);
    if (d->mainSettingsDT != d->settingsInfo.lastModified()) {
        QHash<QString, QString> ret;
        d->settings->beginGroup(QStringLiteral("Main"));
        Q_FOREACH (const QString &key, d->settings->allKeys()) {
            ret.insert(key, d->settings->value(key).toString());
        }
        d->settings->endGroup();
        d->mainSettings = ret;
    }

    return d->mainSettings;
}

QString FileEngine::settingsValue(const QString &key, const QString &defaultValue)
{
    Q_D(FileEngine);
    d->settings->beginGroup(QStringLiteral("Main"));
    const QString &ret = d->settings->value(key, defaultValue).toString();
    d->settings->endGroup();
    return ret;
}

bool FileEngine::setSettingsValue(const QString &key, const QString &value)
{
    Q_D(FileEngine);
    d->settings->beginGroup(QStringLiteral("Main"));
    d->settings->setValue(key, value);
    d->settings->endGroup();
    return d->settings->isWritable();
}


Menu *FileEnginePrivate::createMenu(const QString &name, QObject *parent)
{
    settings->beginGroup(name);

    Menu *menu = new Menu(name, parent);

    menu->setLocations(settings->value("Locations").toStringList());
    menu->setAutoAddPages(settings->value("AutoAddPages").toBool());

    QList<QVariantHash> urls;

    int size = settings->beginReadArray("urls");
    for (int i = 0; i < size; ++i) {
        settings->setArrayIndex(i);
        QVariantHash data;
        // TODO read all data
        data.insert("id", i);
        data.insert("text", settings->value("text"));
        data.insert("url", settings->value("url"));
        data.insert("attr", settings->value("attr"));
        urls.append(data);
    }
    settings->endArray();

    menu->setEntries(urls);

    settings->endGroup(); // menu name

    return menu;
}
