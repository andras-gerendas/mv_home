/**
 * @file   move_homedir.cpp
 * @author András Attila Gerendás
 * @brief  Updates home directory in the registry
 * @date   2018.03.16.
 *
 * This utility renames all entries for a specific user to another user in the
 * registry. Particularly useful when moving the home directory.
 *
 * Requires administrative privileges in order to run correctly.
 */

#include <iostream>
#include "Windows.h"
#include "Winreg.h"

#include <io.h>
#include <fcntl.h>
#include <string>
#include <tchar.h>
#include <stdio.h>

#define DEBUG false

#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 16383
#define NAME_BUFFER 1024

#define FROM_NAME L"Users\\from"
#define TO_NAME L"Users\\to"

/**
 * @class   RegKey
 *
 * @brief   Representation of a key in the registry.
 *
 * @date    2018.03.16.
 */

class RegKey {
    /** @brief  The depth of the current key. Used to detect stack overflow. */
    int depth;
    /** @brief  The error code of the registry API */
    DWORD errorCode;
    /** @brief  Evaulates whether the key was successfully opened */
    bool isValidb;
    /** @brief  Handle of the key */
    HKEY key;
    /** @brief  The name of the key */
    TCHAR name[NAME_BUFFER];
    /** @brief  Number of subkeys of the key */
    DWORD subkeyCount;
    /** @brief  Size of the longest subkey */
    DWORD longestSubkeySize;
    /** @brief  Size of the longest subclass */
    DWORD longestSubClassSize;
    /** @brief  Number of values */
    DWORD valueCount;
    /** @brief  Length of the name of the longest value */
    DWORD longestValueName;
    /** @brief  Information describing the longest value */
    DWORD longestValueData;
    /** @brief  Size of the security descriptor */
    DWORD securityDescriptorSize;
    /** @brief  The last write time */
    FILETIME lastWriteTime;
public:

    /**
     * @fn  RegKey(HKEY parent, TCHAR* name, int depth)
     *
     * @brief   Creates a new registry key under the parent key
     *
     * @date    2018.03.16.
     *
     * @param           parent  Handle of the parent.
     * @param [in,out]  name    If non-null, the name.
     * @param           depth   The depth of the key from the root of the hive
     */

    RegKey(HKEY parent, TCHAR* name, int depth)
    {
        this->depth = depth;
        _tcscpy_s(this->name, NAME_BUFFER, name);
        errorCode = RegOpenKeyEx(parent, name, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY,
                                 &key);
        isValidb = (errorCode == ERROR_SUCCESS);
        if (isValidb) {
            getInfo();
        }
        else {
            if (errorCode == ERROR_ACCESS_DENIED && DEBUG) {
                std::wcout << "Access denied. Are you an administrator?" << "\n";
            }
            else if (errorCode != ERROR_FILE_NOT_FOUND && DEBUG) {
                std::wcout << "Error during key open: " << errorCode << "\n";
            }
        }
    }

    /**
     * @fn  ~RegKey()
     *
     * @brief   Closes the key if it was opened successfully in the first place
     *
     * @date    2018.03.16.
     */

    ~RegKey()
    {
        if (isValidb) {
            RegCloseKey(key);
        }
    }

    /**
     * @fn  void getInfo()
     *
     * @brief   Retrieves metadata of the key (used for subkey count mainly)
     *
     * @date    2018.03.16.
     */

    void getInfo()
    {
        RegQueryInfoKey(
            key,                     // key handle
            NULL,                    // buffer for class name
            NULL,                    // size of class string
            NULL,                    // reserved
            &subkeyCount,            // number of subkeys
            &longestSubkeySize,      // longest subkey size
            &longestSubClassSize,    // longest class string
            &valueCount,             // number of values for this key
            &longestValueName,       // longest value name
            &longestValueData,       // longest value data
            &securityDescriptorSize, // security descriptor
            &lastWriteTime);         // last write time
    }

    TCHAR* getName()
    {
        return name;
    }

    HKEY getKey()
    {
        return key;
    }

