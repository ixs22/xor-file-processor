#include "worker.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>

worker::worker(QObject* parent) : QThread(parent), stop_flag_(false){}

void worker::stop(){
    stop_flag_ = true;
}

void worker::setSettings(const WorkerSettings& settings){
    settings_ = settings;
}

void worker::run(){
    stop_flag_ = false;

    QDir inputDir(settings_.inputFolder);
    if(!inputDir.exists()){
        emit errorOccurred("Ошибка: файл не найден! " + settings_.inputFolder);
        emit finish();
        return;
    }

    QStringList filters;
    filters << settings_.fileFilter;
    QStringList fileName = inputDir.entryList(filters, QDir::Files);
    if(fileName.isEmpty()){
        emit errorOccurred("Файлы с фильтром: '" + settings_.fileFilter + "' не найдены.");
        emit finish();
        return;
    }

    int totalFiles = fileName.size();
    int processedFiles = 0;
    emit logMessage("Найдено: " + QString::number(totalFiles));

    for(const QString& file_name : fileName){
        if(stop_flag_){
            emit logMessage("Обработка остановлена пользователем.");
            break;
        }

        QString inputPath = inputDir.filePath(file_name);
        QString outputPath = resolveOutputPath(file_name);

        emit logMessage("Обработка: " + file_name);

        bool ok = processFile(inputPath, outputPath);

        if(ok){
            processedFiles++;
            emit logMessage("Обработан:" + file_name + " - " + outputPath);
            if(settings_.deleteInput){
                if(!QFile::remove(inputPath)){
                    emit errorOccurred("Не удалось удалить файл " + inputPath);
                }
            }
        }else{
            emit errorOccurred("Ошибка обработки файла: " + file_name);
        }

     }
    emit logMessage("Обработка файлов завершена. Обработано " + QString::number(processedFiles) + " из " + QString::number(totalFiles));
    emit finish();
}

bool worker::processFile(const QString& inputPath, const QString& outputPath){
    QFile inputFile(inputPath);
    if(!inputFile.open(QIODevice::ReadOnly)){
        emit logMessage("Не удалось открыть файл: " + inputPath);
        return false;
    }

    QFile outputFile(outputPath);
    if(!outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate)){
        emit logMessage("Не удалось создать файл: " + outputPath);
        return false;
    }

    qint64 totalSize = inputFile.size();
    qint64 processedBytes = 0;
    int lastPercent = -1;

    qint8 keyBytes[8];
    for(int i = 0; i < 8; ++i){
        keyBytes[i] = static_cast<quint8>((settings_.xorKey >> (56-i*8)) &0xFF);
    }

    const int BUFFER_SIZE = 65536;
    QByteArray buffer;
    buffer.resize(BUFFER_SIZE);

    while(!inputFile.atEnd()){
        if(stop_flag_){
            inputFile.close();
            outputFile.close();

            QFile::remove(outputPath);
            return false;
        }
        qint64 bytesRead = inputFile.read(buffer.data(), BUFFER_SIZE);
        if(bytesRead < 0){
            emit errorOccurred("Ошбка чтения файла: " + inputPath);
            inputFile.close();
            outputFile.close();

            QFile::remove(outputPath);
            return false;
        }

        for(qint64 i = 0; i < bytesRead; ++i){
            buffer.data()[i] ^= keyBytes[i%8];
        }

        qint64 bytesWritten = outputFile.write(buffer.data(), bytesRead);
        if(bytesRead != bytesWritten){
            emit errorOccurred("Ошибка записи файла: " + outputPath);
            inputFile.close();
            outputFile.close();
            QFile::remove(outputPath);
            return  false;
        }
        processedBytes += bytesRead;

        if(totalSize > 0){
            int percent = static_cast<int>((processedBytes * 100)/totalSize);
            if(percent != lastPercent){
                lastPercent = percent;
                emit progressChanged(percent);
            }
        }
    }

    inputFile.close();
    outputFile.close();
    return true;
}

QString worker::resolveOutputPath(const QString& fileName){
    QDir outputDir(settings_.outputFolder);
    QString fullPath = outputDir.filePath(fileName);

    if(settings_.overwriteOutput || !QFile::exists(fullPath)){
        return fullPath;
    }

    QFileInfo fileInfo(fileName);
    QString baseName = fileInfo.completeBaseName();
    QString extension = fileInfo.completeSuffix();

    int counter = 1;
    QString newPath;

    do{
        QString newName;
        if(extension.isEmpty()){
            newName = baseName + "_" + QString::number(counter);
        }else{
            newName = baseName + "_" + QString::number(counter) + "." + extension;
        }
        newPath = outputDir.filePath(newName);
        counter++;
    }while(QFile::exists(newPath));

    return newPath;
}

