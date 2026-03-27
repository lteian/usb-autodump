#include "crypto.h"
#include <QByteArray>
#include <QDebug>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

#ifndef NT_SUCCESS
#define NT_SUCCESS(status) ((NTSTATUS)(status) >= 0)
#endif

// BCrypt function pointer types (NTAPI = __cdecl on x86, fastcall on x64)
typedef NTSTATUS (NTAPI *BCryptOpenAlgorithmProviderFn)(BCRYPT_ALG_HANDLE *, LPCWSTR, LPCWSTR, ULONG);
typedef NTSTATUS (NTAPI *BCryptCloseAlgorithmProviderFn)(BCRYPT_ALG_HANDLE, ULONG);
typedef NTSTATUS (NTAPI *BCryptSetPropertyFn)(BCRYPT_HANDLE, LPCWSTR, PUCHAR, ULONG, ULONG);
typedef NTSTATUS (NTAPI *BCryptGenerateSymmetricKeyFn)(BCRYPT_ALG_HANDLE, BCRYPT_KEY_HANDLE *, PVOID, ULONG, PUCHAR, ULONG, ULONG);
typedef NTSTATUS (NTAPI *BCryptDestroyKeyFn)(BCRYPT_KEY_HANDLE);
typedef NTSTATUS (NTAPI *BCryptEncryptFn)(BCRYPT_KEY_HANDLE, PUCHAR, ULONG, PVOID, PUCHAR, ULONG, PUCHAR, ULONG, ULONG *, ULONG);
typedef NTSTATUS (NTAPI *BCryptDecryptFn)(BCRYPT_KEY_HANDLE, PUCHAR, ULONG, PVOID, PUCHAR, ULONG, PUCHAR, ULONG, ULONG *, ULONG);
typedef NTSTATUS (NTAPI *BCryptDeriveKeyPBKDF2Fn)(BCRYPT_ALG_HANDLE, PUCHAR, ULONG, PUCHAR, ULONG, ULONG64, PUCHAR, ULONG, ULONG);

static const DWORD AES_KEY_SIZE = 256;
static const DWORD BLOCK_SIZE = 16;
static const DWORD PBKDF2_ITERATIONS = 100000;
static const DWORD SALT_SIZE = 16;
static const QString SEPARATOR = ":";

static QString lastCryptoError;

static QString getLastError() {
    return lastCryptoError;
}

