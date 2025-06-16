#ifndef RHIITEM_H
#define RHIITEM_H

#include <QQmlEngine>
#include <QQuickRhiItem>

class RhiItem : public QQuickRhiItem
{
    Q_OBJECT
    QML_ELEMENT
public:
    RhiItem(QQuickItem *parent = nullptr);

    // QQuickRhiItem interface
protected:
    QQuickRhiItemRenderer *createRenderer() final;
};

#endif // RHIITEM_H
