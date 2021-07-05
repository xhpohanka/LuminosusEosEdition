#include "EosFaderBankBlock.h"

#include "core/MainController.h"
#include "core/Nodes.h"


EosFaderBankBlock::EosFaderBankBlock(MainController* controller, QString uid)
    : OneOutputBlock(controller, uid)
    , m_bankIndex(QString::number(controller->eosManager()->getNewFaderBankNumber()))
    , m_page(1)
    , m_eosVersion(0)
    , m_bankLabel("")
    , m_numFaders(this, "numFaders", 10, 1, 24)
    , m_faderLabels(m_numFaders)
    , m_faderLevels(m_numFaders)
    , m_externalLevels(m_numFaders)
    , m_externalLevelsValid(m_numFaders, false)
    , m_faderSync(m_numFaders, false)
    , m_feedbackInvalid(m_numFaders, false)
    , m_catchFaders(this, "catchFaders", true)
    , m_lastExtTime(m_numFaders)
    , m_lastOscTime(m_numFaders)
    , m_feedbackEnabled(this, "feedbackEnabled", true)
{
    connect(m_controller->eosManager(), SIGNAL(connectionEstablished()),
            this, SLOT(onEosConnectionEstablished()));
    connect(controller->eosManager(), SIGNAL(eosMessageReceived(EosOSCMessage)),
            this, SLOT(onIncomingEosMessage(EosOSCMessage)));
    connect(controller->eosManager(), SIGNAL(connectionReset()),
            this, SLOT(onConnectionReset()));
    connect(&m_numFaders, &IntegerAttribute::valueChanged, this, &EosFaderBankBlock::updateFaderCount);
    connect(m_controller->midi(), SIGNAL(inputConnected()),
            this, SLOT(onMidiConnected()));

    sendConfigMessage();
}

void EosFaderBankBlock::getAdditionalState(QJsonObject& state) const {
    state["page"] = getPage();
}

void EosFaderBankBlock::setAdditionalState(const QJsonObject& state) {
    setPageFromGui(state["page"].toInt());
}

void EosFaderBankBlock::setPageFromGui(int value) {
    if (value == m_page) return;
    m_page = limit(1, value, getMaxFaderPage());
    m_faderLevels.fill(0.0);
    m_faderSync.fill(false);
    emit faderLevelsChanged();
    sendConfigMessage();
    emit pageChanged();
    setValue(m_page);
}

void EosFaderBankBlock::setFaderLabelFromOsc(int faderIndex, QString label) {
    if (faderIndex < 0 || faderIndex > m_numFaders - 1) return;
    m_faderLabels[faderIndex] = label;
    m_feedbackInvalid[faderIndex] = false;
    if (m_eosVersion < 31) {
        if (label == "Global FX" || label == "Man Time")
            m_feedbackInvalid[faderIndex] = true;
    }

    emit faderLabelsChanged();
}

void EosFaderBankBlock::setFaderLevel(int faderIndex, qreal value) {
    if (faderIndex < 0 || faderIndex > m_numFaders - 1) return;
    value = limit(0, value, 1);

    m_faderLevels[faderIndex] = value;

    QString message = "/eos/user/1/fader/%1/%2";
    message = message.arg(m_bankIndex, QString::number(faderIndex + 1));
    m_controller->lightingConsole()->sendMessage(message, value);
    m_lastOscTime[faderIndex].restart();
}

void EosFaderBankBlock::setFaderLevelFromGui(int faderIndex, qreal value) {
    m_faderSync[faderIndex] = false;
    setFaderLevel(faderIndex, value);

    // emitting faderLevelsChanged() not actually neccessary
    // because value in GUI is already up-to-date?
}

void EosFaderBankBlock::setFaderLevelFromExt(int faderIndex, qreal value) {
    const double thresh = 1.0/1024; // we support 10bit resolution at best (mackie control)
    if (m_catchFaders) {
        if (!m_faderSync[faderIndex]) {
            if (m_externalLevelsValid[faderIndex]) {
                if ((m_externalLevels[faderIndex] <= m_faderLevels[faderIndex] && (value + thresh) >= m_faderLevels[faderIndex])
                        || (m_externalLevels[faderIndex] >= m_faderLevels[faderIndex] && (value - thresh) <= m_faderLevels[faderIndex]))
                    m_faderSync[faderIndex] = true;
            }
            if (abs(value - m_faderLevels[faderIndex]) < thresh * 2) {
                m_faderSync[faderIndex] = true;
            }
        }
        m_externalLevels[faderIndex] = value;
        m_externalLevelsValid[faderIndex] = true;

        if (!m_faderSync[faderIndex])
            return;
    }

    setFaderLevel(faderIndex, value);
    m_lastExtTime[faderIndex].restart();
    emit faderLevelsChanged();
}

void EosFaderBankBlock::setFaderLevelFromOsc(int faderIndex, qreal value){
    if (faderIndex < 0 || faderIndex > m_numFaders - 1) return;
    if (m_feedbackInvalid[faderIndex]) return;

    // by docs eos fader levels are sent 3 seconds after last osc command
    // sometimes however it seems that there are some glitches
    if (m_lastOscTime[faderIndex].isValid() && m_lastOscTime[faderIndex].elapsed() < 1500) return;

    // asynchronous osc feedback can break sync so give it some time
    if (m_lastExtTime[faderIndex].isValid() && m_lastExtTime[faderIndex].elapsed() > 200) {
        if (abs(value - m_externalLevels[faderIndex]) > m_catchThresh)
            m_faderSync[faderIndex] = false;
        else
            m_faderSync[faderIndex] = true;
    }

    m_faderLevels[faderIndex] = limit(0, value, 1);

    emit faderLevelsChanged();
}

