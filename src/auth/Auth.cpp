#include "Auth.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonObject>
#include <QSettings>

#include "check.h"
#include "qt_utilites/SlotWrapper.h"
#include "Paths.h"
#include "qt_utilites/QRegister.h"

#include "AuthJavascript.h"

#include "qt_utilites/ManagerWrapperImpl.h"

#include "utilites/machine_uid.h"

SET_LOG_NAMESPACE("AUTH");

namespace auth {

Auth::Auth(AuthJavascript &javascriptWrapper, QObject *parent)
    : TimerClass(1s, parent)
    , javascriptWrapper(javascriptWrapper)
{
    QSettings settings(getSettingsPath(), QSettings::IniFormat);
    CHECK(settings.contains("servers/auth"), "settings server not found");
    authUrl = settings.value("servers/auth").toString();
    hardwareId = QString::fromStdString(::getMachineUid());
    CHECK(settings.contains("timeouts_sec/auth"), "settings timeout not found");
    timeout = seconds(settings.value("timeouts_sec/auth").toInt());

    readLoginInfo();

    Q_CONNECT(this, &Auth::login, this, &Auth::onLogin);
    Q_CONNECT(this, &Auth::logout, this, &Auth::onLogout);
    Q_CONNECT(this, &Auth::check, this, &Auth::onCheck);
    Q_CONNECT(this, &Auth::forceRefresh, this, &Auth::onForceRefresh);
    Q_CONNECT(this, &Auth::reEmit, this, &Auth::onReEmit);

    Q_CONNECT(this, &Auth::getLoginInfo, this, &Auth::onGetLoginInfo);

    Q_REG(LoginInfoCallback, "LoginInfoCallback");
    Q_REG2(TypedException, "TypedException", false);
    Q_REG(LoginInfo, "LoginInfo");

    tcpClient.setParent(this);
    Q_CONNECT(&tcpClient, &SimpleClient::callbackCall, this, &Auth::callbackCall);
    tcpClient.moveToThread(TimerClass::getThread());

    javascriptWrapper.setAuthManager(*this);

    moveToThread(TimerClass::getThread()); // TODO вызывать в TimerClass
}

Auth::~Auth() {
    TimerClass::exit();
}

void Auth::onLogin(const QString &login, const QString &password) {
BEGIN_SLOT_WRAPPER
    const QString request = makeLoginRequest(login, password);
    tcpClient.sendMessagePost(authUrl, request, [this, login](const SimpleClient::Response &response) {
       if (response.exception.isSet()) {
           QString content = QString::fromStdString(response.exception.content);
           emit javascriptWrapper.sendLoginInfoResponseSig(info, TypedException(TypeErrors::CLIENT_ERROR, !content.isEmpty() ? content.toStdString() : response.exception.description));
       } else {
           const TypedException exception = apiVrapper2([&] {
               info = parseLoginResponse(QString::fromStdString(response.response), login);
               writeLoginInfo();
               isInitialize = true;
               emit logined(isInitialize, info.login);
               emit logined2(isInitialize, info.login, info.token);
           });
           emit javascriptWrapper.sendLoginInfoResponseSig(info, exception);
       }
    }, timeout, true);
END_SLOT_WRAPPER
}

void Auth::onLogout() {
BEGIN_SLOT_WRAPPER
    const TypedException exception = apiVrapper2([&, this] {
        logoutImpl();
    });
    emit javascriptWrapper.sendLoginInfoResponseSig(info, exception);
END_SLOT_WRAPPER
}

void Auth::logoutImpl() {
    info.clear();
    writeLoginInfo();
    isInitialize = true;
    emit logined(isInitialize, "");
    emit logined2(isInitialize, "", "");
}

void Auth::onCheck() {
BEGIN_SLOT_WRAPPER
    emit javascriptWrapper.sendLoginInfoResponseSig(info, TypedException());
END_SLOT_WRAPPER
}

void auth::Auth::finishMethod() {
    // empty
}

void auth::Auth::startMethod() {
    const bool isChecked = checkToken();
    if (isChecked) {
        emit javascriptWrapper.sendLoginInfoResponseSig(info, TypedException());
        emit checkTokenFinished(TypedException());
    }
}

void auth::Auth::timerMethod() {
    checkToken();
}

void auth::Auth::readLoginInfo() {
    QSettings settings(getStoragePath(), QSettings::IniFormat);
    settings.beginGroup("Login");
    info.login = settings.value("login", QString()).toString();
    info.token = settings.value("token", QString()).toString();
    info.refresh = settings.value("refresh", QString()).toString();
    info.isAuth = settings.value("isAuth", false).toBool();
    info.isTest = settings.value("isTest", false).toBool();
    info.expire = seconds(settings.value("expire", 0).toInt());
    settings.endGroup();
}

void Auth::writeLoginInfo() {
    QSettings settings(getStoragePath(), QSettings::IniFormat);
    settings.beginGroup("Login");
    settings.setValue("login", info.login);
    settings.setValue("token", info.token);
    settings.setValue("refresh", info.refresh);
    settings.setValue("isAuth", info.isAuth);
    settings.setValue("isTest", info.isTest);
    settings.setValue("expire", (int)info.expire.count());
    settings.endGroup();
}

void Auth::onForceRefresh() {
BEGIN_SLOT_WRAPPER
    forceRefreshInternal();
END_SLOT_WRAPPER
}

void Auth::forceRefreshInternal() {
    LOG << "Try refresh token ";
    const QString request = makeRefreshTokenRequest(info.refresh);
    const QString token = info.token;

    class GuardRefresh {
    public:

        GuardRefresh(int &guard)
            : guardRefresh(guard)
        {
            guardRefresh++;
            isLock = true;
        }

        void unlock() {
            if (isLock) {
                guardRefresh--;
                isLock = false;
            }
        }

        ~GuardRefresh() {
            unlock();
        }

    private:

        int &guardRefresh;
        bool isLock = false;
    };

    if (guardRefresh != 0) {
        LOG << "Refresh token block";
        return;
    }

    std::shared_ptr<GuardRefresh> guard = std::make_shared<GuardRefresh>(guardRefresh);

    tcpClient.sendMessagePost(authUrl, request, [this, token, guard](const SimpleClient::Response &response) {
        guard->unlock();

        if (info.token != token) {
            return;
        }
        if (response.exception.isSet() && response.exception.code == SimpleClient::ServerException::BAD_REQUEST_ERROR) {
            LOG << "Refresh token failed";
            logoutImpl();
            QString content = QString::fromStdString(response.exception.content);
            const TypedException except(TypeErrors::CLIENT_ERROR, !content.isEmpty() ? content.toStdString() : response.exception.description);
            emit javascriptWrapper.sendLoginInfoResponseSig(info, except);
            emit checkTokenFinished(except);
        } else if (!response.exception.isSet()) {
            const TypedException exception = apiVrapper2([&] {
                const LoginInfo newLogin = parseRefreshTokenResponse(QString::fromStdString(response.response), info.login, info.isTest);
                if (!newLogin.isAuth) {
                    LOG << "Refresh token failed";
                    logoutImpl();
                } else {
                    LOG << "Refresh token ok";
                    info = newLogin;
                    writeLoginInfo();
                    isInitialize = true;
                    emit logined(isInitialize, info.login);
                    emit logined2(isInitialize, info.login, info.token);
                }
            });
            emit javascriptWrapper.sendLoginInfoResponseSig(info, exception);
            emit checkTokenFinished(exception);
        } else {
            CHECK(!response.exception.isSet(), response.exception.description);
        }
    }, timeout, true);
}

bool Auth::checkToken() {
    if (!info.isAuth) {
        isInitialize = true;
        return true;
    }
    const time_point now = ::now();
    const system_time_point systemNow = ::system_now();
    if (now - info.saveTime >= info.expire - minutes(1) || systemNow - info.saveTimeSystem >= info.expire - minutes(1)) {
        LOG << "Token expire";
        forceRefreshInternal();
        return false;
    }

    if (now - info.prevCheck < 1min) {
        return true;
    }

    LOG << "Check token";
    info.prevCheck = now;
    const QString request = makeCheckTokenRequest(info.token);
    const QString token = info.token;
    tcpClient.sendMessagePost(authUrl, request, [this, token](const SimpleClient::Response &response) {
        if (info.token != token) {
            return;
        }

        if (response.exception.isSet() && response.exception.code == SimpleClient::ServerException::BAD_REQUEST_ERROR) {
            forceRefreshInternal();
        } else if (!response.exception.isSet()) {
            const TypedException exception = apiVrapper2([&] {
                const bool res = parseCheckTokenResponse(QString::fromStdString(response.response));
                if (!res) {
                    forceRefreshInternal();
                }
            });
            if (exception.isSet()) {
                emit javascriptWrapper.sendLoginInfoResponseSig(info, exception);
                emit checkTokenFinished(exception);
            }
        } else {
            CHECK(!response.exception.isSet(), response.exception.description);
        }
    }, timeout);
    return false;
}

void Auth::onReEmit() {
BEGIN_SLOT_WRAPPER
    LOG << "Auth Reemit";
    emit logined(isInitialize, info.login);
    emit logined2(isInitialize, info.login, info.token);
END_SLOT_WRAPPER
}

template<typename Func>
void Auth::runCallback(const Func &callback) {
    emit javascriptWrapper.callbackCall(callback);
}

QString Auth::makeLoginRequest(const QString &login, const QString &password) const {
    QJsonObject request;
    request.insert("id", "1");
    request.insert("version", "1.0.0");
    request.insert("method", "user.auth");
    request.insert("uid", hardwareId);
    QJsonArray params;
    QJsonObject p;
    p.insert("login", login);
    p.insert("password", password);
    params.append(p);

    request.insert("params", params);
    return QString(QJsonDocument(request).toJson(QJsonDocument::Compact));
}

QString Auth::makeCheckTokenRequest(const QString &token) const {
    QJsonObject request;
    request.insert("id", "1");
    request.insert("version", "1.0.0");
    request.insert("method", "user.token");
    request.insert("token", token);
    request.insert("uid", hardwareId);
    QJsonObject params;
    request.insert("params", params);
    return QString(QJsonDocument(request).toJson(QJsonDocument::Compact));
}

QString Auth::makeRefreshTokenRequest(const QString &token) const {
    QJsonObject request;
    request.insert("id", "1");
    request.insert("version", "1.0.0");
    request.insert("method", "user.token.refresh");
    request.insert("token", token);
    request.insert("uid", hardwareId);
    QJsonObject params;
    request.insert("params", params);
    return QString(QJsonDocument(request).toJson(QJsonDocument::Compact));
}

LoginInfo Auth::parseLoginResponse(const QString &response, const QString &login) const {
    LoginInfo result;
    result.login = login;
    result.saveTime = ::now();
    result.saveTimeSystem = ::system_now();
    result.prevCheck = ::now();

    const QJsonDocument jsonResponse = QJsonDocument::fromJson(response.toUtf8());
    CHECK(jsonResponse.isObject(), "Incorrect json");
    const QJsonObject &json1 = jsonResponse.object();

    CHECK(json1.contains("result") && json1.value("result").isString(), "Incorrect json: result field not found");

    if (json1.value("result").toString() != "OK") {
        return result;
    }

    CHECK(json1.contains("data") && json1.value("data").isObject(), "Incorrect json: data field not found");
    const QJsonObject &json = json1.value("data").toObject();

    CHECK(json.contains("token") && json.value("token").isString(), "Incorrect json: token field not found");
    result.token = json.value("token").toString();

    CHECK(json.contains("refresh_token") && json.value("refresh_token").isString(), "Incorrect json: refresh_token field not found");
    result.refresh = json.value("refresh_token").toString();

    CHECK(json.contains("is_test_user") && json.value("is_test_user").isBool(), "Incorrect json: is_test_user field not found");
    result.isTest = json.value("is_test_user").toBool();

    CHECK(json.contains("expire") && json.value("expire").isDouble(), "Incorrect json: expire field not found");
    result.expire = seconds(json.value("expire").toInt());

    result.isAuth = !result.token.isEmpty();

    return result;
}

LoginInfo Auth::parseRefreshTokenResponse(const QString &response, const QString &login, bool isTest) const {
    LoginInfo result;
    result.login = login;
    result.isTest = isTest;
    result.saveTime = ::now();
    result.saveTimeSystem = ::system_now();
    result.prevCheck = ::now();

    const QJsonDocument jsonResponse = QJsonDocument::fromJson(response.toUtf8());
    CHECK(jsonResponse.isObject(), "Incorrect json");
    const QJsonObject &json1 = jsonResponse.object();

    CHECK(json1.contains("result") && json1.value("result").isString(), "Incorrect json: result field not found");

    if (json1.value("result").toString() != "OK") {
        return result;
    }

    CHECK(json1.contains("data") && json1.value("data").isObject(), "Incorrect json: data field not found");
    const QJsonObject &json = json1.value("data").toObject();

    CHECK(json.contains("refresh") && json.value("refresh").isString(), "Incorrect json: refresh field not found");
    result.refresh = json.value("refresh").toString();

    CHECK(json.contains("access") && json.value("access").isString(), "Incorrect json: access field not found");
    result.token = json.value("access").toString();

    CHECK(json.contains("expire") && json.value("expire").isDouble(), "Incorrect json: expire field not found");
    result.expire = seconds(json.value("expire").toInt());

    result.isAuth = !result.token.isEmpty();

    return result;
}

bool auth::Auth::parseCheckTokenResponse(const QString &response) const {
    const QJsonDocument jsonResponse = QJsonDocument::fromJson(response.toUtf8());
    CHECK(jsonResponse.isObject(), "Incorrect json");
    const QJsonObject &json = jsonResponse.object();

    CHECK(json.contains("result") && json.value("result").isString(), "Incorrect json: result field not found");

    return (json.value("result").toString() == QStringLiteral("OK"));
}

void Auth::onGetLoginInfo(const LoginInfoCallback &callback) {
BEGIN_SLOT_WRAPPER
    callback.emitFunc(TypedException(), info);
END_SLOT_WRAPPER
}

}
