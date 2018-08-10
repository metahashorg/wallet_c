#ifndef MESSENGERMESSAGES_H
#define MESSENGERMESSAGES_H

#include <QJsonDocument>

#include <vector>

#include "Messenger.h"
#include "Message.h"

QString makeTextForSignRegisterRequest(const QString &address, const QString &rsaPubkeyHex, uint64_t fee);

QString makeTextForGetPubkeyRequest(const QString &address);

QString makeTextForSendMessageRequest(const QString &address, const QString &dataHex, uint64_t fee);

QString makeTextForGetMyMessagesRequest();

QString makeTextForGetChannelRequest();

QString makeTextForGetChannelsRequest();

QString makeTextForMsgAppendKeyOnlineRequest();

QString makeRegisterRequest(const QString &rsaPubkeyHex, const QString &pubkeyAddressHex, const QString &signHex, uint64_t fee);

QString makeGetPubkeyRequest(const QString &address, const QString &pubkeyHex, const QString &signHex);

QString makeSendMessageRequest(const QString &toAddress, const QString &dataHex, const QString &pubkeyHex, const QString &signHex, uint64_t fee, uint64_t timestamp);

QString makeGetMyMessagesRequest(const QString &pubkeyHex, const QString &signHex, Message::Counter from, Message::Counter to);

QString makeAppendKeyOnlineRequest(const QString &pubkeyHex, const QString &signHex);

enum METHOD: int {
    APPEND_KEY_TO_ADDR = 0, GET_KEY_BY_ADDR = 1, SEND_TO_ADDR = 2, NEW_MSGS = 3, NEW_MSG = 4, COUNT_MESSAGES = 5, NOT_SET = 1000
};

struct ResponseType {
    METHOD method = METHOD::NOT_SET;
    QString address;
    bool isError;
    QString error;
};

struct NewMessageResponse {
    QString data;
    bool isInput;
    Message::Counter counter;
    uint64_t timestamp;
    QString collocutor;

    bool operator< (const NewMessageResponse &second) const {
        return this->counter < second.counter;
    }
};

ResponseType getMethodAndAddressResponse(const QJsonDocument &response);

NewMessageResponse parseNewMessageResponse(const QJsonDocument &response);

std::vector<NewMessageResponse> parseNewMessagesResponse(const QJsonDocument &response);

Message::Counter parseCountMessagesResponse(const QJsonDocument &response);

std::pair<QString, QString> parseKeyMessageResponse(const QJsonDocument &response);

#endif // MESSENGERMESSAGES_H
