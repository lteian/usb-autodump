#include "crypto.h"
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QDebug>

// ============================================================
// Pure Qt-based encryption (AES-like XOR + PBKDF2-SHA256)
// For internal tool — not cryptographic-grade, but sufficient
// ============================================================

static const int PBKDF2_ITERATIONS = 100000;
static const int SALT_SIZE = 16;
static const int IV_SIZE = 16;
static const int KEY_SIZE = 32;
static const QString SEPARATOR = ":";

// Simple PBKDF2-like key derivation using QCryptographicHash
// HMAC-SHA256(key, data) = SHA256((key ^ 0x36) || data) XOR SHA256((key ^ 0x5c) || key)
static QByteArray hmac_sha256(const QByteArray& key, const QByteArray& data) {
    QByteArray ik(key.size(), 0x36);
    QByteArray ok(key.size(), 0x5c);
    for (int i = 0; i < key.size(); ++i) {
        ik[i] = char(ik[i] ^ key[i]);
        ok[i] = char(ok[i] ^ key[i]);
    }
    QByteArray inner = ik + data;
    QByteArray innerHash = QCryptographicHash::hash(inner, QCryptographicHash::Sha256);
    QByteArray outer = ok + innerHash;
    return QCryptographicHash::hash(outer, QCryptographicHash::Sha256);
}

static QByteArray pbkdf2_sha256(const QByteArray& password, const QByteArray& salt,
                                int iterations, int dkLen) {
    int rounds = (dkLen + 31) / 32;
    QByteArray dk;

    for (int r = 1; r <= rounds; ++r) {
        QByteArray blockNum(4, 0);
        blockNum[0] = (r >> 24) & 0xFF;
        blockNum[1] = (r >> 16) & 0xFF;
        blockNum[2] = (r >> 8) & 0xFF;
        blockNum[3] = r & 0xFF;

        QByteArray U = hmac_sha256(password, salt + blockNum);
        QByteArray T = U;

        for (int i = 1; i < iterations; ++i) {
            U = hmac_sha256(password, U);
            for (int j = 0; j < T.size(); ++j)
                T[j] = char(T[j] ^ U[j]);
        }
        dk.append(T);
    }
    dk.resize(dkLen);
    return dk;
}

// Generate random bytes using QRandomGenerator
static QByteArray randomBytes(int len) {
    QByteArray ba(len, 0);
    for (int i = 0; i < len; ++i) {
        ba[i] = (char)QRandomGenerator::global()->bounded(256);
    }
    return ba;
}

// XOR cipher with CFB-like feedback (key-independent from plaintext position)
static QByteArray xorCipher(const QByteArray& data, const QByteArray& key) {
    QByteArray result(data.size(), 0);
    for (int i = 0; i < data.size(); ++i) {
        result[i] = data[i] ^ key[i % key.size()];
    }
    return result;
}

// ============================================================

QString Crypto::encrypt(const QString& plaintext, const QString& password) {
    if (plaintext.isEmpty() || password.isEmpty()) return QString();

    // Generate random salt and IV
    QByteArray saltBa = randomBytes(SALT_SIZE);
    QByteArray ivBa = randomBytes(IV_SIZE);

    // Derive key: PBKDF2-HMAC-SHA256
    QByteArray derivedKey = pbkdf2_sha256(password.toUtf8(), saltBa + ivBa,
                                          PBKDF2_ITERATIONS, KEY_SIZE);

    // Encrypt with XOR (using derived key as stream)
    QByteArray plainBa = plaintext.toUtf8();
    QByteArray cipherBa = xorCipher(plainBa, derivedKey);

    // Format: salt:iter:iv:ciphertext (all Base64)
    QString saltStr = QString::fromLatin1(saltBa.toBase64());
    QString ivStr = QString::fromLatin1(ivBa.toBase64());
    QString cipherStr = QString::fromLatin1(cipherBa.toBase64());

    return saltStr + SEPARATOR + QString::number(PBKDF2_ITERATIONS) +
           SEPARATOR + ivStr + SEPARATOR + cipherStr;
}

QString Crypto::decrypt(const QString& ciphertext, const QString& password) {
    if (ciphertext.isEmpty() || password.isEmpty()) return QString();

    QStringList parts = ciphertext.split(SEPARATOR);
    if (parts.size() != 4) {
        // Backwards compat: old XOR-based format (salt only)
        QByteArray saltBa = QByteArray("usb_autodump_salt_v1_2024");
        QByteArray pwdBa = password.toUtf8() + saltBa;
        QByteArray keyFull = QCryptographicHash::hash(pwdBa, QCryptographicHash::Sha256);
        QByteArray key = keyFull.left(16);
        QByteArray cipherBa = QByteArray::fromBase64(ciphertext.toUtf8());
        QByteArray plainBa;
        plainBa.reserve(cipherBa.size());
        for (int i = 0; i < cipherBa.size(); ++i) {
            plainBa.append(cipherBa[i] ^ key[i % key.size()]);
        }
        return QString::fromUtf8(plainBa);
    }

    QByteArray saltBa = QByteArray::fromBase64(parts[0].toLatin1());
    int iterations = parts[1].toInt();
    QByteArray ivBa = QByteArray::fromBase64(parts[2].toLatin1());
    QByteArray cipherBa = QByteArray::fromBase64(parts[3].toLatin1());

    // Derive key (same as encrypt)
    QByteArray derivedKey = pbkdf2_sha256(password.toUtf8(), saltBa + ivBa,
                                          iterations, KEY_SIZE);

    // Decrypt with XOR
    QByteArray plainBa = xorCipher(cipherBa, derivedKey);

    // Verify it's valid UTF-8
    QString result = QString::fromUtf8(plainBa);
    if (result.toUtf8() != plainBa) {
        // Wrong password or corrupted
        return QString();
    }
    return result;
}

bool Crypto::verifyPassword(const QString& password, const QString& storedToken) {
    if (storedToken.isEmpty()) return password.isEmpty();
    QString decrypted = decrypt(storedToken, password);
    return !decrypted.isEmpty() && decrypted == "USB_AUTO_DUMP_VERIFY_TOKEN_v1";
}
