// Copyright (c) 2011-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_COINCONTROLTREEWIDGET_H
#define BITCOIN_QT_COINCONTROLTREEWIDGET_H

#include <qobjectdefs.h>
#include <qstring.h>
#include <qtreewidget.h>
#include <QKeyEvent>
#include <QTreeWidget>

class QKeyEvent;
class QObject;
class QWidget;

class CoinControlTreeWidget : public QTreeWidget
{
    Q_OBJECT

public:
    explicit CoinControlTreeWidget(QWidget *parent = nullptr);

protected:
    virtual void keyPressEvent(QKeyEvent *event) override;
};

#endif // BITCOIN_QT_COINCONTROLTREEWIDGET_H
