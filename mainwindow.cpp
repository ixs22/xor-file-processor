#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_worker(nullptr)
    , m_timer(new QTimer(this))
{
    ui->setupUi(this);

    this->setWindowTitle("XOR File Processor");
    ui->txt_filter->setText("Фильтры поиска");
    ui->file_filer->setPlaceholderText("Пример: *.txt");

    ui->delete_input_file->setText("Удаление входного файла после обработки");

    ui->groupBox->setTitle("Если файлы имеют одинаковое имя:");
    ui->overwriting_file->setText("Перезаписать файл");
    ui->overwriting_file->setChecked(true);
    ui->copy_file->setText("Создать копию");

    ui->groupBox_2->setTitle("Режим запуска");

    ui->one_time->setText("Разовый");
    ui->one_time->setChecked(true);

    ui->timer->setText("Таймер");
    ui->t_period->setEnabled(false);
    ui->t_period->setPlaceholderText("5");
    ui->txt_t_period_2->setText("частота в секундах");

    ui->txt_XOR_key->setText("XOR ключ");
    ui->XOR_key->setPlaceholderText("Пример: C48A6F129E357B0D");

    ui->path_input_file->setReadOnly(true);
    ui->path_input_file->setPlaceholderText("Выберите папку входных файлов...");
    ui->input_folder_btn->setText("Обзор...");

    ui->path_output_file->setReadOnly(true);
    ui->path_output_file->setPlaceholderText("Выберите папку для сохранения файлов...");
    ui->output_folder_btn->setText("Обзор...");

    ui->start->setText("Старт");

    ui->stop->setText("Стоп");
    ui->stop->setEnabled(false);

    ui->progress_bar->setValue(0);
    ui->progress_bar->setMinimum(0);
    ui->progress_bar->setMaximum(100);

    ui->log->setReadOnly(true);

    connect(m_timer, &QTimer::timeout, this, &MainWindow::onTimerTick);

    appendLog("Программа запущена. Настройте параметры и нажмите Старт.");
}

MainWindow::~MainWindow()
{
    if(m_timer->isActive()){
        m_timer->stop();
    }

    if(m_worker && m_worker->isRunning()){
        m_worker->stop();
        m_worker->wait(3000);
    }

    delete ui;
}

void MainWindow::on_input_folder_btn_clicked(){
    QString folder = QFileDialog::getExistingDirectory(
        this,
        "Выберите папку входных файлов",
        ui->path_input_file->text().isEmpty() ? QDir::homePath() : ui->path_input_file->text()
        );

    if(!folder.isEmpty()){
        ui->path_input_file->setText(folder);
        appendLog("Входная папка: " + folder);
    }
}

void MainWindow::on_output_folder_btn_clicked(){
    QString folder = QFileDialog::getExistingDirectory(
        this,
        "Выберите папку для сохранения",
        ui->path_output_file->text().isEmpty() ? QDir::homePath() : ui->path_output_file->text()
        );

    if(!folder.isEmpty()){
        ui->path_output_file->setText(folder);
        appendLog("Папка для сохранения: " + folder);
    }
}

void MainWindow::on_one_time_toggled(bool checked){
    if(checked){
        ui->t_period->setEnabled(false);
    }
}

void MainWindow::on_timer_toggled(bool checked){
    if(checked){
        ui->t_period->setEnabled(true);
    }
}

void MainWindow::on_start_clicked(){
    if(m_worker && m_worker->isRunning()){
        return;
    }

    if(m_timer->isActive()){
        m_timer->stop();
    }

    if(ui->timer->isChecked()){
        bool ok = false;
        int period = ui->t_period->text().toInt(&ok);
        if(!ok || period <= 0){
            QMessageBox::warning(this, "Ошибка", "Введите корректный период опроса.");
            return;
        }

        WorkerSettings settings;
        if(!collectSettings(settings)){
            return;
        }

        appendLog("Запуск в режиме таймера, период: " + QString::number(period) + " сек.");

        startProcessing();

        m_timer->start(period*1000);
    }else{
        appendLog("Разовый запуск.");
        startProcessing();
    }
}

void MainWindow::on_stop_clicked(){
    if(m_timer->isActive()){
        m_timer->stop();
        appendLog("Таймер остановлен.");
    }
    if (m_worker && m_worker->isRunning()){
        m_worker->stop();
        appendLog("Оставонка обработки...");
    }
}