QString Crypto::encrypt(const QString& plaintext, const QString& password) {
    if (plaintext.isEmpty() || password.isEmpty()) return QString();
    lastCryptoError.clear();

    QByteArray pwdBa = password.toUtf8();

    // Generate random salt
    QByteArray saltBa(SALT_SIZE, 0);
    HCRYPTPROV hProv = 0;
    if (!CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        lastCryptoError = "CryptAcquireContext failed";
        return QString();
    }
    CryptGenRandom(hProv, SALT_SIZE, (BYTE*)saltBa.data());
    CryptReleaseContext(hProv, 0);

    // Generate random IV
    QByteArray ivBa(BLOCK_SIZE, 0);
    if (!CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        lastCryptoError = "CryptAcquireContext failed";
        return QString();
    }
    CryptGenRandom(hProv, BLOCK_SIZE, (BYTE*)ivBa.data());
    CryptReleaseContext(hProv, 0);

    // PBKDF2 key derivation using BCrypt
    BCryptOpenAlgorithmProviderFn pBCryptOpenAlgorithmProvider = (BCryptOpenAlgorithmProviderFn)
        GetProcAddress(GetModuleHandleW(L"bcrypt.dll"), "BCryptOpenAlgorithmProvider");
    BCryptDeriveKeyPBKDF2Fn pBCryptDeriveKeyPBKDF2 = (BCryptDeriveKeyPBKDF2Fn)
        GetProcAddress(GetModuleHandleW(L"bcrypt.dll"), "BCryptDeriveKeyPBKDF2");
    BCryptEncryptFn pBCryptEncrypt = (BCryptEncryptFn)
        GetProcAddress(GetModuleHandleW(L"bcrypt.dll"), "BCryptEncrypt");
    BCryptDestroyKeyFn pBCryptDestroyKey = (BCryptDestroyKeyFn)
        GetProcAddress(GetModuleHandleW(L"bcrypt.dll"), "BCryptDestroyKey");
    BCryptCloseAlgorithmProviderFn pBCryptCloseAlgorithmProvider = (BCryptCloseAlgorithmProviderFn)
        GetProcAddress(GetModuleHandleW(L"bcrypt.dll"), "BCryptCloseAlgorithmProvider");

    if (!pBCryptOpenAlgorithmProvider || !pBCryptDeriveKeyPBKDF2 || !pBCryptEncrypt ||
        !pBCryptDestroyKey || !pBCryptCloseAlgorithmProvider) {
        lastCryptoError = "BCrypt functions not found";
        return QString();
    }

    BCryptAlgorithmHandle hAlg;
    NTSTATUS status = pBCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0);
    if (!NT_SUCCESS(status)) {
        lastCryptoError = "BCryptOpenAlgorithmProvider failed";
        return QString();
    }

    // Derive key using PBKDF2
    QByteArray derivedKey(SALT_SIZE, 0);
    status = pBCryptDeriveKeyPBKDF2(hAlg, (PUCHAR)pwdBa.constData(), pwdBa.size(),
        (PUCHAR)saltBa.constData(), SALT_SIZE, PBKDF2_ITERATIONS,
        (PUCHAR)derivedKey.data(), SALT_SIZE, 0);
    pBCryptCloseAlgorithmProvider(hAlg, 0);
    if (!NT_SUCCESS(status)) {
        lastCryptoError = "BCryptDeriveKeyPBKDF2 failed";
        return QString();
    }

    // Open AES algorithm for encryption
    BCryptAlgorithmHandle hAesAlg;
    status = pBCryptOpenAlgorithmProvider(&hAesAlg, BCRYPT_AES_ALGORITHM, NULL, 0);
    if (!NT_SUCCESS(status)) {
        lastCryptoError = "BCryptOpenAlgorithmProvider AES failed";
        return QString();
    }

    // Set chain mode to CBC
    wchar_t chainModeStr[] = L"CBC";
    pBCryptSetProperty((void*)hAesAlg, BCRYPT_CHAIN_MODE_PROPERTY, (PUCHAR)chainModeStr,
        (ULONG)wcslen(chainModeStr) * sizeof(wchar_t), 0);

    // Generate symmetric key
    BCryptKeyHandle hKey;
    status = pBCryptGenerateSymmetricKey(hAesAlg, &hKey, NULL, 0,
        (PUCHAR)derivedKey.data(), derivedKey.size(), 0);
    pBCryptCloseAlgorithmProvider(hAesAlg, 0);
    if (!NT_SUCCESS(status)) {
        lastCryptoError = "BCryptGenerateSymmetricKey failed";
        return QString();
    }

    // PKCS7 padding
    QByteArray plainBa = plaintext.toUtf8();
    int pad = BLOCK_SIZE - (plainBa.size() % BLOCK_SIZE);
    QByteArray padded = plainBa;
    padded.append(QByteArray(pad, char(pad)));

    ULONG resultLen = 0;
    status = pBCryptEncrypt(hKey, (PUCHAR)padded.data(), padded.size(),
        NULL, (PUCHAR)ivBa.data(), BLOCK_SIZE,
        NULL, 0, &resultLen, BCRYPT_PADDING_PKCS7_FLAG);
    if (!NT_SUCCESS(status)) {
        pBCryptDestroyKey(hKey);
        lastCryptoError = "BCryptEncrypt (size query) failed";
        return QString();
    }

    QByteArray cipherBa(resultLen, 0);
    status = pBCryptEncrypt(hKey, (PUCHAR)padded.data(), padded.size(),
        NULL, (PUCHAR)ivBa.data(), BLOCK_SIZE,
        (PUCHAR)cipherBa.data(), resultLen, &resultLen, BCRYPT_PADDING_PKCS7_FLAG);
    pBCryptDestroyKey(hKey);

    if (!NT_SUCCESS(status)) {
        lastCryptoError = "BCryptEncrypt failed";
        return QString();
    }

    // Format: salt:iter:iv:ciphertext (all Base64)
    QString saltStr = QString::fromLatin1(saltBa.toBase64());
    QString ivStr = QString::fromLatin1(ivBa.toBase64());
    QString cipherStr = QString::fromLatin1(cipherBa.toBase64());

    return saltStr + SEPARATOR + QString::number(PBKDF2_ITERATIONS) + SEPARATOR + ivStr + SEPARATOR + cipherStr;
}

