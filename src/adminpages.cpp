/***************************************************************************
 *   Copyright (C) 2014-2017 Daniel Nicoletti <dantti12@gmail.com>         *
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

#include "adminpages.h"

#include <Cutelyst/Plugins/Authentication/authentication.h>
#include <Cutelyst/Application>

#include <QDebug>

#include "libCMS/page.h"

AdminPages::AdminPages(Application *app) : Controller(app)
{

}

AdminPages::~AdminPages()
{

}

void AdminPages::index(Context *c)
{
    index(c, QStringLiteral("page"), CMS::Engine::Pages);
}

void AdminPages::create(Context *c)
{
    create(c, QStringLiteral("page"), true);
}

void AdminPages::edit(Context *c, const QString &id)
{
    edit(c, id, QStringLiteral("page"), true);
}

void AdminPages::remove(Context *c, const QString &id)
{
    if (!c->request()->isPost()) {
        c->response()->setStatus(Response::BadRequest);
        return;
    }

    engine->removePage(id.toInt());
    c->response()->setBody(QStringLiteral("ok"));
}

void AdminPages::index(Context *c, const QString &postType, CMS::Engine::Filter filters)
{
    c->setStash(QStringLiteral("post_type"), postType);

    QList<CMS::Page *> pages;
    if (filters == CMS::Engine::Pages) {
        pages = engine->listPages(c, -1, -1);
    } else {
        pages = engine->listPosts(c, -1, -1);
    }
    c->setStash(QStringLiteral("posts"), QVariant::fromValue(pages));

    c->setStash(QStringLiteral("template"), QStringLiteral("posts/index.html"));
}

void AdminPages::create(Context *c, const QString &postType, bool isPage)
{
    const ParamsMultiMap params = c->request()->bodyParams();
    QString title = params.value(QStringLiteral("title"));
    QString path = params.value(QStringLiteral("path"));
    QString content = params.value(QStringLiteral("edit-content"));
    if (c->req()->isPost()) {
        QString savePath;
        if (isPage) {
            savePath = CMS::Engine::normalizePath(path);
        } else {
            const QString date = QDate::currentDate().toString(QStringLiteral("yyyy/MM/dd/"));
            if (path.isEmpty()) {
                savePath = date + CMS::Engine::normalizeTitle(title);
            } else {
                savePath = date + CMS::Engine::normalizeTitle(path);
            }
        }

        auto page = new CMS::Page(c);
        page->setPath(savePath);
        page->setUuid(QString());
        page->setContent(content, true);
        page->setTitle(title);
        page->setPage(isPage);

        QDateTime dt = QDateTime::currentDateTimeUtc();
        page->setCreated(dt);
        page->setUpdated(dt);

        Author author = engine->user(Authentication::user(c).id().toInt());
        page->setAuthor(author);

        int id = engine->savePage(c, page);
        if (id) {
            c->res()->redirect(c->uriFor(actionFor(QStringLiteral("edit")), QStringList{ QString::number(id)}));
            return;
        } else {
            if (isPage && savePath.isEmpty()) {
                c->setStash(QStringLiteral("error_msg"), QStringLiteral("Path can not be empty"));
            } else {
                c->setStash(QStringLiteral("error_msg"), QStringLiteral("Failed to save page"));
            }
        }
    }

    c->setStash(QStringLiteral("post_type"), postType);
    c->setStash(QStringLiteral("title"), title);
    c->setStash(QStringLiteral("path"), path);
    c->setStash(QStringLiteral("edit_content"), content);
    c->setStash(QStringLiteral("template"), QStringLiteral("posts/create.html"));
}

void AdminPages::edit(Context *c, const QString &id, const QString &postType, bool isPage)
{
    CMS::Page *page = engine->getPageById(id, c);
    if (!page || page->page() != isPage) {
        c->res()->redirect(c->uriFor(actionFor(QStringLiteral("index"))));
        return;
    }

    QString path = page->path();
    QString title = page->title();
    QString content = page->content();

    if (c->req()->isPost()) {
        const ParamsMultiMap params = c->request()->bodyParams();
        title = params.value(QStringLiteral("title"));
        content = params.value(QStringLiteral("edit-content"));
        path = params.value(QStringLiteral("path"));
        QString action = params.value(QStringLiteral("submit"));

        page->updateContent(content);
        page->setTitle(title);
        page->setPage(isPage);
        page->setPath(path);
        if (action == QLatin1String("unpublish")) {
            page->setPublished(false);
        } else if (action == QLatin1String("publish")) {
            page->setPublished(true);
            if (!page->publishedAt().isValid()) {
                page->setPublishedAt(QDateTime::currentDateTimeUtc());
            }
        }
        page->setUpdated(QDateTime::currentDateTimeUtc());

        Author author = engine->user(Authentication::user(c).id().toInt());
        page->setAuthor(author);

        bool ret = engine->savePage(c, page);
        if (!ret) {
            c->setStash(QStringLiteral("error_msg"), QStringLiteral("Failed to save page"));
        }
    }

    c->setStash(QStringLiteral("title"), title);
    c->setStash(QStringLiteral("path"), path);
    c->setStash(QStringLiteral("edit_content"), content);
    c->setStash(QStringLiteral("editting"), true);
    c->setStash(QStringLiteral("published"), page->published());
    c->setStash(QStringLiteral("post_type"), postType);
    c->setStash(QStringLiteral("template"), QStringLiteral("posts/create.html"));
}
