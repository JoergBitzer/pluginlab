#include "pluginlab/hosting/PluginCatalog.h"

#include <algorithm>

namespace pluginlab::hosting
{
namespace
{
const juce::String kRootTag = "PluginCatalog";
const juce::String kFileTag = "File";
const juce::String kValidationTag = "Validation";
const juce::String kPathAttribute = "path";
const juce::String kStampAttribute = "stamp";
const juce::String kModifiedAttribute = "modified";
const juce::String kScannedAtAttribute = "scannedAt";
const juce::String kScanStatusAttribute = "scanStatus";
const juce::String kMessageAttribute = "message";
const juce::String kLevelAttribute = "level";
const juce::String kQuickAttribute = "quick";
const juce::String kPluginIdAttribute = "pluginId";
const juce::String kStatusAttribute = "status";
const juce::String kValidatedAtAttribute = "validatedAt";
const juce::String kPluginStampAttribute = "pluginStamp";
const juce::String kPluginModifiedAttribute = "pluginModified";
const juce::String kPluginvalStampAttribute = "pluginvalStamp";
const juce::String kDateFormat = "%Y-%m-%d %H:%M";
const juce::String kDateTimeFormat = "%Y-%m-%d %H:%M:%S";
const juce::String kFolderName = "pluginlab";
const juce::String kFileName = "plugin_catalog.xml";
const juce::String kLockName = "pluginlab_plugin_catalog";
constexpr int kLockTimeoutMs = 5000;

std::vector<juce::File> getPluginFiles(const juce::File& pluginFile)
{
    std::vector<juce::File> files;
    if (pluginFile.isDirectory())
    {
        for (const juce::File& file : pluginFile.findChildFiles(juce::File::findFiles, true))
        {
            files.push_back(file);
        }
        return files;
    }
    files.push_back(pluginFile);
    return files;
}

ScanStatus scanStatusFromText(const juce::String& text)
{
    for (const ScanStatus status : {ScanStatus::Ok, ScanStatus::NoPluginInFile, ScanStatus::Crashed, ScanStatus::TimedOut})
    {
        if (text == toString(status))
        {
            return status;
        }
    }
    return ScanStatus::ScannerFailed;
}

ValidationStatus validationStatusFromText(const juce::String& text)
{
    for (const ValidationStatus status : {ValidationStatus::Passed, ValidationStatus::TimedOut, ValidationStatus::NotAvailable})
    {
        if (text == toString(status))
        {
            return status;
        }
    }
    return ValidationStatus::Failed;
}

CatalogEntry* findEntry(std::vector<CatalogEntry>& entries, const juce::File& file)
{
    for (CatalogEntry& entry : entries)
    {
        if (entry.file == file)
        {
            return &entry;
        }
    }
    return nullptr;
}

// Reads and writes while holding the lock of all programs that use the catalog.
class CatalogLock
{
public:
    CatalogLock()
        : m_lock(kLockName)
    {
        m_held = m_lock.enter(kLockTimeoutMs); // after the timeout the program goes on without the lock: better than a hang
    }

    ~CatalogLock()
    {
        if (m_held)
        {
            m_lock.exit();
        }
    }

private:
    juce::InterProcessLock m_lock;
    bool m_held = false;
};
}

juce::String describePluginFile(const juce::File& pluginFile)
{
    juce::String description = pluginFile.getFullPathName();
    for (const juce::File& file : getPluginFiles(pluginFile))
    {
        description += "|" + file.getRelativePathFrom(pluginFile) + ":" + juce::String(file.getSize()) + ":"
                     + juce::String(file.getLastModificationTime().toMilliseconds());
    }
    return juce::String(description.hashCode64());
}

juce::Time getNewestModificationTime(const juce::File& pluginFile)
{
    juce::Time newest = pluginFile.getLastModificationTime();
    for (const juce::File& file : getPluginFiles(pluginFile))
    {
        newest = std::max(newest, file.getLastModificationTime());
    }
    return newest;
}

juce::String getModifiedText(const juce::File& pluginFile)
{
    if (! pluginFile.exists())
    {
        return {};
    }
    return getNewestModificationTime(pluginFile).formatted(kDateFormat);
}

juce::String getNowText()
{
    return juce::Time::getCurrentTime().formatted(kDateTimeFormat);
}

PluginCatalog::PluginCatalog(const juce::File& catalogFile)
    : m_file(catalogFile)
{
}

juce::File PluginCatalog::getDefaultFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile(kFolderName).getChildFile(kFileName);
}