QString Crypto::decrypt(const QString& ciphertext, const QString& password) {
    if (ciphertext.isEmpty() || password.isEmpty()) return QString();
    lastCryptoError.clear();

    QStringList parts = ciphertext.split(SEPARATOR);
    if (parts.size() != 4) {
        // Try old format (simple base64) for backwards compat
        QByteArray saltBa = QByteArray("usb_autodump_salt_v1_2024");
        QByteArray pwdBa = password.toUtf8() + saltBa;
        QByteArray keyFull;
        keyFull.resize(32);
        // Simple SHA256
        HCRYPTPROV hProv = 0;
        if (!CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
            return QString();
        }
        HCRYPTHASH hHash = 0;
        CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash);
        CryptHashData(hHash, (BYTE*)pwdBa.constData(), pwdBa.size(), 0);
        DWORD len = 32;
        CryptGetHashParam(hHash, HP_HASHVAL, (BYTE*)keyFull.data(), &len, 0);
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
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
    QByteArray pwdBa = password.toUtf8();

    BCryptDeriveKeyPBKDF2Fn pBCryptDeriveKeyPBKDF2 = (BCryptDeriveKeyPBKDF2Fn)
        GetProcAddress(GetModuleHandleW(L"bcrypt.dll"), "BCryptDeriveKeyPBKDF2");
    BCryptOpenAlgorithmProviderFn pBCryptOpenAlgorithmProvider = (BCryptOpenAlgorithmProviderFn)
        GetProcAddress(GetModuleHandleW(L"bcrypt.dll"), "BCryptOpenAlgorithmProvider");
    BCryptDecryptFn pBCryptDecrypt = (BCryptDecryptFn)
        GetProcAddress(GetModuleHandleW(L"bcrypt.dll"), "BCryptDecrypt");
    BCryptDestroyKeyFn pBCryptDestroyKey = (BCryptDestroyKeyFn)
        GetProcAddress(GetModuleHandleW(L"bcrypt.dll"), "BCryptDestroyKey");
    BCryptCloseAlgorithmProviderFn pBCryptCloseAlgorithmProvider = (BCryptCloseAlgorithmProviderFn)
        GetProcAddress(GetModuleHandleW(L"bcrypt.dll"), "BCryptCloseAlgorithmProvider");

    if (!pBCryptDeriveKeyPBKDF2 || !pBCryptOpenAlgorithmProvider || !pBCryptDecrypt ||
        !pBCryptDestroyKey || !pBCryptCloseAlgorithmProvider) {
        lastCryptoError = "BCrypt functions not found";
        return QString();
    }

    BCryptAlgorithmHandle hAlg;
    NTSTATUS status = pBCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0);
    if (!NT_SUCCESS(status)) {
        lastCryptoError = "BCryptOpenAlgorithmProvider failed";
        return QString();
    }

    QByteArray derivedKey(SALT_SIZE, 0);
    status = pBCryptDeriveKeyPBKDF2(hAlg, (PUCHAR)pwdBa.constData(), pwdBa.size(),
        (PUCHAR)saltBa.constData(), saltBa.size(), iterations,
        (PUCHAR)derivedKey.data(), SALT_SIZE, 0);
    pBCryptCloseAlgorithmProvider(hAlg, 0);
    if (!NT_SUCCESS(status)) {
        lastCryptoError = "BCryptDeriveKeyPBKDF2 failed";
        return QString();
    }

    BCryptAlgorithmHandle hAesAlg;
    status = pBCryptOpenAlgorithmProvider(&hAesAlg, BCRYPT_AES_ALGORITHM, NULL, 0);
    if (!NT_SUCCESS(status)) {
        lastCryptoError = "BCryptOpenAlgorithmProvider AES failed";
        return QString();
    }

    wchar_t chainModeStrDec[] = L"CBC";
    pBCryptSetProperty(hAesAlg, BCRYPT_CHAIN_MODE_PROPERTY, (PUCHAR)chainModeStrDec,
        (ULONG)wcslen(chainModeStrDec) * sizeof(wchar_t), 0);

    BCryptKeyHandle hKey;
    status = pBCryptGenerateSymmetricKey(hAesAlg, &hKey, NULL, 0,
        (PUCHAR)derivedKey.data(), derivedKey.size(), 0);
    pBCryptCloseAlgorithmProvider(hAesAlg, 0);
    if (!NT_SUCCESS(status)) {
        lastCryptoError = "BCryptGenerateSymmetricKey failed";
        return QString();
    }

    ULONG resultLen = 0;
    status = pBCryptDecrypt(hKey, (PUCHAR)cipherBa.data(), cipherBa.size(),
        NULL, (PUCHAR)ivBa.data(), BLOCK_SIZE,
        NULL, 0, &resultLen, BCRYPT_PADDING_PKCS7_FLAG);
    if (!NT_SUCCESS(status)) {
        pBCryptDestroyKey(hKey);
        lastCryptoError = "BCryptDecrypt (size query) failed";
        return QString();
    }

    QByteArray plainBa(resultLen, 0);
    status = pBCryptDecrypt(hKey, (PUCHAR)cipherBa.data(), cipherBa.size(),
        NULL, (PUCHAR)ivBa.data(), BLOCK_SIZE,
        (PUCHAR)plainBa.data(), resultLen, &resultLen, BCRYPT_PADDING_PKCS7_FLAG);
    pBCryptDestroyKey(hKey);

    if (!NT_SUCCESS(status)) {
        lastCryptoError = "BCryptDecrypt failed (wrong password?)";
        return QString();
    }

    // Remove PKCS7 padding
    if (!plainBa.isEmpty()) {
        int pad = (unsigned char)plainBa.back();
        if (pad > 0 && pad <= BLOCK_SIZE && pad <= plainBa.size()) {
            bool validPad = true;
            for (int i = plainBa.size() - pad; i < plainBa.size(); ++i) {
                if ((unsigned char)plainBa[i] != (unsigned char)pad) {
                    validPad = false;
                    break;
                }
            }
            if (validPad) {
                plainBa.resize(plainBa.size() - pad);
            }
        }
    }

    return QString::fromUtf8(plainBa);
}

