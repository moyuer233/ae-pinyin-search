/*******************************************************************/
/*                                                                 */
/* AE Pinyin Search - what After Effects calls the installed        */
/* effects                                                          */
/*                                                                 */
/* The search index carries the .aex file name for third-party      */
/* effects ("AutoFill2"), but the host registers it under a display */
/* name ("Auto Fill 2") and a match name, and addProperty() only    */
/* accepts those. So ask the host once for every installed effect    */
/* and translate at apply time.                                      */
/*                                                                 */
/*******************************************************************/

#ifndef AEPINYINSEARCH_EFFECTNAMES_H
#define AEPINYINSEARCH_EFFECTNAMES_H

#include "AE_GeneralPlug.h"
#include "DiagLog.h"

#include <algorithm>
#include <string>
#include <vector>

class EffectNames
{
public:
    // Fold a name down to letters and digits: "AutoFill2" and "Auto Fill 2"
    // both become "autofill2", which is how the index name finds the host name.
    static std::string KeyOf(const char* name)
    {
        std::string out;
        for (const unsigned char* p = reinterpret_cast<const unsigned char*>(name); p && *p; ++p)
        {
            const unsigned char c = *p;
            if (c >= 'A' && c <= 'Z')
            {
                out += static_cast<char>(c - 'A' + 'a');
            }
            else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            {
                out += static_cast<char>(c);
            }
        }
        return out;
    }

    void Build(SPBasicSuite* spbP)
    {
        i_entries.clear();
        i_byKey.clear();
        i_byExact.clear();

        AEGP_EffectSuite5* suite = NULL;
        const void* suiteP = NULL;
        if (!spbP ||
            spbP->AcquireSuite(kAEGPEffectSuite, kAEGPEffectSuiteVersion5, &suiteP) != A_Err_NONE || !suiteP)
        {
            AEPinyinLog("effect names: AEGP Effect Suite 5 unavailable");
            return;
        }
        suite = reinterpret_cast<AEGP_EffectSuite5*>(const_cast<void*>(suiteP));

        A_long count = 0;
        if (suite->AEGP_GetNumInstalledEffects(&count) == A_Err_NONE)
        {
            AEGP_InstalledEffectKey key = AEGP_InstalledEffectKey_NONE;
            for (A_long i = 0; i < count; ++i)
            {
                AEGP_InstalledEffectKey next = AEGP_InstalledEffectKey_NONE;
                if (suite->AEGP_GetNextInstalledEffect(key, &next) != A_Err_NONE ||
                    next == AEGP_InstalledEffectKey_NONE)
                {
                    break;
                }
                key = next;

                A_char display[AEGP_MAX_EFFECT_NAME_SIZE] = {};
                A_char match[AEGP_MAX_EFFECT_MATCH_NAME_SIZE] = {};
                suite->AEGP_GetEffectName(key, display);
                suite->AEGP_GetEffectMatchName(key, match);
                if (display[0] == '\0' && match[0] == '\0')
                {
                    continue;
                }

                // Skip the entries whose "name" is actually the file name AE
                // hands out for hidden/internal effects? No: keep everything,
                // the folded key decides whether it is useful.
                Entry e;
                e.display = display;
                e.matchName = match;
                i_entries.push_back(e);
            }
        }
        spbP->ReleaseSuite(kAEGPEffectSuite, kAEGPEffectSuiteVersion5);

        for (size_t i = 0; i < i_entries.size(); ++i)
        {
            const std::string displayKey = KeyOf(i_entries[i].display.c_str());
            const std::string matchKey = KeyOf(i_entries[i].matchName.c_str());
            if (!displayKey.empty())
            {
                i_byKey.push_back(std::make_pair(displayKey, i));
            }
            if (!matchKey.empty() && matchKey != displayKey)
            {
                i_byKey.push_back(std::make_pair(matchKey, i));
            }
            if (!i_entries[i].display.empty())
            {
                i_byExact.push_back(std::make_pair(i_entries[i].display, i));
            }
        }
        std::sort(i_byKey.begin(), i_byKey.end());
        std::sort(i_byExact.begin(), i_byExact.end());
        AEPinyinLog("effect names: %d installed effect(s) from the host", static_cast<int>(i_entries.size()));
    }

    size_t Size() const { return i_entries.size(); }

    // Only a name with ASCII in it can be looked up by folding. The chinese
    // names of the built-in effects fold to nothing, and those names are exactly
    // what the host's own UI shows, so they must be left alone.
    static bool CanLookUp(const char* indexName) { return !KeyOf(indexName).empty(); }

    // The name to show for an index row: what the host calls the effect, or an
    // empty string when there is nothing better than the index name.
    std::string DisplayNameFor(const char* indexName, const char* indexEnglish) const
    {
        if (!CanLookUp(indexName))
        {
            return std::string();
        }
        const Entry* e = Find(indexName, indexEnglish);
        return e ? e->display : std::string();
    }

