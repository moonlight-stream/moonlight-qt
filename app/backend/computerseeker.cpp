#include "computerseeker.h"
#include "computermanager.h"
#include <QTimer>

ComputerSeeker::ComputerSeeker(ComputerManager *manager, QString computerName, QObject *parent)
    : QObject(parent), m_ComputerManager(manager), m_ComputerName(computerName),
      m_TimeoutTimer(new QTimer(this))
{
    // If we know this computer, send a WOL packet to wake it up in case it is asleep.
    NvComputer* matchingComputer = findMatchingComputer();
    if (matchingComputer) {
        matchingComputer->wake();
    }

    m_TimeoutTimer->setSingleShot(true);
    connect(m_TimeoutTimer, &QTimer::timeout,
            this, &ComputerSeeker::onTimeout);
    connect(m_ComputerManager, &ComputerManager::computerStateChanged,
            this, &ComputerSeeker::onComputerUpdated);
}

void ComputerSeeker::start(int timeout)
{
    m_TimeoutTimer->start(timeout);

    // If we don't know this computer by name, address, or UUID, try adding it
    // manually and see if we can find it by address, hostname, or mDNS.
    //
    // NB: We don't do this unconditionally because it will wipe out the user's
    // manual address if they pass another reachable hostname/address.
    if (!findMatchingComputer()) {
        m_ComputerManager->addNewHostManually(m_ComputerName);
    }

    m_ComputerManager->startPolling();
}

void ComputerSeeker::onComputerUpdated(NvComputer *computer)
{
    if (!m_TimeoutTimer->isActive()) {
        return;
    }
    if (matchComputer(computer) && isOnline(computer)) {
        m_ComputerManager->stopPollingAsync();
        m_TimeoutTimer->stop();
        emit computerFound(computer);
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
