#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSet>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QMutex>
#include <QHash>
#include <QMap>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
private:
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void selectDirectory();
    void scanDirectory(QString const& directoryName);
    void runSearch();
    void afterSearch();
    void afterScan();
    void changeLine();
    bool cancel();
    void chooseFileType();
    //void setProgress(qint64 x);

signals:
    void setProgress(qint64 x);

private:
    enum state {START, READY_TO_SEARCH, SCAN, SEARCH};

    struct Filters {
        QMap<QString, bool> activeState;
        QVector<QAction *> actions;
        void setEnabled(bool isEnabled);
        void clear();
        void chooseFileType();
        bool getActiveState(const QString& type);
        QAtomicInt filtesSize = 0;
        QMutex checkState;
    };

    Filters filters;

    void addFilterAction(QAction *action);
    void connectAllFilters();
    void checkFile(QFileInfo const& file);
    void toStartState();
    quint64 searchInBuffer(QString const& buffer);
    void changeState(state s);
    //void filterEnabled(bool isEnabled);

    QMutex addToResultList;

    static const quint32 MAX_BLOCK_SIZE = 1024;

    Ui::MainWindow *ui;
    QString line;
    QString currentDirectoryName;
    state currentState = START;
    QVector<QFileInfo> files;

    QFutureWatcher<void> scanning;
    QFutureWatcher<void> searching;

    QAtomicInt dirInQueue = 0;
    QAtomicInt finishedDir = 0;
    bool directoryChoose = false;
    QAtomicInt isCanceled = 0;
    QAtomicInt isCleared = 0;
};

#endif // MAINWINDOW_H
