// Pinyin matching for the search bar.
//
// Mirrors the rules already proven in the JS prototype (tools/build_index.py +
// extension/search.js): match against the display name, the full pinyin and the
// initials, with a fuzzy-sound fold (zh->z, ch->c, sh->s, ang->an, eng->en,
// ing->in) so "gsmh" / "gaosimohu" / "gsm" all find 高斯模糊.

#pragma once

#include "pinyin_data.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

// windows.h (pulled in by the AE headers) defines min/max as macros, which
// breaks std::min / std::max usages below.
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace pinyin {

// ---------------------------------------------------------------------------
// Usage history: the entries the user actually applies come back to the top.
//
// Keyed by NAME, not by row index, so regenerating the index (a new plug-in was
// installed) does not shuffle the history.
//
// The bonus stays inside the tiers Score() produces: it reorders equally
// relevant matches (which is what "the one I always use" needs) without letting
// a weak substring match jump above an exact one.
// ---------------------------------------------------------------------------

inline int UsageBoost(int count)
{
    const int capped = (count > 15) ? 15 : count;
    return capped * 6; // max 90, below the 100 gap between tiers
}

class UsageTable
{
public:
    void Clear() { i_entries.clear(); }

    void Add(const char* name, int count)
    {
        if (!name || !*name || count <= 0)
        {
            return;
        }
        const std::string key(name);
        const size_t at = LowerBound(key);
        if (at < i_entries.size() && i_entries[at].name == key)
        {
            i_entries[at].count += count;
        }
        else
        {
            Entry e;
            e.name = key;
            e.count = count;
            i_entries.insert(i_entries.begin() + static_cast<ptrdiff_t>(at), e);
        }
    }

    void Bump(const char* name) { Add(name, 1); }

    int Count(const char* name) const
    {
        if (!name || !*name)
        {
            return 0;
        }
        const std::string key(name);
        const size_t at = LowerBound(key);
        if (at < i_entries.size() && i_entries[at].name == key)
        {
            return i_entries[at].count;
        }
        return 0;
    }

    int Boost(const char* name) const { return UsageBoost(Count(name)); }

    size_t Size() const { return i_entries.size(); }

    const std::string& NameAt(size_t i) const { return i_entries[i].name; }
    int CountAt(size_t i) const { return i_entries[i].count; }

private:
    struct Entry
    {
        std::string name;
        int count;
    };

    size_t LowerBound(const std::string& key) const
    {
        size_t lo = 0;
        size_t hi = i_entries.size();
        while (lo < hi)
        {
            const size_t mid = lo + (hi - lo) / 2;
            if (i_entries[mid].name < key)
            {
                lo = mid + 1;
            }
            else
            {
                hi = mid;
            }
        }
        return lo;
    }

    std::vector<Entry> i_entries; // sorted by name, so lookups are a binary search
};

// Lowercase and drop separators, but KEEP non-ASCII bytes so that typing actual
// chinese characters still matches the display name.
inline std::string Normalize(const char* s)
{
    std::string out;
    if (!s)
    {
        return out;
    }
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p; ++p)
    {
        unsigned char c = *p;
        if (c >= 'A' && c <= 'Z')
        {
            out += static_cast<char>(c - 'A' + 'a');
        }
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
        {
            out += static_cast<char>(c);
        }
        else if (c >= 0x80)
        {
            out += static_cast<char>(c);
        }
    }
    return out;
}

inline std::string Fold(const std::string& in)
{
    static const char* kFrom[] = {"zh", "ch", "sh", "ang", "eng", "ing"};
    static const char* kTo[] = {"z", "c", "s", "an", "en", "in"};

    std::string s = in;
    for (size_t i = 0; i < sizeof(kFrom) / sizeof(kFrom[0]); ++i)
    {
        const size_t fromLen = std::strlen(kFrom[i]);
        size_t pos = 0;
        while ((pos = s.find(kFrom[i], pos)) != std::string::npos)
        {
            s.replace(pos, fromLen, kTo[i]);
            pos += std::strlen(kTo[i]);
        }
    }
    return s;
}

