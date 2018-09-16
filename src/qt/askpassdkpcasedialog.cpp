// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "askpassdkpcasedialog.h"
#include "ui_askpassdkpcasedialog.h"

#include "guiconstants.h"
#include "guiutil.h"
#include "walletmodel.h"

#include "allocators.h"

#include <QKeyEvent>
#include <QMessageBox>
#include <QPushButton>

AskPassdkpcaseDialog::AskPassdkpcaseDialog(Mode mode, QWidget* parent, WalletModel* model) : QDialog(parent),
                                                                                           ui(new Ui::AskPassdkpcaseDialog),
                                                                                           mode(mode),
                                                                                           model(model),
                                                                                           fCapsLock(false)
{
    ui->setupUi(this);
    this->setStyleSheet(GUIUtil::loadStyleSheet());

    ui->passEdit1->setMinimumSize(ui->passEdit1->sizeHint());
    ui->passEdit2->setMinimumSize(ui->passEdit2->sizeHint());
    ui->passEdit3->setMinimumSize(ui->passEdit3->sizeHint());

    ui->passEdit1->setMaxLength(MAX_PASSDKPCASE_SIZE);
    ui->passEdit2->setMaxLength(MAX_PASSDKPCASE_SIZE);
    ui->passEdit3->setMaxLength(MAX_PASSDKPCASE_SIZE);

    // Setup Caps Lock detection.
    ui->passEdit1->installEventFilter(this);
    ui->passEdit2->installEventFilter(this);
    ui->passEdit3->installEventFilter(this);

    this->model = model;

    switch (mode) {
    case Encrypt: // Ask passdkpcase x2
        ui->warningLabel->setText(tr("Enter the new passdkpcase to the wallet.<br/>Please use a passdkpcase of <b>ten or more random characters</b>, or <b>eight or more words</b>."));
        ui->passLabel1->hide();
        ui->passEdit1->hide();
        setWindowTitle(tr("Encrypt wallet"));
        break;
    case UnlockAnonymize:
        ui->anonymizationCheckBox->setChecked(true);
        ui->anonymizationCheckBox->show();
    case Unlock: // Ask passdkpcase
        ui->warningLabel->setText(tr("This operation needs your wallet passdkpcase to unlock the wallet."));
        ui->passLabel2->hide();
        ui->passEdit2->hide();
        ui->passLabel3->hide();
        ui->passEdit3->hide();
        setWindowTitle(tr("Unlock wallet"));
        break;
    case Decrypt: // Ask passdkpcase
        ui->warningLabel->setText(tr("This operation needs your wallet passdkpcase to decrypt the wallet."));
        ui->passLabel2->hide();
        ui->passEdit2->hide();
        ui->passLabel3->hide();
        ui->passEdit3->hide();
        setWindowTitle(tr("Decrypt wallet"));
        break;
    case ChangePass: // Ask old passdkpcase + new passdkpcase x2
        setWindowTitle(tr("Change passdkpcase"));
        ui->warningLabel->setText(tr("Enter the old and new passdkpcase to the wallet."));
        break;
    }

    ui->anonymizationCheckBox->setChecked(model->isAnonymizeOnlyUnlocked());

    textChanged();
    connect(ui->passEdit1, SIGNAL(textChanged(QString)), this, SLOT(textChanged()));
    connect(ui->passEdit2, SIGNAL(textChanged(QString)), this, SLOT(textChanged()));
    connect(ui->passEdit3, SIGNAL(textChanged(QString)), this, SLOT(textChanged()));
}

AskPassdkpcaseDialog::~AskPassdkpcaseDialog()
{
    // Attempt to overwrite text so that they do not linger around in memory
    ui->passEdit1->setText(QString(" ").repeated(ui->passEdit1->text().size()));
    ui->passEdit2->setText(QString(" ").repeated(ui->passEdit2->text().size()));
    ui->passEdit3->setText(QString(" ").repeated(ui->passEdit3->text().size()));
    delete ui;
}

