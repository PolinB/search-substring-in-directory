#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <iostream>
#include <QCommonStyle>
#include <QDesktopWidget>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QTextStream>
#include <algorithm>
#include <functional>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include <QtConcurrent/QtConcurrentMap>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {

    ui->setupUi(this);

    addFilterAction(ui->actionTxt);
    addFilterAction(ui->actionCpp);
    addFilterAction(ui->actionJava);
    addFilterAction(ui->actionKt);
    addFilterAction(ui->actionPdf);
    addFilterAction(ui->actionCss);
    addFilterAction(ui->actionHtml);
    addFilterAction(ui->actionMd);
    addFilterAction(ui->actionJs);
    addFilterAction(ui->actionH);
    addFilterAction(ui->actionHs);

    toStartState();

    connect(ui->actionExit, &QAction::triggered, this, &QApplication::quit);

    connectAllFilters();

    connect(ui->inputLineEdit, &QLineEdit::textChanged, this, &MainWindow::changeLine);
    connect(ui->actionClearInput, &QAction::triggered, ui->inputLineEdit, &QLineEdit::clear);

    connect(ui->actionChooseDirectory, &QAction::triggered, this, &MainWindow::selectDirectory);
    connect(&scanning, &QFutureWatcher<void>::started, ui->progressBar, &QProgressBar::reset);
    connect(&scanning, &QFutureWatcher<void>::finished, this, &MainWindow::afterScan);
    connect(this, &MainWindow::setProgress, ui->progressBar, &QProgressBar::setValue);

    connect(ui->actionFind, &QAction::triggered, this, &MainWindow::runSearch);
    connect(&searching, &QFutureWatcher<void>::started, ui->progressBar, &QProgressBar::reset);
    connect(&searching, &QFutureWatcher<void>::progressRangeChanged, ui->progressBar, &QProgressBar::setRange);
    connect(&searching, &QFutureWatcher<void>::progressValueChanged, ui->progressBar, &QProgressBar::setValue);
    connect(&searching, &QFutureWatcher<void>::finished, this, &MainWindow::afterSearch);

    connect(ui->actionCancel, &QAction::triggered, this, &MainWindow::cancel);
    connect(ui->actionClear, &QAction::triggered, this, [this] {
       isCleared = 1;
       if (!cancel()) {
           toStartState();
           isCleared = 0;
       }
    });
}

void MainWindow::addFilterAction(QAction *action) {
    filters.actions.push_back(action);
    filters.activeState[action->text()] = false;
}

void MainWindow::connectAllFilters() {
    for (QAction* filter : filters.actions) {
        connect(filter, &QAction::triggered, this, &MainWindow::chooseFileType);
    }
}

/*void MainWindow::filterEnabled(bool isEnabled) {
    ui->actionTxt->setEnabled(isEnabled);
    ui->actionCpp->setEnabled(isEnabled);
    ui->actionJava->setEnabled(isEnabled);
    ui->actionKt->setEnabled(isEnabled);
    ui->actionPdf->setEnabled(isEnabled);
    ui->actionCss->setEnabled(isEnabled);
    ui->actionHtml->setEnabled(isEnabled);
    ui->actionMd->setEnabled(isEnabled);
    ui->actionJs->setEnabled(isEnabled);
    ui->actionH->setEnabled(isEnabled);
    ui->actionHs->setEnabled(isEnabled);
}*/

void MainWindow::Filters::setEnabled(bool isEnabled) {
    for (QAction* filter : actions) {
        filter->setEnabled(isEnabled);
    }
}

bool MainWindow::Filters::getActiveState(const QString& suffix) {
    bool result;

    checkState.lock();
    result = activeState[suffix];
    checkState.unlock();

    return result;
}

void MainWindow::chooseFileType() {
    QAction *action = qobject_cast<QAction *>(sender());

    bool setInFilters = filters.activeState[action->text()];

    filters.activeState[action->text()] = (!setInFilters);
    if (setInFilters) {
        filters.filtesSize -= 1;
        action->setIcon(QIcon());
    } else {
        action->setIcon(QIcon(":/pref/img/checked.png"));
        filters.filtesSize += 1;
    }
}

void MainWindow::toStartState() {
    filters.setEnabled(true);
    ui->progressBar->setValue(0);
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setDisabled(true);
    ui->inputLineEdit->clear();
    ui->resultListWidget->clear();
    files.clear();
    line.clear();
    ui->statusBar->clearMessage();

    directoryChoose = false;
    ui->actionChooseDirectory->setEnabled(true);
    ui->actionFind->setEnabled(false);
    ui->actionCancel->setEnabled(false);
    ui->inputLineEdit->setReadOnly(false);
}

void MainWindow::changeLine() {
    line = ui->inputLineEdit->text();
    if (!line.isEmpty() && directoryChoose) {
        ui->actionFind->setEnabled(true);
    } else {
        ui->actionFind->setEnabled(false);
    }
}

