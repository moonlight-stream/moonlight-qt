#pragma once

#include <QByteArray>
#include <QString>

// Reading a client identity supplied on standard input by a wrapper application
namespace IdentityStdin
{

// Reads standard input until EOF. Fails if standard input is a terminal or
// holds more than maxBytes, so key material is never read interactively.
bool
readAll(qint64 maxBytes, QByteArray* data, QString* error);

// Parses a JSON object of exactly the form {"certificate": "...", "key": "..."},
// where both members are PEM-encoded strings.
bool
parseJson(const QByteArray& json, QByteArray* pemCert, QByteArray* pemKey, QString* error);

// Overwrites and clears a buffer that held key material. Only effective if the
// caller holds the sole reference to the buffer's data.
void
cleanse(QByteArray& data);

}
