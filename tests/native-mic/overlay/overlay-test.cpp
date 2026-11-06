#include "overlaymanager.h"
#include "path.h"
#include <QCoreApplication>
#include <QFile>
#include <cstdio>
#include <cstdlib>
QByteArray Path::readDataFile(QString fileName) {
    QFile file(QString("../../../app/") + fileName);
    if (!file.open(QIODevice::ReadOnly)) return QByteArray();
    return file.readAll();
}
static void check(bool result) { if (!result) std::abort(); }
int main(int argc, char** argv) {
    QCoreApplication app(argc,argv);
    Overlay::OverlayManager overlay;
    auto text = [&] { return QByteArray(overlay.getOverlayText(Overlay::OverlayStatusUpdate)); };
    overlay.setMicrophoneStatus("Microphone ON");
    check(text()=="Microphone ON");
    check(overlay.isOverlayEnabled(Overlay::OverlayStatusUpdate));
    overlay.updateOverlayText(Overlay::OverlayStatusUpdate,"Connection warning");
    overlay.setOverlayState(Overlay::OverlayStatusUpdate,true);
    check(text()=="Connection warning\nMicrophone ON");
    overlay.setMicrophoneStatus("Microphone ON");
    check(text()=="Connection warning\nMicrophone ON");
    overlay.setOverlayState(Overlay::OverlayStatusUpdate,false);
    check(text()=="Microphone ON");
    overlay.updateOverlayText(Overlay::OverlayStatusUpdate,"Gamepad mode");
    overlay.setOverlayState(Overlay::OverlayStatusUpdate,true);
    check(text()=="Gamepad mode\nMicrophone ON");
    overlay.setMicrophoneStatus(QByteArray());
    check(text()=="Gamepad mode");
    overlay.setOverlayState(Overlay::OverlayStatusUpdate,false);
    check(text().isEmpty());
    check(!overlay.isOverlayEnabled(Overlay::OverlayStatusUpdate));
    overlay.updateOverlayText(Overlay::OverlayDebug,"Debug unchanged");
    overlay.setOverlayState(Overlay::OverlayDebug,true);
    check(QByteArray(overlay.getOverlayText(Overlay::OverlayDebug))=="Debug unchanged");
    std::puts("PASS: microphone/connection/gamepad composition and clearing; debug channel preserved");
}