void MainWindow::onTimerTick(){
    if(m_worker && m_worker->isRunning()){
        appendLog("предыдущая обработка ещё не завершена.");
        return;
    }

    appendLog("Запуск новой итерации.");
    startProcessing();
}

void MainWindow::startProcessing(){
    WorkerSettings settings;
    if(!collectSettings(settings)){
        if(m_timer->isActive()){
            m_timer->stop();
        }
        return;
    }
    if(m_worker){
        delete m_worker;
        m_worker = nullptr;
    }

    m_worker = new worker(this);
    m_worker->setSettings(settings);
    connect(m_worker, &worker::progressChanged, this, &MainWindow::onProgressChanged);
    connect(m_worker, &worker::logMessage,      this, &MainWindow::onLogMessage);
    connect(m_worker, &worker::finish, this, &MainWindow::onWorkerFinished);
    connect(m_worker, &worker::errorOccurred,   this, &MainWindow::onErrorOccurred);

    setUiEnabled(false);
    ui->progress_bar->setValue(0);

    m_worker->start();

}

bool MainWindow::collectSettings(WorkerSettings &settings)
{
    QString mask = ui->file_filer->text().trimmed();
    if (mask.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите маску файлов (например *.txt).");
        return false;
    }
    QString inputFolder = ui->path_input_file->text().trimmed();
    if (inputFolder.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Выберите папку входных файлов.");
        return false;
    }
    QString outputFolder = ui->path_output_file->text().trimmed();
    if (outputFolder.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Выберите папку выходных файлов.");
        return false;
    }

    if(QDir(inputFolder) == QDir(outputFolder) && !ui->delete_input_file->isChecked()){
        QMessageBox::warning(this, "Ошибка",
                             "Папки входных и выходных файлов совпадают.\n"
                             "Включите удаление входных файлов или выберите разные папки.\n\n"
                             "Иначе обработанные файлы будут повторно обрабатываться.");
        return false;
    }

    QString keyStr = ui->XOR_key->text().trimmed();
    if (keyStr.length() != 16) {
        QMessageBox::warning(this, "Ошибка",
                             "XOR ключ должен содержать ровно 16 HEX символов (8 байт).\n"
                             "Пример: C48A6F129E357B0D");
        return false;
    }

    bool ok = false;
    quint64 xorKey = keyStr.toULongLong(&ok, 16);
    if (!ok) {
        QMessageBox::warning(this, "Ошибка",
                             "XOR ключ содержит недопустимые символы.\n"
                             "Используйте только HEX символы: 0-9, A-F.");
        return false;
    }

    settings.inputFolder = inputFolder;
    settings.outputFolder = outputFolder;
    settings.fileFilter = mask;
    settings.xorKey = xorKey;
    settings.deleteInput = ui->delete_input_file->isChecked();
    settings.overwriteOutput = ui->overwriting_file->isChecked();

    return true;
}

void MainWindow::onProgressChanged(int percent)
{
    ui->progress_bar->setValue(percent);
}

void MainWindow::onLogMessage(const QString &message)
{
    appendLog(message);
}

void MainWindow::onWorkerFinished()
{
    ui->progress_bar->setValue(100);
    if (!m_timer->isActive()) {
        setUiEnabled(true);
        appendLog("---Готово---");
    }
}

void MainWindow::onErrorOccurred(const QString &error)
{
    appendLog("[ОШИБКА] " + error);
    statusBar()->showMessage("Ошибка: " + error, 5000);
}

void MainWindow::setUiEnabled(bool enabled)
{
    ui->file_filer->setEnabled(enabled);
    ui->input_folder_btn->setEnabled(enabled);
    ui->output_folder_btn->setEnabled(enabled);
    ui->delete_input_file->setEnabled(enabled);
    ui->overwriting_file->setEnabled(enabled);
    ui->copy_file->setEnabled(enabled);
    ui->XOR_key->setEnabled(enabled);
    ui->one_time->setEnabled(enabled);
    ui->timer->setEnabled(enabled);
    ui->t_period->setEnabled(enabled && ui->timer->isChecked());
    ui->start->setEnabled(enabled);
    ui->stop->setEnabled(!enabled);
}

void MainWindow::appendLog(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    ui->log->appendPlainText("[" + timestamp + "] " + message);
    QTextCursor cursor = ui->log->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->log->setTextCursor(cursor);
}
