#ifndef CRYPTO_H
#define CRYPTO_H

#include <QString>

class Crypto {
public:
    // AES-256-CBC encryption with PBKDF2-SHA256 key derivation
    // plaintext: original string
    // password: user-set encryption password
    // returns: Base64-encoded ciphertext (salt:iter:iv:data) or empty on error
    static QString encrypt(const QString& plaintext, const QString& password);

    // decrypt ciphertext encrypted with encrypt()
    // returns: original plaintext (or empty on error)
    static QString decrypt(const QString& ciphertext, const QString& password);

    // Verify password against stored verification token
    static bool verifyPassword(const QString& password, const QString& storedToken);
};

#endif // CRYPTO_H
