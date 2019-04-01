#include "dialog.h"
#include "ui_dialog.h"
#include "blake2/blake2.h"

extern char const rs_librsync_version[];

Dialog::Dialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Dialog)
{
    ui->setupUi(this);
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    QString strVersion = QString("<html><h2>%1</h2></html>").arg(rs_librsync_version);
    ui->label->setText(strVersion);
}

Dialog::~Dialog()
{
    delete ui;
}

bool Dialog::rsync_sig(QString oldPath, QString sigPath, bool isMD4)
{
    bool bIsOk = false;
    FILE* fpOld = fopen(oldPath.toLocal8Bit().data(), "rb");
    FILE* fpSig = fopen(sigPath.toLocal8Bit().data(), "wb");
    if( !fpOld || !fpSig ){
        goto err_end;
    }
    rs_result res;
    if( isMD4 ){
        res = rs_sig_file(fpOld, fpSig, RS_DEFAULT_BLOCK_LEN, 8, rs_magic_number::RS_MD4_SIG_MAGIC, nullptr);
    }else{
        res = rs_sig_file(fpOld, fpSig, RS_DEFAULT_BLOCK_LEN, 0, rs_magic_number::RS_BLAKE2_SIG_MAGIC, nullptr);
    }
    if( res == RS_DONE ){
        bIsOk = true;
    }
err_end:
    if (fpOld) {
        fclose(fpOld);
        fpOld = nullptr;
    }
    if (fpSig) {
        fclose(fpSig);
        fpSig = nullptr;
    }
    return bIsOk;
}

bool Dialog::rsync_delta(QString sigPath, QString newPath, QString deltaPath)
{
    bool bIsOk = false;
    rs_result res;
    rs_signature* sumset = nullptr;
    FILE* fpSig = fopen(sigPath.toLocal8Bit().data(), "rb");
    FILE* fpNew = fopen(newPath.toLocal8Bit().data(), "rb");
    FILE* fpDelta = fopen(deltaPath.toLocal8Bit().data(), "wb");
    if( !fpSig || !fpNew || !fpDelta){
        goto err_end;
    }

    res = rs_loadsig_file(fpSig, &sumset, nullptr);
    if( res != RS_DONE ){
        goto err_end;
    }
    res = rs_build_hash_table(sumset);
    if( res != RS_DONE ){
        goto err_end;
    }
    res = rs_delta_file(sumset, fpNew, fpDelta, nullptr);
    if( res == RS_DONE ){
        bIsOk = true;
    }

err_end:
    if( sumset ){
        rs_free_sumset(sumset);
    }
    if (fpNew) {
        fclose(fpNew);
        fpNew = nullptr;
    }
    if (fpSig) {
        fclose(fpSig);
        fpSig = nullptr;
    }
    if (fpDelta) {
        fclose(fpDelta);
        fpDelta = nullptr;
    }
    return bIsOk;
}

bool Dialog::rsync_patch(QString oldPath, QString deltaPath, QString newPath)
{
    bool bIsOk = false;
    rs_result res;

    FILE* fpOld = fopen(oldPath.toLocal8Bit().data(), "rb");
    FILE* fpNew = fopen(newPath.toLocal8Bit().data(), "wb");
    FILE* fpDelta = fopen(deltaPath.toLocal8Bit().data(), "rb");
    if( !fpOld || !fpNew || !fpDelta){
        goto err_end;
    }

    res = rs_patch_file(fpOld, fpDelta, fpNew, nullptr);
    if( res == RS_DONE ){
        bIsOk = true;
    }

err_end:
    if (fpNew) {
        fclose(fpNew);
        fpNew = nullptr;
    }
    if (fpOld) {
        fclose(fpOld);
        fpOld = nullptr;
    }
    if (fpDelta) {
        fclose(fpDelta);
        fpDelta = nullptr;
    }
    return bIsOk;
}

void Dialog::push_log(QString log)
{
    ui->edit_log->append(log);
}