// Returns a score (higher is better) or -1 when the entry does not match.
inline int Score(const PinyinEntry& e, const std::string& q, const UsageTable* usage = NULL)
{
    if (q.empty())
    {
        return -1;
    }

    int best = -1;

    // Direct hit on the display name (works for chinese input too).
    const std::string name = Normalize(e.name);
    if (!name.empty())
    {
        const size_t p = name.find(q);
        if (p != std::string::npos)
        {
            best = (name == q) ? 1000 : ((p == 0) ? 800 : 600);
        }
    }

    // Pinyin / initials / english, only worth checking for 2+ characters.
    if (q.size() >= 2)
    {
        const std::string keys[6] = {
            e.full ? e.full : "",
            e.initials ? e.initials : "",
            Fold(e.full ? e.full : ""),
            Fold(e.initials ? e.initials : ""),
            Normalize(e.english),
            Fold(Normalize(e.english)),
        };
        for (const std::string& k : keys)
        {
            if (k.empty())
            {
                continue;
            }
            if (k == q)
            {
                best = std::max(best, 900);
            }
            else if (k.compare(0, q.size(), q) == 0)
            {
                best = std::max(best, 700);
            }
            else if (k.find(q) != std::string::npos)
            {
                best = std::max(best, 400);
            }
        }
    }

    if (best < 0)
    {
        return -1;
    }

    // Tighter (shorter) names win ties.
    const size_t len = name.empty() ? 1 : name.size();
    int score = best - static_cast<int>(std::min<size_t>(len, 40));
    if (usage)
    {
        score += usage->Boost(e.name);
    }
    return score;
}

struct Hit
{
    int index;
    int score;
};

// ---------------------------------------------------------------------------
// Classes ("@group"), like JEI's mod filter.
//
// A class is the vendor a plug-in came from - the index already carries it
// (Boris FX 481 effects, Sapphire 287, Red Giant Universe 105, ...) - and it
// also matches a NAME prefix, so "@BCC" still reaches the Continuum effects
// whose names all start with BCC even though BCC is not a vendor.
//
// Tokenizing names into classes was tried first and does not survive this data:
// "BCCAlphaProcess" has no separator to cut at, "BCC3WayColorGrade" has one, and
// "3D Camera Tracker" starts with a digit.
// ---------------------------------------------------------------------------

