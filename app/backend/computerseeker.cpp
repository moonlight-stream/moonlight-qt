#include "computerseeker.h"
#include "computermanager.h"
#include <QTimer>

ComputerSeeker::ComputerSeeker(ComputerManager *manager, QString computerName, QObject *parent)
    : QObject(parent), m_ComputerManager(manager), m_ComputerName(computerName),
      m_TimeoutTimer(new QTimer(this))
{
    m_TimeoutTimer->setSingleShot(true);
    connect(m_TimeoutTimer, &QTimer::timeout,
            this, &ComputerSeeker::onTimeout);
    connect(m_ComputerManager, &ComputerManager::computerStateChanged,
            this, &ComputerSeeker::onComputerUpdated);
}

void ComputerSeeker::start(int timeout, const NvAddress& preferredAddress)
{
    m_TimeoutTimer->start(timeout);

    auto* computer = findMatchingComputer();
    if (!computer) {
        // Only add unknown computers so a lookup never replaces a saved manual address.
        m_ComputerManager->addNewHostManually(m_ComputerName);
    }
    else if (!preferredAddress.isNull() && preferredAddress.port() && m_ComputerName == computer->uuid) {
        // The GUI just reached this host. Try that route before other saved addresses.
        // Polling still verifies the host UUID and pairing status before launching.
        QWriteLocker lock(&computer->lock);
        computer->activeAddress = preferredAddress;
        m_WakeIfOffline = true;
    }
    else {
        computer->wake();
    }

    m_ComputerManager->startPolling();
}

void ComputerSeeker::onComputerUpdated(NvComputer *computer)
{
    if (!m_TimeoutTimer->isActive() || !matchComputer(computer)) {
        return;
    }
    if (isOnline(computer)) {
        m_ComputerManager->stopPollingAsync();
        m_TimeoutTimer->stop();
        emit computerFound(computer);
    }
    else if (m_WakeIfOffline && computer->state == NvComputer::CS_OFFLINE) {
        m_WakeIfOffline = false;
        computer->wake();
    }
}

bool ComputerSeeker::matchComputer(NvComputer *computer) const
{
    QString value = m_ComputerName.toLower();

    if (computer->name.toLower() == value || computer->uuid.toLower() == value) {
        return true;
    }

    const auto uniqueAddresses = computer->uniqueAddresses();
    for (const NvAddress& addr : uniqueAddresses) {
        if (addr.address().toLower() == value || addr.toString().toLower() == value) {
            return true;
        }
    }

    return false;
}

NvComputer* ComputerSeeker::findMatchingComputer() const
{
    const auto computers = m_ComputerManager->getComputers();
    for (NvComputer* computer : computers) {
        if (this->matchComputer(computer)) {
            return computer;
        }
    }

    return nullptr;
}

bool ComputerSeeker::isOnline(NvComputer *computer) const
{
    return computer->state == NvComputer::CS_ONLINE;
}

void ComputerSeeker::onTimeout()
{
    m_TimeoutTimer->stop();
    m_ComputerManager->stopPollingAsync();
    emit errorTimeout();
}