std::vector<CatalogEntry> PluginCatalog::read() const
{
    if (m_file == juce::File())
    {
        return m_memory;
    }
    std::vector<CatalogEntry> entries;
    const std::unique_ptr<juce::XmlElement> root = juce::XmlDocument::parse(m_file);
    if (root == nullptr || ! root->hasTagName(kRootTag))
    {
        return entries;
    }
    for (const juce::XmlElement* fileElement : root->getChildWithTagNameIterator(kFileTag))
    {
        CatalogEntry entry;
        entry.file = juce::File(fileElement->getStringAttribute(kPathAttribute));
        entry.stamp = fileElement->getStringAttribute(kStampAttribute);
        entry.modified = fileElement->getStringAttribute(kModifiedAttribute);
        entry.scannedAt = fileElement->getStringAttribute(kScannedAtAttribute);
        entry.scanStatus = scanStatusFromText(fileElement->getStringAttribute(kScanStatusAttribute));
        entry.message = fileElement->getStringAttribute(kMessageAttribute);
        for (const juce::XmlElement* child : fileElement->getChildIterator())
        {
            if (child->hasTagName(kValidationTag))
            {
                CatalogValidation validation;
                validation.level = child->getIntAttribute(kLevelAttribute);
                validation.isQuickCheck = child->getBoolAttribute(kQuickAttribute);
                validation.pluginId = child->getStringAttribute(kPluginIdAttribute);
                validation.status = validationStatusFromText(child->getStringAttribute(kStatusAttribute));
                validation.message = child->getStringAttribute(kMessageAttribute);
                validation.validatedAt = child->getStringAttribute(kValidatedAtAttribute);
                validation.pluginStamp = child->getStringAttribute(kPluginStampAttribute);
                validation.pluginModified = child->getStringAttribute(kPluginModifiedAttribute);
                validation.pluginvalStamp = child->getStringAttribute(kPluginvalStampAttribute);
                entry.validations.push_back(validation);
                continue;
            }
            juce::PluginDescription description;
            if (description.loadFromXml(*child))
            {
                entry.descriptions.add(description);
            }
        }
        entries.push_back(entry);
    }
    return entries;
}

void PluginCatalog::write(const std::vector<CatalogEntry>& entries) const
{
    if (m_file == juce::File())
    {
        m_memory = entries;
        return;
    }
    juce::XmlElement root(kRootTag);
    for (const CatalogEntry& entry : entries)
    {
        juce::XmlElement* fileElement = root.createNewChildElement(kFileTag);
        fileElement->setAttribute(kPathAttribute, entry.file.getFullPathName());
        fileElement->setAttribute(kStampAttribute, entry.stamp);
        fileElement->setAttribute(kModifiedAttribute, entry.modified);
        fileElement->setAttribute(kScannedAtAttribute, entry.scannedAt);
        fileElement->setAttribute(kScanStatusAttribute, toString(entry.scanStatus));
        fileElement->setAttribute(kMessageAttribute, entry.message);
        for (const juce::PluginDescription& description : entry.descriptions)
        {
            fileElement->addChildElement(description.createXml().release());
        }
        for (const CatalogValidation& validation : entry.validations)
        {
            juce::XmlElement* element = fileElement->createNewChildElement(kValidationTag);
            element->setAttribute(kLevelAttribute, validation.level);
            element->setAttribute(kQuickAttribute, validation.isQuickCheck);
            element->setAttribute(kPluginIdAttribute, validation.pluginId);
            element->setAttribute(kStatusAttribute, toString(validation.status));
            element->setAttribute(kMessageAttribute, validation.message);
            element->setAttribute(kValidatedAtAttribute, validation.validatedAt);
            element->setAttribute(kPluginStampAttribute, validation.pluginStamp);
            element->setAttribute(kPluginModifiedAttribute, validation.pluginModified);
            element->setAttribute(kPluginvalStampAttribute, validation.pluginvalStamp);
        }
    }
    m_file.getParentDirectory().createDirectory();
    const juce::TemporaryFile temporary(m_file); // written completely, then put in place: a reader never sees half a file
    if (root.writeTo(temporary.getFile()))
    {
        temporary.overwriteTargetFileWithTemporary();
    }
}

std::vector<CatalogEntry> PluginCatalog::load() const
{
    const CatalogLock lock;
    return read();
}

void PluginCatalog::storeScanResults(const std::vector<PluginScanResult>& results, bool removeMissing)
{
    const CatalogLock lock;
    std::vector<CatalogEntry> entries = read();
    for (const PluginScanResult& result : results)
    {
        CatalogEntry* entry = findEntry(entries, result.file);
        if (entry == nullptr)
        {
            entries.emplace_back();
            entry = &entries.back();
            entry->file = result.file;
        }
        entry->stamp = describePluginFile(result.file);
        entry->modified = getModifiedText(result.file);
        entry->scannedAt = getNowText();
        entry->scanStatus = result.status;
        entry->message = result.message;
        entry->descriptions = result.descriptions;
    }
    if (removeMissing)
    {
        entries.erase(std::remove_if(entries.begin(), entries.end(), [](const CatalogEntry& entry) { return ! entry.file.exists(); }),
                      entries.end());
    }
    write(entries);
}

void PluginCatalog::storeValidation(const juce::File& pluginFile, const CatalogValidation& validation)
{
    const CatalogLock lock;
    std::vector<CatalogEntry> entries = read();
    CatalogEntry* entry = findEntry(entries, pluginFile);
    if (entry == nullptr)
    {
        entries.emplace_back();
        entry = &entries.back();
        entry->file = pluginFile;
    }
    std::vector<CatalogValidation>& validations = entry->validations;
    validations.erase(std::remove_if(validations.begin(), validations.end(),
                                     [&validation](const CatalogValidation& known)
                                     {
                                         return known.level == validation.level && known.isQuickCheck == validation.isQuickCheck
                                             && known.pluginId == validation.pluginId;
                                     }),
                      validations.end());
    validations.push_back(validation);
    write(entries);
}
}
