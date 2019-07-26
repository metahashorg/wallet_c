#ifndef INFRASTRUCTURENSLOOKUP_H
#define INFRASTRUCTURENSLOOKUP_H

#include "qt_utilites/CallbackWrapper.h"
#include "qt_utilites/ManagerWrapper.h"

#include <map>
#include <vector>

#include <QString>

class NsLookup;

class InfrastructureNsLookup: public ManagerWrapper {
    Q_OBJECT
private:

    struct Nodes {
        QString torrent;
        QString proxy;

        Nodes(const QString &torrent, const QString &proxy);
    };

public:

    using GetServersCallback = CallbackWrapper<void(const std::vector<QString> &servers)>;

public:

    explicit InfrastructureNsLookup(NsLookup &nsLookup, QObject *parent = nullptr);

signals:

    void getTorrents(const QString &currency, size_t limit, size_t count, const InfrastructureNsLookup::GetServersCallback &callback);

    void getProxy(const QString &currency, size_t limit, size_t count, const InfrastructureNsLookup::GetServersCallback &callback);

private slots:

    void onGetTorrents(const QString &currency, size_t limit, size_t count, const InfrastructureNsLookup::GetServersCallback &callback);

    void onGetProxy(const QString &currency, size_t limit, size_t count, const InfrastructureNsLookup::GetServersCallback &callback);

private:

    template<typename Member>
    void getServers(const QString &currency, const Member &member, size_t limit, size_t count, const InfrastructureNsLookup::GetServersCallback &callback);

private:

    NsLookup &nsLookup;

    std::map<QString, Nodes> infrastructure;

};

#endif // INFRASTRUCTURENSLOOKUP_H