void AskPassdkpcaseDialog::accept()
{
    SecureString oldpass, newpass1, newpass2;
    if (!model)
        return;
    oldpass.reserve(MAX_PASSDKPCASE_SIZE);
    newpass1.reserve(MAX_PASSDKPCASE_SIZE);
    newpass2.reserve(MAX_PASSDKPCASE_SIZE);
    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make this input mlock()'d to begin with.
    oldpass.assign(ui->passEdit1->text().toStdString().c_str());
    newpass1.assign(ui->passEdit2->text().toStdString().c_str());
    newpass2.assign(ui->passEdit3->text().toStdString().c_str());

    switch (mode) {
    case Encrypt: {
        if (newpass1.empty() || newpass2.empty()) {
            // Cannot encrypt with empty passdkpcase
            break;
        }
        QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm wallet encryption"),
            tr("Warning: If you encrypt your wallet and lose your passdkpcase, you will <b>LOSE ALL OF YOUR DKPC</b>!") + "<br><br>" + tr("Are you sure you wish to encrypt your wallet?"),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel);
        if (retval == QMessageBox::Yes) {
            if (newpass1 == newpass2) {
                if (model->setWalletEncrypted(true, newpass1)) {
                    QMessageBox::warning(this, tr("Wallet encrypted"),
                        "<qt>" +
                            tr("DarkPayCoin will close now to finish the encryption process. "
                               "Remember that encrypting your wallet cannot fully protect "
                               "your DKPCs from being stolen by malware infecting your computer.") +
                            "<br><br><b>" +
                            tr("IMPORTANT: Any previous backups you have made of your wallet file "
                               "should be replaced with the newly generated, encrypted wallet file. "
                               "For security reasons, previous backups of the unencrypted wallet file "
                               "will become useless as soon as you start using the new, encrypted wallet.") +
                            "</b></qt>");
                    QApplication::quit();
                } else {
                    QMessageBox::critical(this, tr("Wallet encryption failed"),
                        tr("Wallet encryption failed due to an internal error. Your wallet was not encrypted."));
                }
                QDialog::accept(); // Success
            } else {
                QMessageBox::critical(this, tr("Wallet encryption failed"),
                    tr("The supplied passdkpcases do not match."));
            }
        } else {
            QDialog::reject(); // Cancelled
        }
    } break;
    case UnlockAnonymize:
    case Unlock:
        if (!model->setWalletLocked(false, oldpass, ui->anonymizationCheckBox->isChecked())) {
            QMessageBox::critical(this, tr("Wallet unlock failed"),
                tr("The passdkpcase entered for the wallet decryption was incorrect."));
        } else {
            QDialog::accept(); // Success
        }
        break;
    case Decrypt:
        if (!model->setWalletEncrypted(false, oldpass)) {
            QMessageBox::critical(this, tr("Wallet decryption failed"),
                tr("The passdkpcase entered for the wallet decryption was incorrect."));
        } else {
            QDialog::accept(); // Success
        }
        break;
    case ChangePass:
        if (newpass1 == newpass2) {
            if (model->changePassdkpcase(oldpass, newpass1)) {
                QMessageBox::information(this, tr("Wallet encrypted"),
                    tr("Wallet passdkpcase was successfully changed."));
                QDialog::accept(); // Success
            } else {
                QMessageBox::critical(this, tr("Wallet encryption failed"),
                    tr("The passdkpcase entered for the wallet decryption was incorrect."));
            }
        } else {
            QMessageBox::critical(this, tr("Wallet encryption failed"),
                tr("The supplied passdkpcases do not match."));
        }
        break;
    }
}

void AskPassdkpcaseDialog::textChanged()
{
    // Validate input, set Ok button to enabled when acceptable
    bool acceptable = false;
    switch (mode) {
    case Encrypt: // New passdkpcase x2
        acceptable = !ui->passEdit2->text().isEmpty() && !ui->passEdit3->text().isEmpty();
        break;
    case UnlockAnonymize: // Old passdkpcase x1
    case Unlock:          // Old passdkpcase x1
    case Decrypt:
        acceptable = !ui->passEdit1->text().isEmpty();
        break;
    case ChangePass: // Old passdkpcase x1, new passdkpcase x2
        acceptable = !ui->passEdit1->text().isEmpty() && !ui->passEdit2->text().isEmpty() && !ui->passEdit3->text().isEmpty();
        break;
    }
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(acceptable);
}

bool AskPassdkpcaseDialog::event(QEvent* event)
{
    // Detect Caps Lock key press.
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_CapsLock) {
            fCapsLock = !fCapsLock;
        }
        if (fCapsLock) {
            ui->capsLabel->setText(tr("Warning: The Caps Lock key is on!"));
        } else {
            ui->capsLabel->clear();
        }
    }
    return QWidget::event(event);
}

bool AskPassdkpcaseDialog::eventFilter(QObject* object, QEvent* event)
{
    /* Detect Caps Lock.
     * There is no good OS-independent way to check a key state in Qt, but we
     * can detect Caps Lock by checking for the following condition:
     * Shift key is down and the result is a lower case character, or
     * Shift key is not down and the result is an upper case character.
     */
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* ke = static_cast<QKeyEvent*>(event);
        QString str = ke->text();
        if (str.length() != 0) {
            const QChar* psz = str.unicode();
            bool fShift = (ke->modifiers() & Qt::ShiftModifier) != 0;
            if ((fShift && *psz >= 'a' && *psz <= 'z') || (!fShift && *psz >= 'A' && *psz <= 'Z')) {
                fCapsLock = true;
                ui->capsLabel->setText(tr("Warning: The Caps Lock key is on!"));
            } else if (psz->isLetter()) {
                fCapsLock = false;
                ui->capsLabel->clear();
            }
        }
    }
    return QDialog::eventFilter(object, event);
}