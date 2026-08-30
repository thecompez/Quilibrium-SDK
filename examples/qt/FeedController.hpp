#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class FeedController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QVariantList casts READ casts NOTIFY castsChanged)
public:
    explicit FeedController(QObject* parent = nullptr);
    [[nodiscard]] bool busy() const noexcept { return busy_; }
    [[nodiscard]] QString error() const { return error_; }
    [[nodiscard]] QVariantList casts() const { return casts_; }
    Q_INVOKABLE void loadTrending();
signals:
    void busyChanged();
    void errorChanged();
    void castsChanged();
private:
    bool busy_{false};
    QString error_{};
    QVariantList casts_{};
};
