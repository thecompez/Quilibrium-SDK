#include "FeedController.hpp"

#include <QMetaObject>
#include <QVariantMap>
#include <QtConcurrent/QtConcurrentRun>

import quilibrium;

FeedController::FeedController(QObject* parent) : QObject(parent) {}

void FeedController::loadTrending() {
    if (busy_) return;
    busy_ = true;
    error_.clear();
    emit busyChanged();
    emit errorChanged();

    QtConcurrent::run([this] {
        QVariantList output;
        QString failure;
        auto connected = quilibrium::connect();
        if (!connected) {
            failure = QString::fromStdString(connected.error().message);
        } else {
            auto page = quilibrium::sync_wait(connected->hypersnap().feeds().trending(30));
            if (!page) {
                failure = QString::fromStdString(page.error().message);
            } else {
                for (const auto& cast : page->casts) {
                    QVariantMap item;
                    item.insert("hash", QString::fromStdString(cast.hash));
                    item.insert("username", QString::fromStdString(cast.author.username));
                    item.insert("displayName", QString::fromStdString(cast.author.display_name));
                    item.insert("text", QString::fromStdString(cast.text));
                    item.insert("likes", QVariant::fromValue<qulonglong>(cast.likes));
                    output.push_back(item);
                }
            }
        }

        QMetaObject::invokeMethod(this, [this, output = std::move(output), failure = std::move(failure)]() mutable {
            casts_ = std::move(output);
            error_ = std::move(failure);
            busy_ = false;
            emit castsChanged();
            emit errorChanged();
            emit busyChanged();
        }, Qt::QueuedConnection);
    });
}
