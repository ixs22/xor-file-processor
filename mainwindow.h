#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <worker.h>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_input_folder_btn_clicked();
    void on_output_folder_btn_clicked();
    void on_start_clicked();
    void on_stop_clicked();
    void on_one_time_toggled(bool checked);
    void on_timer_toggled(bool checked);
    void onProgressChanged(int percent);
    void onLogMessage(const QString &message);
    void onWorkerFinished();
    void onErrorOccurred(const QString &error);
    void onTimerTick();

private:
    Ui::MainWindow *ui;

    worker* m_worker;
    QTimer* m_timer;

    bool collectSettings(WorkerSettings &settings);

    void startProcessing();

    void setUiEnabled(bool enabled);

    void appendLog(const QString &message);
};

#endif // MAINWINDOW_H
