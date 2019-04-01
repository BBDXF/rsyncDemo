#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>
#include "librsync.h"
#include <QFileDialog>
#include <QDebug>

namespace Ui {
class Dialog;
}

class Dialog : public QDialog
{
    Q_OBJECT

public:
    explicit Dialog(QWidget *parent = nullptr);
    ~Dialog();

    void push_log(QString log);

private slots:
    void on_btn_sig_clicked();

    void on_btn_delta_clicked();

    void on_btn_patch_clicked();

    void on_btn_get_blake2_clicked();

private:
    Ui::Dialog *ui;
    QString m_curFilePath;
    QString m_curNewFilePath;

    bool rsync_sig(QString oldPath, QString sigPath, bool isMD4=true);
    bool rsync_delta(QString sigPath, QString newPath, QString deltaPath);
    bool rsync_patch(QString oldPath, QString deltaPath, QString newPath);
};

#endif // DIALOG_H
