#include "MidiMappingManager.h"

#include "core/MainController.h"
#include "midi/MidiManager.h"


MidiMappingManager::MidiMappingManager(MainController* controller)
    : QObject(controller)
    , m_controller(controller)
    , m_midi(controller->midi())
    , m_controlThatWaitsForMapping("")
    , m_isWaitingForControl(false)
    , m_connectFeedback(false)
    , m_releaseNextControl(false)
    , m_feedbackEnabled(true)
{
    if (!m_midi) {
        qCritical() << "Could not get MidiManager instance.";
        return;
    }

    connect(m_midi, SIGNAL(messageReceived(MidiEvent)), this, SLOT(onExternalEvent(MidiEvent)));
}

QJsonObject MidiMappingManager::h2qj(const QHash<uint32_t, QVector<QString>> &h) const
{
    QVariantMap vmap;

    for (auto i = h.cbegin(); i != h.cend(); ++i) {
        QJsonArray a;
        for (const auto &v : i.value()) {
            if (!getControlFromUid(v))
                continue;
            a.push_back(v);
        }
        if (a.size() > 0) {
            char key[32];
            sprintf(key, "0x%06x", i.key());

            vmap.insert(key, a);
        }
    }

    return QJsonObject::fromVariantMap(vmap);
}

QJsonObject MidiMappingManager::h2qj(const QHash<QString, QVector<uint32_t>> &h) const
{
    QVariantMap vmap;

    for (auto i = h.cbegin(); i != h.cend(); ++i) {
        if (!getControlFromUid(i.key()))
            continue;

        QJsonArray a;
        for (const auto &v : i.value()) {
            char key[32];
            sprintf(key, "0x%06x", v);
            a.push_back(key);
        }

        if (a.size() > 0) {
            vmap.insert(i.key(), a);
        }
    }

    return QJsonObject::fromVariantMap(vmap);
}

QJsonObject MidiMappingManager::getState() const {
    QJsonObject state;

    state["midiToControl2"] = h2qj(m_midiToControlMappingv2);
    state["controlToFeedback2"] = h2qj(m_controlToFeedbackMappingv2);
    state["feedbackEnabled"] = getFeedbackEnabled();
    return state;
}

void MidiMappingManager::setState(const QJsonObject& state) {
    m_controlToFeedbackMappingv2.clear();
    m_midiToControlMappingv2.clear();

    if (state["controlToFeedback2"].isUndefined() || state["midiToControl2"].isUndefined()) {
        auto mtc = deserialize<QMap<QString, QVector<QString>>>(state["midiToControl"].toString());
        auto ctm = deserialize<QMap<QString, QVector<QString>>>(state["controlToFeedback"].toString());

        for (auto i = ctm.cbegin(); i != ctm.cend(); ++i) {
            for (const auto &v: qAsConst(i.value())) {
                auto address = QByteArray::fromBase64(v.toLatin1());
                uint32_t hexcode = address[0] | address[1] << 8 | address[2] << 16;
                m_controlToFeedbackMappingv2[i.key()].push_back(hexcode);
            }
        }

        for (auto i = mtc.cbegin(); i != mtc.cend(); ++i) {
            auto address = i.key().toLatin1();
            int type, channel, target;
            sscanf(address.constData(), "%03d%03d%03d", &type, &channel, &target);
            uint32_t hexcode = type | channel << 8 | target << 16;
            if (i.value().size() > 0)
                m_midiToControlMappingv2[hexcode] = i.value();
        }

    }
    else {
        auto ctf = state["controlToFeedback2"].toObject().toVariantMap();
        for (auto i = ctf.cbegin(); i != ctf.cend(); ++i) {
            for (const auto &v : i.value().toList()) {
                m_controlToFeedbackMappingv2[i.key()].push_back(v.toString().toInt(nullptr, 0));
            }
        }

        auto mtc = state["midiToControl2"].toObject().toVariantMap();
        for (auto i = mtc.cbegin(); i != mtc.cend(); ++i) {
            auto key = i.key().toInt(nullptr, 0);
            for (const auto &v : i.value().toList()) {
                m_midiToControlMappingv2[key].push_back(v.toString());
            }
        }
    }

    if (!m_controlToFeedbackMappingv2.isEmpty()) {
        setFeedbackEnabled(state["feedbackEnabled"].toBool());
    }
}

// ------------- interface that is accessable from GUI: -------------

void MidiMappingManager::startMappingMidiToControl() {
    m_isWaitingForControl = true;
    m_connectFeedback = false;
}

void MidiMappingManager::startMappingMidiToControlWithFeedback(){
    m_isWaitingForControl = true;
    m_connectFeedback = true;
}