    DWORD getSubkeyCount()
    {
        return subkeyCount;
    }

    DWORD getValueCount()
    {
        return valueCount;
    }

    DWORD getLongestValueData()
    {
        return longestValueData;
    }

    /**
     * @fn  bool isValid()
     *
     * @brief   Query whether the key was successfully opened
     *
     * @date    2018.03.16.
     *
     * @return  True if valid, false if not.
     */

    bool isValid()
    {
        return isValidb;
    }

    /**
     * @fn  int getDepth()
     *
     * @brief   Retrieves the current distance from the root of the hive
     *
     * @date    2018.03.16.
     *
     * @return  The depth from the root of the hive
     */

    int getDepth()
    {
        return depth;
    }

    DWORD getErrorCode()
    {
        return errorCode;
    }
};

/**
 * @fn  std::wstring Replace(const std::wstring& haystack, const std::wstring& needle,
 *                           const std::wstring& replacement)
 *
 * @brief   Replaces all occurences of the needle in the haystack.
 *
 * Does not modify the original string.
 *
 * @date    2018.03.16.
 *
 * @param   haystack    The whole original string.
 * @param   needle      The string to be replaced.
 * @param   replacement The replacement.
 *
 * @return  A std::wstring containing the replaced string.
 */

std::wstring Replace(const std::wstring& haystack, const std::wstring& needle,
                     const std::wstring& replacement)
{
    std::wstring value = haystack;
    size_t pos = 0;

    while (true) {
        pos = value.find(needle, pos);
        if (pos == std::wstring::npos)
            break;

        value.replace(pos, needle.length(), replacement);
        pos += replacement.length();
    }

    return value;
}

/**
 * @fn  bool iter(RegKey *keyHolder, int& count)
 *
 * @brief   Iterates over the subkeys and prints the values of the key. Recursive function.
 *
 * If it finds a matching value it replaces the home directory
 * information in the value.
 * Note: The extensive use of heap memory is to avoid stack overflow.
 *
 * @date    2018.03.16.
 *
 * @param [in,out]  keyHolder   If non-null, the key holder.
 * @param [in,out]  count       Number of values which match.
 *
 * @return  True if it succeeds, false if it fails.
 */