void Dialog::on_btn_sig_clicked()
{
    QFileDialog* fDlg = new QFileDialog(this, "Select a file:","","*.*");
    fDlg->setFileMode(QFileDialog::FileMode::ExistingFile);
    if(fDlg->exec() == QDialog::Accepted){
        QString sFilePath = fDlg->selectedFiles()[0];
        m_curFilePath = sFilePath;
        QFileInfo fInfo(sFilePath);
        QString sSigPath = fInfo.filePath()+".sig";

        push_log ("\n==============\nSig Dir: "+fInfo.dir().path());

        push_log(QString("Target: %1\n  Size: %2\n").arg(fInfo.fileName()).arg(fInfo.size()));
        push_log(" Sig: "+fInfo.fileName()+".sig");
        bool isBlake2 = ui->check_box_blacke2->isChecked();
        if( rsync_sig(sFilePath, sFilePath+".sig", !isBlake2) ){
            QFileInfo fSigInfo(sFilePath+".sig");
            push_log(QString("Size: %1 (%2) %3%").arg(fSigInfo.size()).arg(isBlake2?"blake2":"md4").arg(fSigInfo.size()*100.0/fInfo.size(),0,'f',2));
        }else{
            push_log("Error: get sig failed!");
        }
    }
}


void Dialog::on_btn_delta_clicked()
{
    QFileDialog* fDlg = new QFileDialog(this, "Select a new file:","","*.*");
    fDlg->setFileMode(QFileDialog::FileMode::ExistingFile);
    if(fDlg->exec() == QDialog::Accepted){
        QString sFilePath = fDlg->selectedFiles()[0];
        m_curNewFilePath = sFilePath;
        QFileInfo fInfo(sFilePath);
        QString sNewPath = fInfo.filePath()+".delta";

        push_log ("\n==============\nDelta Dir: "+fInfo.dir().path());

        push_log(QString("Target new: %1\n  Size: %2\n").arg(fInfo.fileName()).arg(fInfo.size()));
        push_log("Delta: "+fInfo.fileName()+".delta");

        if(rsync_delta(m_curFilePath+".sig", m_curNewFilePath, m_curNewFilePath+".delta")){
            QFileInfo fDeltaInfo(sFilePath+".delta");
            push_log(QString(" Size: %1 %2%").arg(fDeltaInfo.size()).arg(fDeltaInfo.size()*100.0/fInfo.size(),0,'f',2));
        }else{
            push_log("Error: get delta failed!");
        }
    }
}

void Dialog::on_btn_patch_clicked()
{
    push_log ("\n==============");

    push_log("  Old File: "+m_curFilePath);
    push_log("Delta File: "+m_curNewFilePath+".delta");

    if(rsync_patch(m_curFilePath, m_curNewFilePath+".delta", m_curNewFilePath+".bak")){
        push_log("  New File: "+m_curNewFilePath+".bak");
        QFileInfo fSigInfo(m_curNewFilePath+".bak");
        push_log(QString(" Size: %1 ").arg(fSigInfo.size()));
    }else{
        push_log("Error: get delta failed!");
    }
}

void Dialog::on_btn_get_blake2_clicked()
{
    QFileDialog* fDlg = new QFileDialog(this, "Select a new file:","","*.*");
    fDlg->setFileMode(QFileDialog::FileMode::ExistingFile);
    if(fDlg->exec() == QDialog::Accepted){
        QString sFilePath = fDlg->selectedFiles()[0];
        char buf[2048];
        int len = 0;
        char sum[RS_MAX_STRONG_SUM_LENGTH];

        blake2b_state ctx;
        blake2b_init(&ctx, RS_MAX_STRONG_SUM_LENGTH);
        QFile fTarget(sFilePath);
        if(fTarget.open(QFile::ReadOnly)){
            len = fTarget.read(buf, 2048);
            blake2b_update(&ctx, (const uint8_t *)buf, len);
        }
        blake2b_final(&ctx, (uint8_t *)sum, RS_MAX_STRONG_SUM_LENGTH);

        QString strSum;
        for(int i=0;i<RS_MAX_STRONG_SUM_LENGTH;i++){
            printf("%02X", sum[i]&0xFF);
            strSum += QString("%1").arg(int(sum[i]&0xFF), 2, 16, QChar('0'));
        }
        push_log("Target File: "+sFilePath);
        push_log("BLAKE2b Check: "+strSum.toUpper());
    }
}
