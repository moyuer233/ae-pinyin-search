/*******************************************************************/
/*                                                                 */
/* AE Pinyin Search - matching an index name to a host effect name  */
/*                                                                 */
/* The search index carries the .aex file name for third-party      */
/* effects ("AutoFill2") and the dictionary name for the built-ins  */
/* ("梯度渐变"), while the host registers a display name and a       */
/* match name ("Auto Fill 2" / "Gradient Ramp" + "ADBE Ramp"). This */
/* class holds the host's list and answers "what should I hand to   */
/* addProperty() for this index row?".                              */
/*                                                                 */
/* No After Effects headers here on purpose: this is the part that  */
/* gets unit tested outside the host (tools\pinyin_match_test.cpp). */
/*                                                                 */
/*******************************************************************/

#ifndef AEPINYINSEARCH_EFFECTNAMEMATCH_H
#define AEPINYINSEARCH_EFFECTNAMEMATCH_H

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

class EffectNameMatch
{
public:
    // Fold a name down to letters and digits: "AutoFill2" and "Auto Fill 2"
    // both become "autofill2".
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

    // Only a name with ASCII in it can be looked up by folding. The chinese
    // names of the built-in effects fold to nothing, and those names are exactly
    // what the host's own UI shows, so they must be left alone.
    static bool CanLookUp(const char* indexName) { return !KeyOf(indexName).empty(); }

    void AddEntry(const char* display, const char* matchName)
    {
        if ((!display || !*display) && (!matchName || !*matchName))
        {
            return;
        }
        Entry e;
        e.display = display ? display : "";
        e.matchName = matchName ? matchName : "";
        i_entries.push_back(e);
    }

    // Call once after the last AddEntry().
    void Index()
    {
        i_byKey.clear();
        i_byExact.clear();
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
    }

    size_t Size() const { return i_entries.size(); }

    // The name to show for an index row, or an empty string when there is
    // nothing better than the index name itself.
    std::string DisplayNameFor(const char* indexName, const char* indexEnglish) const
    {
        if (!CanLookUp(indexName))
        {
            return std::string();
        }
        const Entry* e = Find(indexName, indexEnglish, true);
        return e ? e->display : std::string();
    }

    // What to hand to addProperty(), most reliable first.
    //
    // `thirdParty` says the index name came from a .aex file name, which is the
    // only case where a vendor prefix may be involved. A built-in's index name is
    // the host's own name, so nothing is guessed for it - otherwise the built-in
    // "Gradient Ramp" would be resolved onto another vendor's "uni.Gradient Ramp".
    void ApplyNamesFor(
        const char* indexName,
        const char* indexEnglish,
        bool thirdParty,
        std::vector<std::string>& out) const
    {
        out.clear();
        const Entry* e = Find(indexName, indexEnglish, thirdParty);
        if (e)
        {
            Push(out, e->matchName);
            if (CanLookUp(indexName))
            {
                Push(out, e->display); // never rename a chinese name to the host's
            }
        }
        Push(out, indexName);
        Push(out, indexEnglish);
        // The index splits camel-case dictionary keys into words ("PProRamp" ->
        // "P Pro Ramp") and the host may well know the unsplit spelling.
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
        if (!name)
        {
            return std::string();
        }
        std::string out;
        for (const char* p = name; *p; ++p)
        {
            if (*p != ' ' && *p != '\t')
            {
                out += *p;
            }
        }
        return (out == name) ? std::string() : out;
    }

    const Entry* Find(const char* indexName, const char* indexEnglish, bool thirdParty) const
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
        if (!thirdParty)
        {
            return NULL; // a built-in's name is the host's name; do not guess
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

    // The host name "<vendor prefix><key>". A plain substring test was too
    // eager: it resolved the built-in "Gradient Ramp" onto another vendor's
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
    {
        if (key.empty())
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

#endif // AEPINYINSEARCH_EFFECTNAMEMATCH_H