bool iter(RegKey *keyHolder, int& count)
{
    DWORD errValue;
    std::wcout << "Iterating through (" << keyHolder->getDepth() << ") " <<
               keyHolder->getName() << ":\n";
    for (DWORD i = 0; i < keyHolder->getSubkeyCount(); i++) {
        DWORD maxKeyName = MAX_KEY_LENGTH;
        TCHAR *keyName = new TCHAR[MAX_KEY_LENGTH];
        FILETIME lastWriteTime;
        if ((errValue = RegEnumKeyEx(keyHolder->getKey(), i, keyName, &maxKeyName, NULL,
                                     NULL, NULL, &lastWriteTime)) != ERROR_SUCCESS) {
            std::wcout << "Error: " << errValue << "\n";
            delete keyName;
            return false;
        }
        RegKey *subKey = new RegKey(keyHolder->getKey(), keyName,
                                    keyHolder->getDepth() + 1);
        /* This is to workaround registry virtualization */
        if (!subKey->isValid() && subKey->getErrorCode() != ERROR_FILE_NOT_FOUND) {
            if (DEBUG || subKey->getErrorCode() != ERROR_ACCESS_DENIED) {
                std::wcout << "Error: creation of subkey " << keyName << "\n";
            }
            /* Access denial should not be a problem here */
            if (subKey->getErrorCode() != ERROR_ACCESS_DENIED) {
                delete keyName;
                delete subKey;
                return false;
            }
        }
        std::wcout << i << ": " << keyName << "\n";
        /* Only iterate through the key if it's valid */
        if (subKey->isValid() && !iter(subKey, count)) {
            delete keyName;
            delete subKey;
            return false;
        }
        delete keyName;
        delete subKey;
    }
    std::wcout << "Values for class " << keyHolder->getName() << ":\n";
    for (DWORD i = 0; i < keyHolder->getValueCount(); i++) {
        DWORD maxKeyValue = MAX_VALUE_NAME;
        TCHAR* valueName = new TCHAR[MAX_VALUE_NAME];
        if ((errValue = RegEnumValue(keyHolder->getKey(), i, valueName, &maxKeyValue,
                                     NULL, NULL, NULL, NULL)) != ERROR_SUCCESS) {
            std::wcout << "Error: " << errValue << "\n";
            delete valueName;
            return false;
        }
        std::wcout << i << ": " << valueName << "\n";
        /* We do not know the size of the value to be retrieved, so assume the worst */
        TCHAR *data = new TCHAR[keyHolder->getLongestValueData() * 2 + 2];
        memset(data, 0, keyHolder->getLongestValueData() * 2 + 2);
        DWORD type, size = keyHolder->getLongestValueData() * 2 + 2;
        if ((errValue = RegGetValue(keyHolder->getKey(), NULL, valueName, RRF_RT_REG_SZ,
                                    &type, data, &size)) != ERROR_SUCCESS) {
            /* Unsupported type only means we encountered a non-string value */
            if (errValue != ERROR_UNSUPPORTED_TYPE) {
                if (errValue == ERROR_MORE_DATA) {
                    std::wcout << "Maximum length: " << keyHolder->getLongestValueData() << "\n";
                }
                std::wcout << "Error during value retrival: " << errValue << "\n";
                delete data;
                delete valueName;
                return false;
            }
        }
        if (errValue == ERROR_SUCCESS) {
            /*Only replace the string if it matches what we search for */
            if (wcsstr(data, FROM_NAME) != NULL) {
                count++;
                std::wcout << "key: " << keyHolder->getName() << " valueName: " << i << ": " <<
                           valueName << "\n";
                std::wstring replaced(data);
                replaced = Replace(replaced, FROM_NAME, TO_NAME);
                std::wcout << i << " value: " << data << "\n";
                std::wcout << i << " new value: " << replaced << "\n";
                DWORD setRes = RegSetValueEx(keyHolder->getKey(), valueName, 0, REG_SZ,
                                             (LPBYTE)replaced.c_str(),
                                             ((DWORD)replaced.length() + 1) * (DWORD)sizeof(WCHAR));
                if (setRes != ERROR_SUCCESS) {
                    delete valueName;
                    delete data;
                    return false;
                }
            }
        }
        delete valueName;
        delete data;
    }
    return true;
}

/**
 * @fn  int main()
 *
 * @brief   Main entry-point for this application
 *
 * Hives need to be iterated separately, but they use a common count.
 *
 * @date    2018.03.16.
 *
 * @return  Exit-code for the process - 0 for success, else an error code.
 */

int main()
{
    //https://stackoverflow.com/questions/2492077/output-unicode-strings-in-windows-console-app
    _setmode(_fileno(stdout), _O_U16TEXT);

    /* Used to hold the values which match the replacement criterium */
    int count = 0;

    RegKey classesRoot(HKEY_CLASSES_ROOT, L"", 0);
    if (!classesRoot.isValid()) {
        return -1;
    }
    iter(&classesRoot, count);

    RegKey currentUser(HKEY_CURRENT_USER, L"", 0);
    if (!currentUser.isValid()) {
        return -1;
    }
    iter(&currentUser, count);

    RegKey localMachine(HKEY_LOCAL_MACHINE, L"", 0);
    if (!localMachine.isValid()) {
        return -1;
    }
    iter(&localMachine, count);

    RegKey users(HKEY_USERS, L"", 0);
    if (!users.isValid()) {
        return -1;
    }
    iter(&users, count);

    RegKey currentConfig(HKEY_CURRENT_CONFIG, L"", 0);
    if (!currentConfig.isValid()) {
        return -1;
    }
    iter(&currentConfig, count);

    std::wcout << "Number of results: " << count << "\n";
    /* This is to ensure the program is also usable from the desktop */
    do {
        std::wcout << '\n' << "Press the return key to continue...";
    }
    while (std::wcin.get() != '\n');

    return 0;
}