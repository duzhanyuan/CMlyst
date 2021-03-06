/***************************************************************************
 *   Copyright (C) 2014 Daniel Nicoletti <dantti12@gmail.com>              *
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

#include "adminmedia.h"

#include <Cutelyst/Upload>

#include <QDir>
#include <QDirIterator>
#include <QStringBuilder>
#include <QDebug>

AdminMedia::AdminMedia(QObject *app) : Controller(app)
{

}

AdminMedia::~AdminMedia()
{

}

void AdminMedia::index(Context *c)
{
    const static QDir mediaDir(c->config(QStringLiteral("DataLocation")).toString() + QLatin1String("/media"));

    QStringList files;
    QDirIterator it(mediaDir.absolutePath(),
                    QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        files.append(it.next());
    }
    files.sort(Qt::CaseInsensitive);

    int removeSize = mediaDir.absolutePath().size();
    QVariantList filesHash;
    for (const QString &file : files) {
        QFileInfo fileInfo(file);

        QString id = file.mid(removeSize);

        QString urlPath = QLatin1String("/.media") + id;

        QVariantHash hash;
        hash.insert(QStringLiteral("id"), id);
        hash.insert(QStringLiteral("name"), fileInfo.fileName());
        hash.insert(QStringLiteral("modified"), fileInfo.lastModified());
        hash.insert(QStringLiteral("url"), c->uriFor(urlPath).toString());
        filesHash.push_back(hash);
    }

    c->stash({
                   {QStringLiteral("template"), QStringLiteral("media/index.html")},
                   {QStringLiteral("files"), filesHash}
               });
}

void AdminMedia::upload(Context *c)
{
    const static QDir mediaDir(c->config(QStringLiteral("DataLocation")).toString() + QLatin1String("/media"));

    if (!mediaDir.exists() && !mediaDir.mkpath(mediaDir.absolutePath())) {
        qWarning() << "Could not create media directory" << mediaDir.absolutePath();
        c->response()->redirect(c->uriFor(CActionFor(QStringLiteral("index")),
                                              ParamsMultiMap({
                                                                 {QStringLiteral("error_msg"), QStringLiteral("Failed to save file")}
                                                             })));
        return;
    }

    // TODO this is NOT working...
    QFile link(c->config(QStringLiteral("DataLocation")).toString() + QLatin1String("/.media"));
    if (!link.exists() && !QFile::link(QStringLiteral("media"), link.fileName())) {
        qWarning() << "Could not create link media directory" << mediaDir.absolutePath() << link.fileName();
        c->response()->redirect(c->uriFor(CActionFor(QStringLiteral("index")),
                                              ParamsMultiMap({
                                                                 {QStringLiteral("error_msg"), QStringLiteral("Failed to save file")}
                                                             })));
        return;
    }

    QDir fileDir(mediaDir.absolutePath() + QDateTime::currentDateTimeUtc().toString(QStringLiteral("/yyyy/MM")));
    if (!fileDir.exists() && !fileDir.mkpath(fileDir.absolutePath())) {
        qWarning() << "Could not create media directory" << fileDir.absolutePath();
        c->response()->redirect(c->uriFor(CActionFor(QStringLiteral("index")),
                                              ParamsMultiMap({
                                                                 {QStringLiteral("error_msg"), QStringLiteral("Failed to save file")}
                                                             })));
        return;
    }

    Request *request = c->request();
    Upload *upload = request->upload(QStringLiteral("file"));
    if (!upload) {
        qWarning() << "Could not find upload";
        c->response()->redirect(c->uriFor(CActionFor(QStringLiteral("index")),
                                              ParamsMultiMap({
                                                                 {QStringLiteral("error_msg"), QStringLiteral("Failed to save file")}
                                                             })));
        return;
    }

    QString filepath = fileDir.absoluteFilePath(upload->filename());
    if (!upload->save(filepath)) {
        qWarning() << "Could not save upload" << filepath;
        c->response()->redirect(c->uriFor(CActionFor(QStringLiteral("index")),
                                              ParamsMultiMap({
                                                                 {QStringLiteral("error_msg"), QStringLiteral("Failed to save file")}
                                                             })));
        return;
    }

    c->response()->redirect(c->uriFor(CActionFor(QStringLiteral("index"))));
}

void AdminMedia::remove(Context *c, const QStringList &path)
{
    const static QDir mediaDir(c->config(QStringLiteral("DataLocation")).toString() + QLatin1String("/media"));

    QString file;

    for (const QString &part : path) {
        if (part.isEmpty() || part == QLatin1String("..")) {
            continue;
        }

        if (!file.isEmpty()) {
            file.append(QLatin1Char('/') + part);
        } else {
            file = part;
        }
    }

    QFile removeFile(mediaDir.absoluteFilePath(file));
    if (!removeFile.remove()) {
        qDebug() << "Failed to remove media file" << removeFile.fileName() << removeFile.errorString();
    }

    c->response()->redirect(c->uriFor(CActionFor(QStringLiteral("index"))));
}

