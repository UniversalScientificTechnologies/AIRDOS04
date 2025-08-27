#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QStringList>

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    // Kontrola příkazové řádky pro režim CLI
    bool cliMode = false;
    QStringList args = QCoreApplication::arguments();
    if (args.contains("--cli")) {
        cliMode = true;
    }

    if (cliMode) {
        QCoreApplication coreApplication(argc, argv);
        // Zde implementujte vaši CLI logiku
        // Například zpracování příkazů zde
        return coreApplication.exec();
    } else {
        QGuiApplication guiApplication(argc, argv);
        QQmlApplicationEngine engine;
        engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
        if (engine.rootObjects().isEmpty())
            return -1;

        return guiApplication.exec();
    }
}