void EosFaderBankBlock::sendLoadEvent(int faderIndex, bool value) {
    QString message = "/eos/user/1/fader/%1/%2/load";
    message = message.arg(m_bankIndex, QString::number(faderIndex + 1));
    m_controller->lightingConsole()->sendMessage(message, value ? 1.0 : 0.0);
}

void EosFaderBankBlock::sendUnloadEvent(int faderIndex) {
    QString message = "/eos/user/1/fader/%1/%2/unload";
    message = message.arg(m_bankIndex, QString::number(faderIndex + 1));
    m_controller->lightingConsole()->sendMessage(message);
}

void EosFaderBankBlock::sendStopEvent(int faderIndex, bool value) {
    QString message = "/eos/user/1/fader/%1/%2/stop";
    message = message.arg(m_bankIndex, QString::number(faderIndex + 1));
    m_controller->lightingConsole()->sendMessage(message, value ? 1.0 : 0.0);
}

void EosFaderBankBlock::sendFireEvent(int faderIndex, bool value) {
    QString message = "/eos/user/1/fader/%1/%2/fire";
    message = message.arg(m_bankIndex, QString::number(faderIndex + 1));
    m_controller->lightingConsole()->sendMessage(message, value ? 1.0 : 0.0);
}

void EosFaderBankBlock::sendBlEvent(bool value) {
    QString message = "/eos/user/1/key/blackout";
    m_controller->lightingConsole()->sendMessage(message, value ? 1.0 : 0.0);
}

void EosFaderBankBlock::sendPageMinusEvent() {
    if (m_page == 1) {
        setPageFromGui(getMaxFaderPage());
    } else {
        setPageFromGui(m_page - 1);
    }
}

void EosFaderBankBlock::sendPagePlusEvent() {
    if (m_page == getMaxFaderPage()) {
        setPageFromGui(1);
    } else {
        setPageFromGui(m_page + 1);
    }
}

void EosFaderBankBlock::onEosConnectionEstablished() {
    sendConfigMessage();

    QString eosVersion = m_controller->eosManager()->getConsoleVersion();
    if (eosVersion.startsWith("2.4.")) {
        m_eosVersion = 24;
    }
    else if (eosVersion.startsWith("3.0.")) {
        m_eosVersion = 30;
    }
    else if (eosVersion.startsWith("3.1.")) {
        m_eosVersion = 31;
    }
}

void EosFaderBankBlock::onMidiConnected() {
    m_faderSync.fill(false);
    m_externalLevelsValid.fill(false);
}

void EosFaderBankBlock::sendConfigMessage() {
    // send /eos/fader/<index>/config/<page>/10 command:
    QString message = "/eos/user/1/fader/%1/config/%2/%3";
    message = message.arg(m_bankIndex, QString::number(m_page), QString::number(m_numFaders));
    m_controller->lightingConsole()->sendMessage(message);
}

void EosFaderBankBlock::onIncomingEosMessage(const EosOSCMessage& msg) {
    // check if this message belongs to this FaderBank:
    if (msg.pathPart(0) != "fader" || msg.pathPart(1) != m_bankIndex) {
        // it doesn't -> ignore it:
        return;
    }

    if (msg.path().size() == 2 && msg.arguments().size() >= 1) {
        // this message contains a descriptive label for this fader bank
        // most of the time it is the number of the page
        // /eos/out/fader/<index>=<label>
        setBankLabel(msg.stringValue());
        int page = msg.stringValue().toInt();
        if (page >= 1 && page <= 100) {
            m_page = page;
            setValue(m_page);
            emit pageChanged();
        }
    } else if (msg.pathPart(3) == "name") {
        // this message contains the label for a specific fader in this bank
        // /eos/out/fader/<index>/<faderNumber>/name=<label>
        int faderNumber = msg.pathPart(2).toInt();
        QString label = msg.stringValue();
        setFaderLabelFromOsc(faderNumber - 1, label);
    } else if ( msg.path().size() == 3) {
        // this message contains the current level of a fader
        // /eos/fader/<index>/<fader>=<level> (not /eos/out/ !)
        int faderNumber = msg.pathPart(2).toInt();
        double level = msg.numericValue();
        setFaderLevelFromOsc(faderNumber - 1, level);
    }
}

void EosFaderBankBlock::onConnectionReset() {
    sendConfigMessage();
}

int EosFaderBankBlock::getMaxFaderPage() {
    if (m_eosVersion > 23) {
        return 100;
    } else {
        return 30;
    }
}

void EosFaderBankBlock::updateFaderCount() {
    m_faderLabels.resize(m_numFaders.getValue());
    m_faderLevels.resize(m_numFaders.getValue());
    m_externalLevels.resize(m_numFaders.getValue());
    m_externalLevelsValid.resize(m_numFaders.getValue());
    m_faderSync.resize(m_numFaders.getValue());
    m_feedbackInvalid.resize(m_numFaders.getValue());
    m_lastExtTime.resize(m_numFaders.getValue());
    m_lastOscTime.resize(m_numFaders.getValue());

    for (auto &tim : m_lastExtTime)
        tim.invalidate();
    for (auto &tim : m_lastOscTime)
        tim.invalidate();

    m_page = 1;
    setValue(m_page);
    emit pageChanged();
    m_outputNode->updateConnectionLines();

    sendConfigMessage();
}
