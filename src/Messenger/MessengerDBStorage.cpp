#include "MessengerDBStorage.h"

#include "MessengerDBRes.h"
#include <QtSql>

#include "check.h"

namespace messenger {

static const QString databaseFileName = "messenger.db";
static const QString databaseVersion = "1";

MessengerDBStorage::MessengerDBStorage(const QString &path)
    : DBStorage(path, databaseFileName)
{

}

void MessengerDBStorage::init(bool force)
{
    if (dbExist() && !force)
        return;
    DBStorage::init(force);
    createTable(QStringLiteral("msg_users"), createMsgUsersTable);
    createTable(QStringLiteral("msg_contacts"), createMsgContactsTable);
    createTable(QStringLiteral("msg_channels"), createMsgChannelsTable);
    createTable(QStringLiteral("msg_messages"), createMsgMessagesTable);
    createTable(QStringLiteral("msg_lastreadmessage"), createMsgLastReadMessageTable);
    createIndex(createMsgMessageUniqueIndex);
}

void MessengerDBStorage::addMessage(const QString &user, const QString &duser, const QString &text, uint64_t timestamp, Message::Counter counter, bool isIncoming, bool canDecrypted, bool isConfirmed, const QString &hash, qint64 fee, const QString &channelSha)
{
    DbId userid = getUserId(user);
    DbId contactid = -1;
    if (!duser.isNull())
        contactid = getContactId(duser);
    DbId channelid = -1;
    if (!channelSha.isNull())
        channelid = getChannelForUserShaName(user, channelSha);

    QSqlQuery query(database());
    CHECK(query.prepare(insertMsgMessages), query.lastError().text().toStdString());
    query.bindValue(":userid", userid);
    if (contactid == -1)
        query.bindValue(":contactid", QVariant());
    else
        query.bindValue(":contactid", contactid);
    query.bindValue(":order", counter);
    query.bindValue(":dt", static_cast<qint64>(timestamp));
    query.bindValue(":text", text);
    query.bindValue(":isIncoming", isIncoming);
    query.bindValue(":canDecrypted", canDecrypted);
    query.bindValue(":isConfirmed", isConfirmed);
    query.bindValue(":hash", hash);
    query.bindValue(":fee", fee);
    if (channelid == -1)
        query.bindValue(":channelid", QVariant());
    else
        query.bindValue(":channelid", channelid);
    if (!query.exec()) {
        qDebug() << "ERROR" << query.lastError().type();
    }
    //CHECK(query.exec(), query.lastError().text().toStdString());
    addLastReadRecord(userid, contactid, channelid);
}

DBStorage::DbId MessengerDBStorage::getUserId(const QString &username)
{
    QSqlQuery query(database());
    CHECK(query.prepare(selectMsgUsersForName), query.lastError().text().toStdString());
    query.bindValue(":username", username);
    CHECK(query.exec(), query.lastError().text().toStdString());
    if (query.next()) {
        return query.value("id").toLongLong();
    }

    CHECK(query.prepare(insertMsgUsers), query.lastError().text().toStdString());
    query.bindValue(":username", username);
    CHECK(query.exec(), query.lastError().text().toStdString());
    return query.lastInsertId().toLongLong();
}

QStringList MessengerDBStorage::getUsersList()
{
    QStringList res;
    QSqlQuery query(database());
    CHECK(query.prepare(selectMsgUsersList), query.lastError().text().toStdString());
    CHECK(query.exec(), query.lastError().text().toStdString());
    while (query.next()) {
        res.push_back(query.value("username").toString());
    }
    return res;
}

DBStorage::DbId MessengerDBStorage::getContactId(const QString &username)
{
    QSqlQuery query(database());
    CHECK(query.prepare(selectMsgContactsForName), query.lastError().text().toStdString());
    query.bindValue(":username", username);
    CHECK(query.exec(), query.lastError().text().toStdString());
    if (query.next()) {
        return query.value("id").toLongLong();
    }

    CHECK(query.prepare(insertMsgContacts), query.lastError().text().toStdString());
    query.bindValue(":username", username);
    CHECK(query.exec(), query.lastError().text().toStdString());
    return query.lastInsertId().toLongLong();
}

QString MessengerDBStorage::getUserPublicKey(const QString &username)
{
    QSqlQuery query(database());
    CHECK(query.prepare(selectMsgUserPublicKey), query.lastError().text().toStdString());
    query.bindValue(":user", username);
    CHECK(query.exec(), query.lastError().text().toStdString());
    if (query.next()) {
        return query.value("publickey").toString();
    }
    return QString();
}

void MessengerDBStorage::setUserPublicKey(const QString &username, const QString &publickey)
{
    getUserId(username);
    QSqlQuery query(database());
    CHECK(query.prepare(updateMsgUserPublicKey), query.lastError().text().toStdString());
    query.bindValue(":user", username);
    query.bindValue(":publickey", publickey);
    CHECK(query.exec(), query.lastError().text().toStdString());
}

QString MessengerDBStorage::getUserSignatures(const QString &username)
{
    QSqlQuery query(database());
    CHECK(query.prepare(selectMsgUserSignatures), query.lastError().text().toStdString());
    query.bindValue(":user", username);
    CHECK(query.exec(), query.lastError().text().toStdString());
    if (query.next()) {
        return query.value("signatures").toString();
    }
    return QString();
}

void MessengerDBStorage::setUserSignatures(const QString &username, const QString &signatures)
{
    getUserId(username);
    QSqlQuery query(database());
    CHECK(query.prepare(updateMsgUserSignatures), query.lastError().text().toStdString());
    query.bindValue(":user", username);
    query.bindValue(":signatures", signatures);
    CHECK(query.exec(), query.lastError().text().toStdString());
}

QString MessengerDBStorage::getContactrPublicKey(const QString &username)
{
    QSqlQuery query(database());
    CHECK(query.prepare(selectMsgContactsPublicKey), query.lastError().text().toStdString());
    query.bindValue(":user", username);
    CHECK(query.exec(), query.lastError().text().toStdString());
    if (query.next()) {
        return query.value("publickey").toString();
    }
    return QString();
}

void MessengerDBStorage::setContactPublicKey(const QString &username, const QString &publickey)
{
    getContactId(username);
    QSqlQuery query(database());
    CHECK(query.prepare(updateMsgContactsPublicKey), query.lastError().text().toStdString());
    query.bindValue(":user", username);
    query.bindValue(":publickey", publickey);
    CHECK(query.exec(), query.lastError().text().toStdString());
}

Message::Counter MessengerDBStorage::getMessageMaxCounter(const QString &user, const QString &channelSha)
{
    QSqlQuery query(database());
    const QString sql = selectMsgMaxCounter
            .arg(channelSha.isEmpty() ? QStringLiteral("") : selectJoinChannel)
            .arg(channelSha.isEmpty() ? selectWhereIsNotChannel : QStringLiteral(""));
    CHECK(query.prepare(sql), query.lastError().text().toStdString());
    query.bindValue(":user", user);
    if (!channelSha.isEmpty())
        query.bindValue(":channelSha", channelSha);
    CHECK(query.exec(), query.lastError().text().toStdString());
    if (query.next()) {
        return query.value("max").toLongLong();
    }
    return -1;
}

Message::Counter MessengerDBStorage::getMessageMaxConfirmedCounter(const QString &user)
{
    QSqlQuery query(database());
    CHECK(query.prepare(selectMsgMaxConfirmedCounter), query.lastError().text().toStdString());
    query.bindValue(":user", user);
    CHECK(query.exec(), query.lastError().text().toStdString());
    if (query.next()) {
        return query.value("max").toLongLong();
    }
    return -1;
}

std::vector<Message> MessengerDBStorage::getMessagesForUser(const QString &user, qint64 from, qint64 to)
{
    std::vector<Message> res;
    QSqlQuery query(database());
    CHECK(query.prepare(selectMsgMessagesForUser), query.lastError().text().toStdString());
    query.bindValue(":user", user);
    query.bindValue(":ob", from);
    query.bindValue(":oe", to);
    CHECK(query.exec(), query.lastError().text().toStdString());
    createMessagesList(query, res);
    return res;
}

std::vector<Message> MessengerDBStorage::getMessagesForUserAndDest(const QString &user, const QString &duser, qint64 from, qint64 to, bool isChannel)
{
    std::vector<Message> res;
    QSqlQuery query(database());
    const QString sql = selectMsgMessagesForUserAndDest.arg(isChannel ? selectWhereIsChannel : selectWhereIsNotChannel);
    CHECK(query.prepare(sql), query.lastError().text().toStdString());
    query.bindValue(":user", user);
    query.bindValue(":duser", duser);
    query.bindValue(":ob", from);
    query.bindValue(":oe", to);
    CHECK(query.exec(), query.lastError().text().toStdString());
    createMessagesList(query, res);
    return res;
}

std::vector<Message> MessengerDBStorage::getMessagesForUserAndDestNum(const QString &user, const QString &duser, qint64 to, qint64 num, bool isChannel)
{
    std::vector<Message> res;
    QSqlQuery query(database());
    const QString sql = selectMsgMessagesForUserAndDestNum.arg(isChannel ? selectWhereIsChannel : selectWhereIsNotChannel);
    CHECK(query.prepare(sql), query.lastError().text().toStdString());
    query.bindValue(":user", user);
    query.bindValue(":duser", duser);
    query.bindValue(":oe", to);
    query.bindValue(":num", num);
    CHECK(query.exec(), query.lastError().text().toStdString());
    createMessagesList(query, res, true);
    return res;
}

qint64 MessengerDBStorage::getMessagesCountForUserAndDest(const QString &user, const QString &duser, qint64 from)
{
    QSqlQuery query(database());
    CHECK(query.prepare(selectMsgCountMessagesForUserAndDest), query.lastError().text().toStdString());
    query.bindValue(":user", user);
    query.bindValue(":duser", duser);
    query.bindValue(":ob", from);
    CHECK(query.exec(), query.lastError().text().toStdString());
    if (query.next()) {
        return query.value("count").toLongLong();
    }
    return 0;
}

bool MessengerDBStorage::hasMessageWithCounter(const QString &username, Message::Counter counter, const QString &channelSha)
{
    QSqlQuery query(database());
    const QString sql = selectCountMessagesWithCounter
            .arg(channelSha.isEmpty() ? QStringLiteral("") : selectJoinChannel)
            .arg(channelSha.isEmpty() ? selectWhereIsNotChannel : QStringLiteral(""));
    CHECK(query.prepare(sql), query.lastError().text().toStdString());
    query.bindValue(":user", username);
    query.bindValue(":counter", counter);
    if (!channelSha.isEmpty())
        query.bindValue(":channelSha", channelSha);
    CHECK(query.exec(), query.lastError().text().toStdString());
    if (query.next()) {
        return query.value("res").toBool();
    }
    return false;
}

bool MessengerDBStorage::hasUnconfirmedMessageWithHash(const QString &username, const QString &hash)
{
    QSqlQuery query(database());
    CHECK(query.prepare(selectCountNotConfirmedMessagesWithHash), query.lastError().text().toStdString());
    query.bindValue(":user", username);
    query.bindValue(":hash", hash);
    CHECK(query.exec(), query.lastError().text().toStdString());
    if (query.next()) {
        return query.value("res").toBool();
    }
    return false;
}

MessengerDBStorage::IdCounterPair MessengerDBStorage::findFirstNotConfirmedMessageWithHash(const QString &username, const QString &hash, const QString &channelSha)
{
    QSqlQuery query(database());
    const QString sql = selectFirstNotConfirmedMessageWithHash
    .arg(channelSha.isEmpty() ? QStringLiteral("") : selectJoinChannel)
    .arg(channelSha.isEmpty() ? selectWhereIsNotChannel : QStringLiteral(""));
    CHECK(query.prepare(sql), query.lastError().text().toStdString());
    query.bindValue(":user", username);
    query.bindValue(":hash", hash);
    if (!channelSha.isEmpty())
        query.bindValue(":channelSha", channelSha);
    CHECK(query.exec(), query.lastError().text().toStdString());
    if (query.next()) {
        return MessengerDBStorage::IdCounterPair(query.value("id").toLongLong(),
                                        query.value("morder").toLongLong());
    }
    return MessengerDBStorage::IdCounterPair(-1, -1);
}

MessengerDBStorage::IdCounterPair MessengerDBStorage::findFirstMessageWithHash(const QString &username, const QString &hash, const QString &channelSha)
{
    QSqlQuery query(database());
    const QString sql = selectFirstMessageWithHash
    .arg(channelSha.isEmpty() ? QStringLiteral("") : selectJoinChannel)
    .arg(channelSha.isEmpty() ? selectWhereIsNotChannel : QStringLiteral(""));
    CHECK(query.prepare(sql), query.lastError().text().toStdString());
    query.bindValue(":user", username);
    query.bindValue(":hash", hash);
    if (!channelSha.isEmpty())
        query.bindValue(":channelSha", channelSha);
    CHECK(query.exec(), query.lastError().text().toStdString());
    if (query.next()) {
        return MessengerDBStorage::IdCounterPair(query.value("id").toLongLong(),
                                        query.value("morder").toLongLong());
    }
    return MessengerDBStorage::IdCounterPair(-1, -1);
}

DBStorage::DbId MessengerDBStorage::findFirstNotConfirmedMessage(const QString &username)
{
    QSqlQuery query(database());
    CHECK(query.prepare(selectFirstNotConfirmedMessage), query.lastError().text().toStdString());
    query.bindValue(":user", username);
    CHECK(query.exec(), query.lastError().text().toStdString());
    if (query.next()) {
        return query.value("id").toLongLong();
    }
    return -1;
}

void MessengerDBStorage::updateMessage(DbId id, Message::Counter newCounter, bool confirmed)
{
    QSqlQuery query(database());
    CHECK(query.prepare(updateMessageQuery), query.lastError().text().toStdString());
    query.bindValue(":id", id);
    query.bindValue(":counter", newCounter);
    query.bindValue(":isConfirmed", confirmed);
    query.exec();
    qDebug() << query.lastError().text();
    //CHECK(query.exec(), query.lastError().text().toStdString());
}

Message::Counter MessengerDBStorage::getLastReadCounterForUserContact(const QString &username, const QString &channelOrContact, bool isChannel)
{
    QSqlQuery query(database());
    if (isChannel) {
        CHECK(query.prepare(selectLastReadCounterForUserChannel), query.lastError().text().toStdString());
    } else {
        CHECK(query.prepare(selectLastReadCounterForUserContact), query.lastError().text().toStdString());
    }
    query.bindValue(":user", username);
    if (isChannel)
        query.bindValue(":contact", channelOrContact);
    else
        query.bindValue(":shaName", channelOrContact);
    CHECK(query.exec(), query.lastError().text().toStdString());
    if (query.next()) {
        return query.value("lastcounter").toLongLong();
    }
    return -1;
}

void MessengerDBStorage::setLastReadCounterForUserContact(const QString &username, const QString &channelOrContact, Message::Counter counter, bool isChannel)
{
    QSqlQuery query(database());
    if (isChannel) {
        CHECK(query.prepare(updateLastReadCounterForUserChannel), query.lastError().text().toStdString());
    } else {
        CHECK(query.prepare(updateLastReadCounterForUserContact), query.lastError().text().toStdString());
    }
    query.bindValue(":counter", counter);
    query.bindValue(":user", username);
    if (isChannel)
        query.bindValue(":contact", channelOrContact);
    else
        query.bindValue(":shaName", channelOrContact);
    CHECK(query.exec(), query.lastError().text().toStdString());
}

std::vector<MessengerDBStorage::NameCounterPair> MessengerDBStorage::getLastReadCountersForContacts(const QString &username)
{
    std::vector<MessengerDBStorage::NameCounterPair> res;
    QSqlQuery query(database());
    CHECK(query.prepare(selectLastReadCountersForContacts), query.lastError().text().toStdString());
    query.bindValue(":user", username);
    CHECK(query.exec(), query.lastError().text().toStdString());
    while (query.next()) {
        NameCounterPair p(query.value("username").toString(), query.value("lastcounter").toLongLong());
        res.push_back(p);
    }
    return res;
}

std::vector<MessengerDBStorage::NameCounterPair> MessengerDBStorage::getLastReadCountersForChannels(const QString &username)
{
    std::vector<MessengerDBStorage::NameCounterPair> res;
    QSqlQuery query(database());
    CHECK(query.prepare(selectLastReadCountersForChannels), query.lastError().text().toStdString());
    query.bindValue(":user", username);
    CHECK(query.exec(), query.lastError().text().toStdString());
    while (query.next()) {
        NameCounterPair p(query.value("shaName").toString(), query.value("lastcounter").toLongLong());
        res.push_back(p);
    }
    return res;
}

void MessengerDBStorage::addChannel(DBStorage::DbId userid, const QString &channel, const QString &shaName, bool isAdmin, const QString &adminName, bool isBanned, bool isWriter, bool isVisited)
{
    QSqlQuery query(database());
    CHECK(query.prepare(insertMsgChannels), query.lastError().text().toStdString());
    query.bindValue(":userid", userid);
    query.bindValue(":channel", channel);
    query.bindValue(":shaName", shaName);
    query.bindValue(":isAdmin", isAdmin);
    query.bindValue(":adminName", adminName);
    query.bindValue(":isBanned", isBanned);
    query.bindValue(":isWriter", isWriter);
    query.bindValue(":isVisited", isVisited);
    qDebug() << query.lastQuery();
    CHECK(query.exec(), query.lastError().text().toStdString());
}

void MessengerDBStorage::setChannelsNotVisited(const QString &user)
{
    QSqlQuery query(database());
    CHECK(query.prepare(updateSetChannelsNotVisited), query.lastError().text().toStdString());
    query.bindValue(":user", user);
    CHECK(query.exec(), query.lastError().text().toStdString());
}

DBStorage::DbId MessengerDBStorage::getChannelForUserShaName(const QString &user, const QString &shaName)
{
    QSqlQuery query(database());
    CHECK(query.prepare(selectChannelForUserShaName), query.lastError().text().toStdString());
    query.bindValue(":user", user);
    query.bindValue(":shaName", shaName);
    CHECK(query.exec(), query.lastError().text().toStdString());
    if (query.next()) {
        return query.value("id").toLongLong();
    }
    return -1;
}

void MessengerDBStorage::updateChannel(DBStorage::DbId id, bool isVisited)
{
    QSqlQuery query(database());
    CHECK(query.prepare(updateChannelInfo), query.lastError().text().toStdString());
    query.bindValue(":id", id);
    query.bindValue(":isVisited", isVisited);
    CHECK(query.exec(), query.lastError().text().toStdString());
}

void MessengerDBStorage::setWriterForNotVisited(const QString &user)
{
    QSqlQuery query(database());
    CHECK(query.prepare(updatetWriterForNotVisited), query.lastError().text().toStdString());
    query.bindValue(":user", user);
    CHECK(query.exec(), query.lastError().text().toStdString());
}

ChannelInfo MessengerDBStorage::getChannelInfoForUserShaName(const QString &user, const QString &shaName)
{
    ChannelInfo info;
    QSqlQuery query(database());
    CHECK(query.prepare(selectChannelInfoForUserShaName), query.lastError().text().toStdString());
    query.bindValue(":user", user);
    query.bindValue(":shaName", shaName);
    CHECK(query.exec(), query.lastError().text().toStdString());
    if (!query.next()) {

    }
    info.title = query.value("channel").toString();;
    info.titleSha = query.value("shaName").toString();;
    info.admin = query.value("adminName").toString();;
    info.isWriter = query.value("isWriter").toBool();;
    return info;
}

void MessengerDBStorage::setChannelIsWriterForUserShaName(const QString &user, const QString &shaName, bool isWriter)
{
    QSqlQuery query(database());
    CHECK(query.prepare(updateChannelIsWriterForUserShaName), query.lastError().text().toStdString());
    query.bindValue(":user", user);
    query.bindValue(":shaName", shaName);
    query.bindValue(":isWriter", isWriter);
    qDebug() << query.lastQuery();
    CHECK(query.exec(), query.lastError().text().toStdString());
}

void MessengerDBStorage::createMessagesList(QSqlQuery &query, std::vector<Message> &messages, bool reverse)
{
    while (query.next()) {
        Message msg;
        msg.collocutor = query.value("duser").toString();
        msg.isInput = query.value("isIncoming").toBool();
        msg.data = query.value("text").toString();
        msg.counter = query.value("morder").toLongLong();
        msg.timestamp = static_cast<quint64>(query.value("dt").toLongLong());
        msg.fee = query.value("fee").toLongLong();
        msg.isCanDecrypted = query.value("canDecrypted").toBool();
        msg.isConfirmed = query.value("isConfirmed").toBool();
        messages.push_back(msg);
    }
    if (reverse)
        std::reverse(std::begin(messages), std::end(messages));
}

void MessengerDBStorage::addLastReadRecord(DBStorage::DbId userid, DBStorage::DbId contactid, DBStorage::DbId channelid)
{
    QSqlQuery query(database());
    CHECK(query.prepare(selectLastReadMessageCount), query.lastError().text().toStdString());
    query.bindValue(":userid", userid);
    query.bindValue(":contactid", contactid);
    CHECK(query.exec(), query.lastError().text().toStdString());
    if (!query.next()) {
        return;
    }
    qint64 count = query.value("res").toLongLong();
    if (count > 0)
        return;
    CHECK(query.prepare(insertLastReadMessageRecord), query.lastError().text().toStdString());
    query.bindValue(":userid", userid);
    query.bindValue(":contactid", contactid);
    CHECK(query.exec(), query.lastError().text().toStdString());
}

}