/********************************************************************************
** Form generated from reading UI file 'infoform.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_INFOFORM_H
#define UI_INFOFORM_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_InfoForm
{
public:
    QPushButton *connectButton;
    QPushButton *closeButton;
    QLabel *label;
    QLabel *textLb;

    void setupUi(QWidget *InfoForm)
    {
        if (InfoForm->objectName().isEmpty())
            InfoForm->setObjectName(QString::fromUtf8("InfoForm"));
        InfoForm->resize(420, 240);
        InfoForm->setStyleSheet(QString::fromUtf8("background-color: rgb(255, 255, 255);"));
        connectButton = new QPushButton(InfoForm);
        connectButton->setObjectName(QString::fromUtf8("connectButton"));
        connectButton->setGeometry(QRect(130, 150, 160, 50));
        QFont font;
        font.setFamily(QString::fromUtf8("\351\273\221\344\275\223"));
        font.setPointSize(10);
        connectButton->setFont(font);
        connectButton->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"	color: rgb(251, 251, 251);\n"
"	border:0px groove gray;border-radius:5px;padding:2px 4px;border-style: outset;\n"
"	background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, stop:0 rgba(61,163,241, 255), stop:1 rgba(36,95,207, 255));\n"
"}\n"
"QPushButton:hover{\n"
"	color: rgb(251, 251, 251);\n"
"	border:0px groove gray;border-radius:5px;padding:2px 4px;border-style: outset;\n"
"	background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, stop:0 rgba(36, 95, 207, 255), stop:1 rgba(61, 163, 241, 255));\n"
"}\n"
"QPushButton:pressed{\n"
"	color: rgb(251, 251, 251);\n"
"	border:0px groove gray;border-radius:5px;padding:2px 4px;border-style: outset;\n"
"	background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, stop:0 rgba(61,163,241, 255), stop:1 rgba(36,95,207, 255));\n"
"}"));
        closeButton = new QPushButton(InfoForm);
        closeButton->setObjectName(QString::fromUtf8("closeButton"));
        closeButton->setGeometry(QRect(380, 10, 25, 25));
        closeButton->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"image: url(:/UI/images/close-tanchuang.png);\n"
"background-color: transparent;\n"
"border:0px;\n"
"}\n"
"QPushButton:hover{\n"
"image: url(:/UI/images/close-tanchuang.png);\n"
"background-color: transparent;\n"
"border:0px;\n"
"}\n"
"QPushButton:pressed{\n"
"image: url(:/UI/images/close-tanchuang.png);\n"
"background-color: transparent;\n"
"border:0px;\n"
"}"));
        label = new QLabel(InfoForm);
        label->setObjectName(QString::fromUtf8("label"));
        label->setGeometry(QRect(0, 0, 420, 72));
        label->setStyleSheet(QString::fromUtf8("background-image: url(:/UI/images/tanchuangbg.jpg);\n"
"background-color: transparent;"));
        textLb = new QLabel(InfoForm);
        textLb->setObjectName(QString::fromUtf8("textLb"));
        textLb->setGeometry(QRect(60, 90, 300, 40));
        textLb->setFont(font);
        textLb->setAlignment(Qt::AlignHCenter|Qt::AlignTop);
        textLb->setWordWrap(true);
        connectButton->raise();
        label->raise();
        closeButton->raise();
        textLb->raise();

        retranslateUi(InfoForm);

        QMetaObject::connectSlotsByName(InfoForm);
    } // setupUi

    void retranslateUi(QWidget *InfoForm)
    {
        InfoForm->setWindowTitle(QCoreApplication::translate("InfoForm", "Form", nullptr));
        connectButton->setText(QCoreApplication::translate("InfoForm", "\344\270\272\344\270\226\347\225\214\344\270\212\346\211\200\346\234\211\347\232\204\347\276\216\345\245\275\350\200\214\346\210\230", nullptr));
        closeButton->setText(QString());
        label->setText(QString());
        textLb->setText(QCoreApplication::translate("InfoForm", "\347\202\271\345\207\273\345\211\215\345\276\200\345\256\230\346\226\271\347\275\221\347\253\231", nullptr));
    } // retranslateUi

};

namespace Ui {
    class InfoForm: public Ui_InfoForm {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_INFOFORM_H
