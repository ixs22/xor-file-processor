#ifndef WORKER_H
#define WORKER_H

#include <QThread>
#include <QString>
#include <QStringList>
#include <atomic>

struct WorkerSettings{
    QString fileFilter;

    bool deleteInput;
    bool overwriteOutput;

    bool launchMode;
    QString timer;

    quint64 xorKey;

    QString inputFolder;
    QString outputFolder;
};

class worker : public QThread
{
    Q_OBJECT
public:
    explicit worker(QObject* parent = nullptr);

    void setSettings(const WorkerSettings& settings);

    void stop();

signals:
    void progressChanged(int precent);
    void logMessage(const QString& message);
    void finish();
    void errorOccurred(const QString& error);

protected:
    void run() override;

private:
    WorkerSettings settings_;
    std::atomic<bool> stop_flag_;

    bool processFile(const QString& inputPath, const QString& outputPath);
    QString resolveOutputPath(const QString& fileName);
};

#endif // WORKER_H