bool Crypto::verifyPassword(const QString& password, const QString& storedToken) {
    if (storedToken.isEmpty()) return password.isEmpty();
    QString decrypted = decrypt(storedToken, password);
    return !decrypted.isEmpty() && decrypted == "USB_AUTO_DUMP_VERIFY_TOKEN_v1";
}

#else
// Non-Windows fallback (simplified)
#include <QCryptographicHash>

QString Crypto::encrypt(const QString& plaintext, const QString& password) {
    if (plaintext.isEmpty() || password.isEmpty()) return QString();
    QByteArray saltBa = "usb_autodump_crypto_v1";
    QByteArray pwdBa = password.toUtf8() + saltBa;
    QByteArray keyFull = QCryptographicHash::hash(pwdBa, QCryptographicHash::Sha256);
    QByteArray key = keyFull.left(16);
    QByteArray plainBa = plaintext.toUtf8();
    QByteArray cipherBa;
    cipherBa.reserve(plainBa.size());
    for (int i = 0; i < plainBa.size(); ++i) {
        cipherBa.append(plainBa[i] ^ key[i % key.size()]);
    }
    return cipherBa.toBase64();
}

QString Crypto::decrypt(const QString& ciphertext, const QString& password) {
    if (ciphertext.isEmpty() || password.isEmpty()) return QString();
    QByteArray saltBa = "usb_autodump_crypto_v1";
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

bool Crypto::verifyPassword(const QString& password, const QString& storedToken) {
    return decrypt(storedToken, password) == "USB_AUTO_DUMP_VERIFY_TOKEN_v1";
}

#endif // _WIN32