inline std::string ToLowerAscii(const std::string& in)
{
    std::string out = in;
    for (char& c : out)
    {
        if (c >= 'A' && c <= 'Z')
        {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return out;
}

// "Boris FX" -> "borisfx": vendors compare without case or separators.
inline std::string VendorKey(const char* vendor)
{
    std::string out;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(vendor); p && *p; ++p)
    {
        const unsigned char c = *p;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
        {
            out += static_cast<char>((c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c);
        }
    }
    return out;
}

inline bool StartsWith(const std::string& s, const std::string& prefix)
{
    return !prefix.empty() && s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

// Short aliases for the vendors, so "@BFX" reaches the same entries as
// "@Boris FX". The table is GENERATED from the index (tools/gen_pinyin_data.py)
// with an auto rule plus manual overrides, so a newly installed plug-in vendor
// shows up here without touching this file.
inline const PinyinVendorAlias* VendorAliasTable(size_t& count)
{
    count = static_cast<size_t>(kPinyinVendorAliasCount);
    return kPinyinVendorAliases;
}

inline std::string ExpandAlias(const std::string& token)
{
    size_t count = 0;
    const PinyinVendorAlias* aliases = VendorAliasTable(count);
    const std::string key = VendorKey(token.c_str());
    for (size_t i = 0; i < count; ++i)
    {
        if (key == aliases[i].key)
        {
            return std::string(aliases[i].vendor);
        }
    }
    return token;
}

// The shortest thing the user can type for this vendor ("Boris FX" -> "bfx").
// Falls back through: exact alias row, alias row that covers a whole family
// ("Text" covers "Text\Animate In"), then the folded vendor name.
inline std::string AliasForVendor(const std::string& vendor)
{
    size_t count = 0;
    const PinyinVendorAlias* aliases = VendorAliasTable(count);
    size_t bestLength = 0;
    std::string bestKey;
    for (size_t i = 0; i < count; ++i)
    {
        const std::string name(aliases[i].vendor);
        if (vendor == name)
        {
            return std::string(aliases[i].key);
        }
        const bool covers =
            vendor.size() > name.size() && vendor.compare(0, name.size(), name) == 0 &&
            std::strchr(" \\/-_", vendor[name.size()]) != NULL;
        if (covers && name.size() > bestLength)
        {
            bestLength = name.size(); // the most specific covering entry wins
            bestKey = aliases[i].key;
        }
    }
    if (!bestKey.empty())
    {
        return bestKey;
    }
    return VendorKey(vendor.c_str());
}

// Case-insensitive prefix test on a name that stays UTF-8: a chinese query only
// matches when its bytes match too, which is the behaviour we want.
inline bool NameStartsWith(const char* name, const std::string& wantLower)
{
    if (!name || wantLower.empty())
    {
        return false;
    }
    for (size_t i = 0; i < wantLower.size(); ++i)
    {
        unsigned char c = static_cast<unsigned char>(name[i]);
        if (!c)
        {
            return false;
        }
        if (c >= 'A' && c <= 'Z')
        {
            c = static_cast<unsigned char>(c - 'A' + 'a');
        }
        if (c != static_cast<unsigned char>(wantLower[i]))
        {
            return false;
        }
    }
    return true;
}

inline bool InClass(const PinyinEntry& e, const std::string& wantLower, const std::string& wantVendor)
{
    const std::string key = VendorKey(e.vendor);
    if (StartsWith(key, wantVendor))
    {
        return true;
    }
    return NameStartsWith(e.name, wantLower) || NameStartsWith(e.english, wantLower);
}

// A row in the "@" / "#" browser: what to show, what to type for it, how many
// entries it holds.
struct GroupInfo
{
    std::string name;
    std::string key;
    int count;
};

// Every class with at least `minCount` entries, biggest first.
inline void ListGroups(std::vector<GroupInfo>& out, int minCount)
{
    out.clear();
    for (int i = 0; i < kPinyinEntryCount; ++i)
    {
        const char* v = kPinyinEntries[i].vendor;
        if (!v || !*v)
        {
            continue;
        }
        bool found = false;
        for (GroupInfo& gi : out)
        {
            if (gi.name == v)
            {
                gi.count++;
                found = true;
                break;
            }
        }
        if (!found)
        {
            GroupInfo gi;
            gi.name = v;
            gi.key = AliasForVendor(v);
            gi.count = 1;
            out.push_back(gi);
        }
    }
    if (minCount > 1)
    {
        out.erase(
            std::remove_if(
                out.begin(), out.end(), [minCount](const GroupInfo& g) { return g.count < minCount; }),
            out.end());
    }
    std::sort(out.begin(), out.end(), [](const GroupInfo& a, const GroupInfo& b) {
        if (a.count != b.count)
        {
            return a.count > b.count;
        }
        return a.name < b.name;
    });
}

// The kinds the "#" filter understands, for the list you get from typing "#".
inline void ListKinds(std::vector<GroupInfo>& out)
{
    out.clear();
    GroupInfo effects;
    effects.name = "效果";
    effects.key = "effect";
    effects.count = 0;
    GroupInfo presets;
    presets.name = "预设";
    presets.key = "preset";
    presets.count = 0;

    for (int i = 0; i < kPinyinEntryCount; ++i)
    {
        if (kPinyinEntries[i].is_preset)
        {
            presets.count++;
        }
        else
        {
            effects.count++;
        }
    }
    if (effects.count > 0)
    {
        out.push_back(effects);
    }
    if (presets.count > 0)
    {
        out.push_back(presets);
    }
}

// 0 = no kind filter, 1 = effects only, 2 = presets only, -1 = unknown kind.
inline int KindOf(const std::string& token)
{
    if (token.empty())
    {
        return 0;
    }
    // VendorKey folds case and separators but drops non-ascii, so the chinese
    // spellings have to be compared against the raw token.
    const std::string k = VendorKey(token.c_str());
    if (k == "effect" || k == "effects" || token == "效果" || token == "特效")
    {
        return 1;
    }
    if (k == "preset" || k == "presets" || token == "预设")
    {
        return 2;
    }
    return -1;
}

// A parsed query. The filters combine, so "@boris #预设 blur" means: Boris FX
// entries, presets only, matching "blur".
struct Query
{
    bool listClasses;     // the query is exactly "@"
    bool listKinds;       // the query is exactly "#"
    std::string group;    // class filter ("" = none)
    std::string kindName; // kind filter ("" = none)
    std::string text;     // free text ("" = none)

    Query() : listClasses(false), listKinds(false) {}
};

inline bool IsKnownGroup(const std::string& token)
{
    const std::string expanded = ExpandAlias(token);
    const std::string want = VendorKey(expanded.c_str());
    if (want.empty())
    {
        return false;
    }
    for (int i = 0; i < kPinyinEntryCount; ++i)
    {
        if (StartsWith(VendorKey(kPinyinEntries[i].vendor), want))
        {
            return true;
        }
    }
    return false;
}

inline Query ParseQuery(const std::string& raw)
{
    Query q;
    const size_t begin = raw.find_first_not_of(" \t");
    if (begin == std::string::npos)
    {
        return q;
    }
    std::string s = raw.substr(begin);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
    {
        s.pop_back();
    }
    if (s.empty())
    {
        return q;
    }
    if (s == "@")
    {
        q.listClasses = true;
        return q;
    }
    if (s == "#")
    {
        q.listKinds = true;
        return q;
    }

    // A vendor name may contain spaces ("Red Giant Universe"): if the whole
    // remainder after "@" names a class, take it whole instead of splitting it.
    if (s[0] == '@' && IsKnownGroup(s.substr(1)))
    {
        q.group = s.substr(1);
        return q;
    }

    bool sawAt = false;
    size_t pos = 0;
    while (pos < s.size())
    {
        const size_t sp = s.find_first_of(" \t", pos);
        const std::string tok = (sp == std::string::npos) ? s.substr(pos) : s.substr(pos, sp - pos);
        pos = (sp == std::string::npos) ? s.size() : sp + 1;
        if (tok.empty())
        {
            continue;
        }
        if (tok[0] == '@')
        {
            if (!sawAt && tok.size() > 1)
            {
                q.group = tok.substr(1);
                sawAt = true;
            }
            continue;
        }
        if (tok[0] == '#')
        {
            if (q.kindName.empty() && tok.size() > 1)
            {
                q.kindName = tok.substr(1);
            }
            continue;
        }
        if (!q.text.empty())
        {
            q.text += ' ';
        }
        q.text += tok;
    }

    // Typing a vendor name without "@" means the same as "@vendor".
    if (q.group.empty() && q.kindName.empty() && !q.text.empty())
    {
        const size_t sp = q.text.find(' ');
        const std::string head = (sp == std::string::npos) ? q.text : q.text.substr(0, sp);
        if (IsKnownGroup(head))
        {
            q.group = head;
            q.text = (sp == std::string::npos) ? std::string() : q.text.substr(sp + 1);
        }
    }
    return q;
}

inline void SortHits(std::vector<Hit>& out, size_t limit)
{
    std::stable_sort(out.begin(), out.end(), [](const Hit& a, const Hit& b) {
        if (a.score != b.score)
        {
            return a.score > b.score; // most used / most relevant first
        }
        return std::strcmp(kPinyinEntries[a.index].name, kPinyinEntries[b.index].name) < 0;
    });
    if (out.size() > limit)
    {
        out.resize(limit);
    }
}

// Collect up to `limit` best matches for `query` into `out`.
inline void Search(
    const std::string& query,
    std::vector<Hit>& out,
    size_t limit,
    const UsageTable* usage = NULL)
{
    out.clear();
    const std::string q = Normalize(query.c_str());
    if (q.empty())
    {
        return;
    }
    for (int i = 0; i < kPinyinEntryCount; ++i)
    {
        const int s = Score(kPinyinEntries[i], q, usage);
        if (s >= 0)
        {
            Hit h;
            h.index = i;
            h.score = s;
            out.push_back(h);
        }
    }
    SortHits(out, limit);
}

// The whole pipeline the UI uses. Filters combine: "@" picks a class, "#" picks
// a kind (effect / preset) and everything else is free text, in any order.
inline void SearchQuery(
    const std::string& raw,
    std::vector<Hit>& out,
    size_t limit,
    const UsageTable* usage = NULL)
{
    out.clear();
    const Query q = ParseQuery(raw);
    if (q.listClasses || q.listKinds)
    {
        return; // the UI lists the classes / kinds itself
    }

    const int kindWant = KindOf(q.kindName);
    if (kindWant < 0)
    {
        return; // "#something" that is not a kind we know
    }

    const std::string text = Normalize(q.text.c_str());
    std::string wantLower;
    std::string wantVendor;
    if (!q.group.empty())
    {
        const std::string expanded = ExpandAlias(q.group); // "@BFX" -> "Boris FX"
        wantLower = ToLowerAscii(expanded);
        wantVendor = VendorKey(expanded.c_str());
    }

    for (int i = 0; i < kPinyinEntryCount; ++i)
    {
        const PinyinEntry& e = kPinyinEntries[i];
        if (kindWant != 0 && ((kindWant == 2) != (e.is_preset != 0)))
        {
            continue;
        }
        if (!q.group.empty() && !InClass(e, wantLower, wantVendor))
        {
            continue;
        }
        int score = 0;
        if (!text.empty())
        {
            score = Score(e, text, usage);
            if (score < 0)
            {
                continue;
            }
        }
        else if (usage)
        {
            score = usage->Boost(e.name); // browsing a class/kind: used ones first
        }
        Hit h;
        h.index = i;
        h.score = score;
        out.push_back(h);
    }

    SortHits(out, limit);
}

} // namespace pinyin
