#ifndef ADMINSETUP_H
#define ADMINSETUP_H

#include <Cutelyst/Controller>

using namespace Cutelyst;

class AdminSetup : public Controller
{
    Q_OBJECT
    C_NAMESPACE("")
public:
    explicit AdminSetup(QObject *parent = 0);

    C_ATTR(setup, :Local :Args)
    void setup(Context *ctx);

    C_ATTR(edit, :Local :Args)
    void edit(Context *ctx, const QString &id);

    C_ATTR(remove_user, :Local :Args)
    void remove_user(Context *ctx, const QString &id);

    C_ATTR(status, :Path)
    void status(Context *ctx);

    C_ATTR(End, :Private :ActionClass(RenderView))
    void End(Context *ctx);
};

#endif // ADMINSETUP_H
