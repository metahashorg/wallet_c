#ifndef MESSENGERDBSTORAGE_H
#define MESSENGERDBSTORAGE_H

#include "dbstorage.h"

#include "Message.h"

namespace messenger {

class MessengerDBStorage : public DBStorage
{
public:
    using IdCounterPair = std::pair<DbId, Message::Counter>;
    using NameCounterPair = std::pair<QString, Message::Counter>;

    MessengerDBStorage(const QString &path = QString());

    virtual int currentVersion() const final;

    void addMessage(const QString &user, const QString &duser,
                    const QString &text, const QString &decryptedText, bool isDecrypted, uint64_t timestamp, Message::Counter counter,
                    bool isIncoming, bool canDecrypted, bool isConfirmed,
                    const QString &hash, qint64 fee, const QString &channelSha = QString(""));

    void addMessage(const Message &message);

    void addMessages(const std::vector<Message> &messages);

    DbId getUserId(const QString &username);
    DbId getUserIdOrCreate(const QString &username);
    QStringList getUsersList();

    DbId getContactIdOrCreate(const QString &username);

    QString getUserPublicKey(const QString &username);
    ContactInfo getUserInfo(const QString &username);
    void setUserPublicKey(const QString &username, const QString &publickey, const QString &publicKeyRsa, const QString &txHash, const QString &blockchainName);

    QString getUserSignatures(const QString &username);
    void setUserSignatures(const QString &username, const QString &signatures);

    QString getContactPublicKey(const QString &username);
    ContactInfo getContactInfo(const QString &username);
    void setContactPublicKey(const QString &username, const QString &publickey, const QString &txHash, const QString &blockchainName);

    Message::Counter getMessageMaxCounter(const QString &user, const QString &channelSha = QString());
    Message::Counter getMessageMaxConfirmedCounter(const QString &user);

    std::vector<Message> getMessagesForUser(const QString &user, qint64 from, qint64 to);
    std::vector<Message> getMessagesForUserAndDest(const QString &user, const QString &channelOrContact, qint64 from, qint64 tos, bool isChannel = false);
    std::vector<Message> getMessagesForUserAndDestNum(const QString &user, const QString &channelOrContact, qint64 to, qint64 num, bool isChannel = false);
    qint64 getMessagesCountForUserAndDest(const QString &user, const QString &duser, qint64 from);

    bool hasMessageWithCounter(const QString &username, Message::Counter counter, const QString &channelSha = QString());
    bool hasUnconfirmedMessageWithHash(const QString &username, const QString &hash);

    IdCounterPair findFirstNotConfirmedMessageWithHash(const QString &username, const QString &hash, const QString &channelSha = QString());
    IdCounterPair findFirstMessageWithHash(const QString &username, const QString &hash, const QString &channelSha = QString());
    DbId findFirstNotConfirmedMessage(const QString &username);
    void updateMessage(DbId id, Message::Counter newCounter, bool confirmed);

    Message::Counter getLastReadCounterForUserContact(const QString &username, const QString &channelOrContact, bool isChannel = false);
    void setLastReadCounterForUserContact(const QString &username, const QString &channelOrContact, Message::Counter counter, bool isChannel = false);
    std::vector<NameCounterPair> getLastReadCountersForContacts(const QString &username);
    std::vector<NameCounterPair> getLastReadCountersForChannels(const QString &username);
    std::vector<ChannelInfo> getChannelsWithLastReadCounters(const QString &username);

    void addChannel(DbId userid, const QString &channel, const QString &shaName, bool isAdmin, const QString &adminName, bool isBanned, bool isWriter, bool isVisited);
    void setChannelsNotVisited(const QString &user);
    DbId getChannelForUserShaName(const QString &user, const QString &shaName);
    void updateChannel(DbId id, bool isVisited);
    void setWriterForNotVisited(const QString &user);

    ChannelInfo getChannelInfoForUserShaName(const QString &user, const QString &shaName);
    void setChannelIsWriterForUserShaName(const QString &user, const QString &shaName, bool isWriter);

    void removeDecryptedData();

    std::pair<std::vector<DbId>, std::vector<Message>> getNotDecryptedMessage(const QString &user);

    void updateDecryptedMessage(const std::vector<std::tuple<DbId, bool, QString>> &messages);

protected:
    virtual void createDatabase() final;

private:
    void createMessagesList(QSqlQuery &query, std::vector<Message> &messages, std::vector<DbId> &ids, bool isIDs, bool isChannel, bool reverse);
    void addLastReadRecord(DbId userid, DbId contactid, DBStorage::DbId channelid);
};

}

#endif // MESSENGERDBSTORAGE_H