    // The names to hand to addProperty(), most reliable first: the match name is
    // locale independent, then the display name, then whatever the index had.
    void ApplyNamesFor(const char* indexName, const char* indexEnglish, std::vector<std::string>& out) const
    {
        out.clear();
        if (CanLookUp(indexName))
        {
            const Entry* e = Find(indexName, indexEnglish);
            if (e)
            {
                Push(out, e->matchName);
                Push(out, e->display);
            }
        }
        Push(out, indexName);
        Push(out, indexEnglish);
        // The index splits camel-case keys into words ("PProRamp" -> "P Pro
        // Ramp"), and the host may well know the unsplit spelling, so try that
        // too before giving up.
        Push(out, WithoutSpaces(indexName));
        Push(out, WithoutSpaces(indexEnglish));
    }

private:
    struct Entry
    {
        std::string display;
        std::string matchName;
    };

    static void Push(std::vector<std::string>& out, const std::string& value)
    {
        if (value.empty())
        {
            return;
        }
        for (const std::string& seen : out)
        {
            if (seen == value)
            {
                return;
            }
        }
        out.push_back(value);
    }

    static std::string WithoutSpaces(const char* name)
    {
        std::string out;
        for (const char* p = name; p && *p; ++p)
        {
            if (*p != ' ' && *p != '\t')
            {
                out += *p;
            }
        }
        return (out == (name ? name : "")) ? std::string() : out;
    }

    const Entry* Find(const char* indexName, const char* indexEnglish) const
    {
        // A chinese index name folds to nothing, but it can still be exactly the
        // host's display name - that is how the built-ins resolve to their match
        // names, which addProperty() takes regardless of the UI language.
        const Entry* exact = FindExact(indexName);
        if (exact)
        {
            return exact;
        }
        const Entry* byName = FindByKey(KeyOf(indexName));
        if (byName)
        {
            return byName;
        }
        const Entry* exactEnglish = FindExact(indexEnglish);
        if (exactEnglish)
        {
            return exactEnglish;
        }
        const Entry* byEnglish = FindByKey(KeyOf(indexEnglish));
        if (byEnglish)
        {
            return byEnglish;
        }
        // Folding fixes "AutoFill2" vs "Auto Fill 2", but vendors also add
        // prefixes: the file is "Ambient Light.aex" while the host calls it
        // "BCC+Ambient Light". Accept a host name that ENDS with the index name
        // plus such a prefix, and pick the least padded one.
        const Entry* byName2 = FindPrefixed(KeyOf(indexName));
        if (byName2)
        {
            return byName2;
        }
        return FindPrefixed(KeyOf(indexEnglish));
    }

    // The host name "<vendor prefix><key>". A plain substring test was too eager:
    // it resolved the built-in "Gradient Ramp" onto another vendor's
    // "uni.Gradient Ramp". Short keys are ignored so "Glow" cannot claim
    // "DeepGlow".
    const Entry* FindPrefixed(const std::string& key) const
    {
        if (key.size() < 5)
        {
            return NULL;
        }
        const Entry* best = NULL;
        size_t bestSlack = 0;
        for (size_t i = 0; i < i_byKey.size(); ++i)
        {
            const std::string& hostKey = i_byKey[i].first;
            if (hostKey.size() <= key.size() || hostKey.compare(hostKey.size() - key.size(), key.size(), key) != 0)
            {
                continue;
            }
            const size_t slack = hostKey.size() - key.size();
            if (!best || slack < bestSlack)
            {
                best = &i_entries[i_byKey[i].second];
                bestSlack = slack;
            }
        }
        return best;
    }

    const Entry* FindExact(const char* name) const
    {
        if (!name || !*name)
        {
            return NULL;
        }
        const std::pair<std::string, size_t> probe(name, 0);
        std::vector<std::pair<std::string, size_t>>::const_iterator at =
            std::lower_bound(i_byExact.begin(), i_byExact.end(), probe);
        if (at != i_byExact.end() && at->first == name)
        {
            return &i_entries[at->second];
        }
        return NULL;
    }

    const Entry* FindByKey(const std::string& key) const
    {        if (key.empty())
        {
            return NULL;
        }
        const std::pair<std::string, size_t> probe(key, 0);
        std::vector<std::pair<std::string, size_t>>::const_iterator at =
            std::lower_bound(i_byKey.begin(), i_byKey.end(), probe);
        if (at != i_byKey.end() && at->first == key)
        {
            return &i_entries[at->second];
        }
        return NULL;
    }

    std::vector<Entry> i_entries;
    std::vector<std::pair<std::string, size_t>> i_byKey;   // sorted, folded name -> entry
    std::vector<std::pair<std::string, size_t>> i_byExact; // sorted, exact display name -> entry
};

#endif // AEPINYINSEARCH_EFFECTNAMES_H