void MidiMappingManager::cancelMappingMidiToControl() {
    m_midi->clearNextEventCallbacks();
    m_controlThatWaitsForMapping = "";
    m_isWaitingForControl = false;
}

void MidiMappingManager::releaseNextControlMapping() {
    m_releaseNextControl = true;
}

void MidiMappingManager::cancelReleaseNextControlMapping() {
    m_releaseNextControl = false;
}

void MidiMappingManager::releaseNextMidiEventMapping() {
    m_midi->registerForNextEvent("releaseConnectionToControls",
                    [this](MidiEvent event) { this->releaseMapping(event); });
}

void MidiMappingManager::cancelReleaseNextMidiEventMapping() {
    m_midi->clearNextEventCallbacks();
}

// ----------------- interface to other classes: -----------------

void MidiMappingManager::registerGuiControl(QQuickItem* item, QString controlUid) {
    if (!item) return;
    if (controlUid.isEmpty()) return;
    m_registeredControls[controlUid] = item;
}

void MidiMappingManager::unregisterGuiControl(QString controlUid) {
    if (controlUid.isEmpty()) return;

    // if unregister happens during a loading process, the mapping should not be removed
    // because it could be that there is a block in the old project with the same UID as
    // a block in a (cloned) new project and the old block is deleted(later) when the
    // new mapping was already set
    // not releasing the mapping while loading a project is not problematic
    // as the new mapping is set in this process anyway and the old mapping is discarded
    // CHANGE: mapping is never released, because it is unknown if the block is deleted by the user
    // or if just the GUI item was destroyed because it was invisible
//    if (!m_controller->projectManager()->m_loadingIsInProgress) {
//        // only release mapping if no project is loading
//        releaseMapping(controlUid);
//    }
    m_registeredControls.remove(controlUid);
}

QQuickItem* MidiMappingManager::getControlFromUid(QString controlUid) const {
    if (!m_registeredControls.contains(controlUid)) return nullptr;
    return m_registeredControls[controlUid];
}

void MidiMappingManager::guiControlHasBeenTouched(QString controllerUid) {
    if (m_releaseNextControl) {
        releaseMapping(controllerUid);
        m_releaseNextControl = false;
    }
    if (!m_isWaitingForControl) return;  // <- most likely (in case of normal user input)

    m_isWaitingForControl = false;
    m_controlThatWaitsForMapping = controllerUid;

    m_midi->registerForNextEvent("inputConnection",
                    [this](MidiEvent event) { this->mapWaitingControlToMidi(event); });
}

void MidiMappingManager::sendFeedback(QString uid, double value) const {
    if (!m_feedbackEnabled) return;

    for (const auto &feedbackAddress: m_controlToFeedbackMappingv2.value(uid, QVector<uint32_t>())) {
        m_midi->sendFeedback(feedbackAddress, value);
    }
}

void MidiMappingManager::clearMapping() {
    m_midiToControlMappingv2.clear();
    m_controlToFeedbackMappingv2.clear();
}

// ---------------------- private -------------------------

void MidiMappingManager::mapWaitingControlToMidi(const MidiEvent& event) {
    if (m_controlThatWaitsForMapping.isEmpty()) return;
    mapControlToMidi(m_controlThatWaitsForMapping, event);
    m_controlThatWaitsForMapping = "";
}

void MidiMappingManager::mapControlToMidi(QString controlUid, const MidiEvent& event) {
    // append control uid to list if not already existing:
    if (!m_midiToControlMappingv2[event.GetId()].contains(controlUid)) {
        m_midiToControlMappingv2[event.GetId()].append(controlUid);
    }
    if (m_connectFeedback) {
        auto id = event.GetId();
        if (!m_controlToFeedbackMappingv2[controlUid].contains(id)) {
            m_controlToFeedbackMappingv2[controlUid].append(id);
        }
    }
}

void MidiMappingManager::releaseMapping(QString controlUid) {
    for (auto& controlList: m_midiToControlMappingv2) {
        controlList.removeAll(controlUid);
    }
    m_controlToFeedbackMappingv2.remove(controlUid);
}

void MidiMappingManager::releaseMapping(const MidiEvent& event) {
    m_midiToControlMappingv2.remove(event.GetId());

    auto id = event.GetId();
    for (auto& feedbackAddressList: m_controlToFeedbackMappingv2) {
        feedbackAddressList.removeAll(id);
    }
}

void MidiMappingManager::onExternalEvent(const MidiEvent& event) const {
    auto id = event.GetId();

    for (const auto &controlUid: m_midiToControlMappingv2.value(id, QVector<QString>())) {
        QQuickItem* control = getControlFromUid(controlUid);
        // check if control still exists:
        if (!control)
            continue;
        control->setProperty("externalInput", event.value);
    }
}
