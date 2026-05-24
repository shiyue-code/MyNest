#include "widget.h"

#include <QApplication>
#include <QFile>
#include <QFont>

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    
    // Set modern default font
    QFont defaultFont("Microsoft YaHei", 9);
    defaultFont.setStyleHint(QFont::SansSerif);
    a.setFont(defaultFont);

    // Load QSS stylesheet from Qt resource
    QFile file(":/style.qss");
    if (file.open(QFile::ReadOnly)) {
        QString styleSheet = QLatin1String(file.readAll());
        a.setStyleSheet(styleSheet);
        file.close();
    } else {
        qWarning("Failed to load style.qss from resource system.");
    }

    Widget w;
    w.show();
    return a.exec();
}