bool MainWindow::cancel() {
    if (searching.isRunning()) {
        isCanceled = 1;
        searching.cancel();
        return true;
    }
    if (scanning.isRunning()) {
        isCanceled = 1;
        scanning.cancel();
        return true;
    }
    return false;
}

void MainWindow::selectDirectory() {
    QString directoryName = QFileDialog::getExistingDirectory(this, "Select Directory for Scanning",
                                                    QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!directoryName.isEmpty()) {
        files.clear();
        ui->resultListWidget->clear();
        ui->actionCancel->setEnabled(true);
        ui->actionFind->setEnabled(false);
        ui->progressBar->setValue(0);
        ui->progressBar->setRange(0, 100);
        ui->progressBar->setDisabled(false);
        ui->statusBar->showMessage("Scanning...");

        dirInQueue = 1;
        finishedDir = 0;
        currentDirectoryName = directoryName;
        scanning.setFuture(QtConcurrent::run([this, directoryName] {scanDirectory(directoryName);}));
    }
}

void MainWindow::scanDirectory(QString const& directoryName) {
    if (isCanceled == 1) {
        return;
    }
    QDir directory(directoryName);
    QFileInfoList list = directory.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    for (QFileInfo& fileInfo : list) {
        if (isCanceled == 1) {
            return;
        }
        QString path = fileInfo.absoluteFilePath();
        if (fileInfo.isFile()) {
            files.push_back(fileInfo);
        } else if (fileInfo.isDir()){
            ++dirInQueue;
            scanDirectory(path);
        }
    }
    ++finishedDir;
    emit setProgress((finishedDir * 100) / dirInQueue);
}

void MainWindow::afterScan() {
    if (!isCanceled) {
        directoryChoose = true;
        ui->labelOutput->setText("This text in files: " + currentDirectoryName);

        if (!line.isEmpty()) {
            ui->actionFind->setEnabled(true);
        }
    } else {
        directoryChoose = false;
        currentDirectoryName = false;
        ui->labelOutput->setText("This text in files: ");

        files.clear();
        isCanceled = 0;
    }
    ui->progressBar->setDisabled(true);

    ui->actionCancel->setEnabled(false);
    ui->statusBar->showMessage("End scanning.");
}

void MainWindow::runSearch() {
    ui->actionCancel->setEnabled(true);
    filters.setEnabled(false);

    ui->actionChooseDirectory->setEnabled(false);
    ui->inputLineEdit->setReadOnly(true);
    ui->actionFind->setEnabled(false);
    ui->resultListWidget->clear();

    ui->progressBar->setValue(0);
    ui->progressBar->setDisabled(false);

    ui->statusBar->showMessage("Searching...");

    searching.setFuture(QtConcurrent::map(files, [this] (QFileInfo const& fileInfo) { checkFile(fileInfo);}));
}

void MainWindow::checkFile(QFileInfo const& fileInfo) {
    QFile file(fileInfo.absoluteFilePath());

    QString suffix = "." + fileInfo.completeSuffix();
    if (filters.filtesSize != 0 && !filters.getActiveState(suffix)) {
        return;
    }

    if (!file.open(QIODevice::ReadOnly))
            return;

    QTextStream textStream(&file);
    QString buffer = textStream.read(line.size() - 1);

    quint64 numberInFile = 0;
    while (!textStream.atEnd()) {
        if (isCanceled == 1) {
            return;
        }
        QString partBuffer = textStream.read(MAX_BLOCK_SIZE);
        buffer += partBuffer;
        numberInFile += searchInBuffer(buffer);
        buffer.remove(0, partBuffer.size());
    }

    if (numberInFile != 0 && !(isCanceled == 1)) {
        addToResultList.lock();

        ui->resultListWidget->addItem(fileInfo.filePath());

        addToResultList.unlock();
    }
}

quint64 MainWindow::searchInBuffer(QString const& buffer) {
    quint64 numberInBuffer = 0;
    std::string stdBuffer = buffer.toStdString();
    std::string stdLine = line.toStdString();

    auto it = std::search(stdBuffer.begin(), stdBuffer.end(), std::boyer_moore_searcher(stdLine.begin(), stdLine.end()));
    while (it != stdBuffer.end()) {
        if (isCanceled == 1) {
            return numberInBuffer;
        }
        ++numberInBuffer;
        it = std::search(it + 1, stdBuffer.end(), std::boyer_moore_searcher(stdLine.begin(), stdLine.end()));
    }
    return numberInBuffer;
}

void MainWindow::afterSearch() {
    if (isCleared) {
        toStartState();
        isCleared = 0;
        isCanceled = 0;
    } else {
        ui->inputLineEdit->setReadOnly(false);
        ui->actionFind->setEnabled(true);
        ui->actionChooseDirectory->setEnabled(true);
        ui->actionCancel->setEnabled(false);
        ui->progressBar->setDisabled(true);
        filters.setEnabled(true);
        isCanceled = 0;
    }

    ui->statusBar->showMessage("End searching.");
}

MainWindow::~MainWindow() {
    delete ui;
}
