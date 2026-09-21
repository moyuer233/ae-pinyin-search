// Self-test for the matching layer (pinyin_match.h + the generated table).
//
// Runs outside After Effects: the matcher is plain C++, so the "@class" feature
// and the pinyin search can be checked without launching the host.
// Build + run: tools\build_match_test.cmd   (exit code = number of failures)

#include "pinyin_match.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int g_fail = 0;

static void Check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "OK  " : "FAIL", what);
    if (!ok)
    {
        ++g_fail;
    }
}

static bool AllNamesStartWith(const std::vector<pinyin::Hit>& hits, const char* prefix)
{
    const size_t n = std::strlen(prefix);
    for (const pinyin::Hit& h : hits)
    {
        if (std::strncmp(kPinyinEntries[h.index].name, prefix, n) != 0)
        {
            std::printf("      offending row: %s\n", kPinyinEntries[h.index].name);
            return false;
        }
    }
    return true;
}

static bool AllVendorsStartWith(const std::vector<pinyin::Hit>& hits, const char* prefix)
{
    const std::string want(prefix);
    for (const pinyin::Hit& h : hits)
    {
        const std::string key = pinyin::VendorKey(kPinyinEntries[h.index].vendor);
        if (!pinyin::StartsWith(key, want))
        {
            std::printf(
                "      offending row: %s (vendor=%s)\n", kPinyinEntries[h.index].name,
                kPinyinEntries[h.index].vendor);
            return false;
        }
    }
    return true;
}

static const char* TopName(const std::vector<pinyin::Hit>& hits)
{
    return hits.empty() ? "(none)" : kPinyinEntries[hits[0].index].name;
}

// Everything returned by an @query must actually belong to that class.
static bool AllInClass(const std::vector<pinyin::Hit>& hits, const char* token)
{
    const std::string wantLower = pinyin::ToLowerAscii(std::string(token));
    const std::string wantVendor = pinyin::VendorKey(token);
    for (const pinyin::Hit& h : hits)
    {
        if (!pinyin::InClass(kPinyinEntries[h.index], wantLower, wantVendor))
        {
            std::printf(
                "      offending row: %s (vendor=%s)\n", kPinyinEntries[h.index].name,
                kPinyinEntries[h.index].vendor);
            return false;
        }
    }
    return true;
}

int main()
{
    std::printf("entries in table: %d\n\n", kPinyinEntryCount);

    // VendorKey / StartsWith are the basis of the @ filter.
    Check(pinyin::VendorKey("Boris FX") == "borisfx", "VendorKey folds case and spaces");
    Check(pinyin::VendorKey("Red Giant Universe") == "redgiantuniverse", "VendorKey on a long vendor");
    Check(pinyin::VendorKey("") == "", "VendorKey of an empty vendor");
    Check(pinyin::StartsWith("borisfx", "boris"), "StartsWith matches a prefix");
    Check(!pinyin::StartsWith("borisfx", "sapphire"), "StartsWith rejects a different vendor");
    Check(pinyin::NameStartsWith("BCCAlphaProcess", "bcc"), "NameStartsWith is case-insensitive");
    Check(!pinyin::NameStartsWith("S_Aurora", "bcc"), "NameStartsWith rejects another name");

    std::vector<pinyin::Hit> cls;
    std::vector<pinyin::Hit> bare;
    std::vector<pinyin::Hit> hits;

    // "@BCC" is a name prefix, not a vendor: the Continuum names all start with it.
    pinyin::SearchQuery("@BCC", cls, 1000);
    Check(!cls.empty(), "@BCC returns rows");
    Check(AllNamesStartWith(cls, "BCC"), "@BCC returns only BCC names");
    std::printf("      @BCC -> %d rows, first=%s\n", static_cast<int>(cls.size()), TopName(cls));

    pinyin::SearchQuery("BCC", bare, 1000);
    Check(!bare.empty(), "BCC returns rows");
    Check(AllNamesStartWith(std::vector<pinyin::Hit>(bare.begin(), bare.begin() + (bare.size() < 322 ? bare.size() : 322)), "BCC"),
          "BCC puts the whole class at the top even without the @");

    pinyin::SearchQuery("@bcc", hits, 1000);
    Check(!hits.empty() && hits[0].index == cls[0].index, "@bcc is case-insensitive");

    // "@boris" is a vendor: the Boris effects are not all named BCC*.
    pinyin::SearchQuery("@boris", cls, 1000);
    Check(!cls.empty() && AllVendorsStartWith(cls, "boris"), "@boris returns only Boris FX entries");
    std::printf("      @boris -> %d rows, first=%s\n", static_cast<int>(cls.size()), TopName(cls));
    Check(cls.size() >= 400, "Boris FX has at least 400 effects");

    pinyin::SearchQuery("@sapphire", cls, 1000);
    Check(!cls.empty() && AllVendorsStartWith(cls, "sapphire"), "@sapphire returns only Sapphire entries");
    std::printf("      @sapphire -> %d rows\n", static_cast<int>(cls.size()));

    pinyin::SearchQuery("@Red Giant Universe", cls, 1000);
    Check(!cls.empty() && AllVendorsStartWith(cls, "redgiant"), "@Red Giant Universe works with spaces");

    // A bare vendor name must not be worse than the @-form.
    pinyin::SearchQuery("boris", bare, 1000);
    Check(!bare.empty() && AllVendorsStartWith(
              std::vector<pinyin::Hit>(bare.begin(), bare.begin() + (bare.size() < 481 ? bare.size() : 481)),
              "boris"),
          "bare 'boris' starts with the whole Boris FX class");

    // Aliases: "@BFX" must reach exactly what "@Boris FX" reaches.
    pinyin::SearchQuery("@Boris FX", cls, 1000);
    pinyin::SearchQuery("@BFX", hits, 1000);
    Check(!hits.empty() && hits.size() == cls.size(), "@BFX returns as many rows as @Boris FX");
    Check(!hits.empty() && hits[0].index == cls[0].index, "@BFX starts on the same row");
    std::printf("      @BFX -> %d rows, first=%s\n", static_cast<int>(hits.size()), TopName(hits));

    pinyin::SearchQuery("@Red Giant Universe", cls, 1000);
    pinyin::SearchQuery("@RGU", hits, 1000);
    Check(!hits.empty() && hits.size() == cls.size(), "@RGU returns as many rows as @Red Giant Universe");
    Check(!hits.empty() && hits[0].index == cls[0].index, "@RGU starts on the same row");
    std::printf("      @RGU -> %d rows\n", static_cast<int>(hits.size()));

    pinyin::SearchQuery("@sap", hits, 1000);
    Check(!hits.empty() && AllVendorsStartWith(hits, "sapphire"), "@sap reaches Sapphire");

    // Every other class has a short key too, including the preset categories.
    struct AliasCase
    {
        const char* alias;
        const char* vendorPrefix;
    };
    const AliasCase kCases[] = {
        {"@trap", "trapcode"}, {"@mb", "magicbullet"},   {"@vc", "videocopilot"},
        {"@maxon", "maxon"},   {"@legacy", "legacy"},    {"@text", "text"},
        {"@shapes", "shapes"}, {"@trans", "transitions"}, {"@image", "image"},
        {"@bg", "backgrounds"}, {"@beh", "behaviors"},   {"@syn", "synthetics"},
        {"@sfx", "soundeffects"}, {"@ae", "adobeexpress"}, {"@rgv", "redgiantvfx"},
    };
    for (const AliasCase& c : kCases)
    {
        pinyin::SearchQuery(c.alias, hits, 1000);
        bool anyVendor = false;
        const std::string want(c.vendorPrefix);
        for (const pinyin::Hit& h : hits)
        {
            if (pinyin::StartsWith(pinyin::VendorKey(kPinyinEntries[h.index].vendor), want))
            {
                anyVendor = true;
                break;
            }
        }
        char what[96];
        std::snprintf(what, sizeof(what), "%s reaches the %s class", c.alias, c.vendorPrefix);
        // A class is a vendor prefix OR a name prefix, so the rows may include
        // same-prefixed names; what matters is that the class itself is in there.
        Check(!hits.empty() && anyVendor, what);
    }

    pinyin::SearchQuery("@BFX", hits, 1000);
    const int aliasRows = static_cast<int>(hits.size());
    pinyin::SearchQuery("@BFX", hits, 1000);
    Check(aliasRows > 400, "@BFX is a big class, not an empty one");
    pinyin::SearchQuery("@bfx", hits, 1000);
    Check(static_cast<int>(hits.size()) == aliasRows, "aliases are case-insensitive");
    Check(pinyin::AliasForVendor("Boris FX") == "bfx", "AliasForVendor maps back for the class list");
    Check(pinyin::AliasForVendor("Legacy") == "legacy", "AliasForVendor falls back to the folded name");

    pinyin::SearchQuery("@", hits, 1000);
    Check(hits.empty(), "@ alone produces no entry rows");
    Check(pinyin::ParseQuery("@").listClasses, "@ alone is the class browser");
    Check(!pinyin::ParseQuery("BCC").listClasses, "bare BCC does not open the browser");
    Check(pinyin::ParseQuery("@BCC").group == "BCC", "@BCC parses into a class filter");

    // ---------------------------------------------------------------------
    // "#" filters by kind (effect / preset) and combines with "@".
    // ---------------------------------------------------------------------
    Check(pinyin::ParseQuery("#").listKinds, "# alone is the kind browser");
    Check(pinyin::KindOf("效果") == 1, "KindOf understands 效果");
    Check(pinyin::KindOf("预设") == 2, "KindOf understands 预设");
    Check(pinyin::KindOf("presets") == 2, "KindOf understands the english spelling");
    Check(pinyin::KindOf("zzz") == -1, "KindOf rejects an unknown kind");

    pinyin::SearchQuery("#效果", hits, 1000);
    {
        bool allEffects = !hits.empty();
        for (const pinyin::Hit& h : hits)
        {
            if (kPinyinEntries[h.index].is_preset)
            {
                allEffects = false;
                break;
            }
        }
        Check(allEffects, "#效果 returns effects only");
        std::printf("      #效果 -> %d rows (capped by the limit)\n", static_cast<int>(hits.size()));
    }

    pinyin::SearchQuery("#预设", hits, 1000);
    {
        bool allPresets = !hits.empty();
        for (const pinyin::Hit& h : hits)
        {
            if (!kPinyinEntries[h.index].is_preset)
            {
                allPresets = false;
                break;
            }
        }
        Check(allPresets, "#预设 returns presets only");
        std::printf("      #预设 -> %d rows\n", static_cast<int>(hits.size()));
    }

    pinyin::SearchQuery("#preset", hits, 1000);
    Check(!hits.empty() && kPinyinEntries[hits[0].index].is_preset, "#preset works in english");

    pinyin::SearchQuery("#zzz", hits, 1000);
    Check(hits.empty(), "an unknown kind returns nothing");

    pinyin::SearchQuery("@boris #效果", hits, 1000);
    Check(!hits.empty() && AllVendorsStartWith(hits, "boris"), "@boris #效果 keeps the class");
    Check(!hits.empty() && !kPinyinEntries[hits[0].index].is_preset, "@boris #效果 keeps the kind");
    std::printf("      @boris #效果 -> %d rows\n", static_cast<int>(hits.size()));

    pinyin::SearchQuery("#预设 @legacy", hits, 1000);
    {
        bool ok = !hits.empty();
        for (const pinyin::Hit& h : hits)
        {
            if (!kPinyinEntries[h.index].is_preset)
            {
                ok = false;
                break;
            }
        }
        Check(ok, "#预设 @legacy returns presets only, in any order");
    }

    pinyin::SearchQuery("@bfx #效果 blur", hits, 1000);
    Check(!hits.empty() && AllVendorsStartWith(hits, "boris"), "@bfx #效果 blur keeps the class");
    Check(!hits.empty() && !kPinyinEntries[hits[0].index].is_preset, "@bfx #效果 blur keeps the kind");

    std::vector<pinyin::GroupInfo> kinds;
    pinyin::ListKinds(kinds);
    Check(kinds.size() == 2, "the kind browser has two rows");
    {
        int total = 0;
        for (const pinyin::GroupInfo& k : kinds)
        {
            total += k.count;
            Check(!k.key.empty(), "a kind row carries the key to type");
            std::printf("      kind %-8s %5d  key=%s\n", k.name.c_str(), k.count, k.key.c_str());
        }
        Check(total == kPinyinEntryCount, "the kind counts add up to the whole table");
    }

    std::vector<pinyin::GroupInfo> groups;
    pinyin::ListGroups(groups, 2);
    Check(!groups.empty(), "the class list is not empty");
    bool hasBoris = false;
    bool hasSapphire = false;
    for (const pinyin::GroupInfo& g : groups)
    {
        std::printf("      class %-22s %4d\n", g.name.c_str(), g.count);
        if (g.name == "Boris FX")
        {
            hasBoris = true;
            Check(g.count >= 400, "Boris FX class count looks right");
        }
        if (g.name == "Sapphire")
        {
            hasSapphire = true;
        }
    }
    Check(hasBoris, "class list contains Boris FX");
    Check(hasSapphire, "class list contains Sapphire");
    Check(groups.size() <= 64, "class list stays browsable (scrolls, does not explode)");

    // "@S" is both a vendor prefix (Sapphire, Sound Effects) and a name prefix
    // (S_*, Smear, Swish...): everything returned must be in that class.
    pinyin::SearchQuery("@S", hits, 1000);
    Check(!hits.empty() && AllInClass(hits, "S"), "@S returns only rows in the S class");
    std::printf("      @S   -> %d rows\n", static_cast<int>(hits.size()));

    pinyin::SearchQuery("@ZZZZNOPE", hits, 1000);
    Check(hits.empty(), "an unknown class returns nothing");

    // The pinyin search itself must not have regressed.
    pinyin::SearchQuery("gsmh", hits, 50);
    Check(!hits.empty() && std::strcmp(TopName(hits), "高斯模糊") == 0, "gsmh top hit is 高斯模糊");
    pinyin::SearchQuery("gaosimohu", hits, 50);
    Check(!hits.empty() && std::strcmp(TopName(hits), "高斯模糊") == 0, "gaosimohu top hit is 高斯模糊");
    pinyin::SearchQuery("fangkuangmohu", hits, 50);
    Check(!hits.empty(), "fangkuangmohu (方框模糊) still matches");

    // Class plus a narrowing term.
    pinyin::SearchQuery("@BCC blur", hits, 1000);
    Check(AllNamesStartWith(hits, "BCC"), "@BCC blur stays inside the BCC class");
    std::printf("      @BCC blur -> %d rows\n", static_cast<int>(hits.size()));

    // Usage history: the entries the user actually applies come first, but the
    // boost must not let a weak match outrank a better one.
    {
        std::vector<pinyin::Hit> plain;
        pinyin::SearchQuery("mohu", plain, 50);
        Check(plain.size() >= 2, "mohu returns several rows to reorder");

        pinyin::UsageTable usage;
        const char* loser = kPinyinEntries[plain[1].index].name;
        for (int i = 0; i < 15; ++i)
        {
            usage.Bump(loser);
        }
        Check(usage.Count(loser) == 15, "UsageTable counts bumps");
        Check(usage.Boost(loser) == 90, "UsageTable boost is capped");

        pinyin::SearchQuery("mohu", hits, 50, &usage);
        Check(!hits.empty() && std::strcmp(kPinyinEntries[hits[0].index].name, loser) == 0,
              "a frequently used row moves to the top of its tier");
        std::printf("      mohu with usage -> first=%s\n", TopName(hits));

        // Tiers still win: an exact hit must stay above a boosted substring hit.
        pinyin::UsageTable exactUsage;
        exactUsage.Bump(kPinyinEntries[plain[plain.size() - 1].index].name);
        pinyin::SearchQuery("mohu", hits, 50, &exactUsage);
        pinyin::SearchQuery("mohu", plain, 50);
        Check(hits[0].index == plain[0].index, "a boost never crosses the relevance tiers");

        // History is keyed by name, so an index rebuild keeps it.
        pinyin::UsageTable reloaded;
        for (size_t i = 0; i < usage.Size(); ++i)
        {
            reloaded.Add(usage.NameAt(i).c_str(), usage.CountAt(i));
        }
        Check(reloaded.Count(loser) == 15, "history survives a save/load round trip");
    }

    // Recency: the popup shows the most recently used entries when it opens,
    // newest first, and the list resolves back to table rows.
    {
        pinyin::UsageTable recent;
        std::vector<pinyin::Hit> plain;
        pinyin::SearchQuery("mohu", plain, 8);
        Check(plain.size() >= 3, "enough rows to play with recency");

        const char* a = kPinyinEntries[plain[0].index].name;
        const char* b = kPinyinEntries[plain[1].index].name;
        const char* c = kPinyinEntries[plain[2].index].name;

        recent.Bump(a);
        recent.Bump(b);
        recent.Bump(c);
        recent.Bump(a); // a is used twice, c was used last

        std::vector<pinyin::Hit> order;
        pinyin::RecentHits(recent, order, 10);
        Check(order.size() == 3, "RecentHits returns what was used");
        Check(std::strcmp(kPinyinEntries[order[0].index].name, a) == 0, "most recent use comes first");
        Check(std::strcmp(kPinyinEntries[order[1].index].name, c) == 0, "then the one before it");
        Check(std::strcmp(kPinyinEntries[order[2].index].name, b) == 0, "oldest last");
        Check(order[0].score == 2, "the recent row carries its use count");

        pinyin::RecentHits(recent, order, 2);
        Check(order.size() == 2, "RecentHits honours the limit");

        // A history entry whose plug-in is gone must not crash the list.
        pinyin::UsageTable stale;
        stale.Bump("ThisEffectDoesNotExist");
        pinyin::RecentHits(stale, order, 10);
        Check(order.empty(), "RecentHits skips entries that are no longer installed");

        // The save/load round trip keeps both numbers (3 columns).
        pinyin::UsageTable reloaded;
        for (size_t i = 0; i < recent.Size(); ++i)
        {
            reloaded.Add(recent.NameAt(i).c_str(), recent.CountAt(i), recent.LastUsedAt(i));
        }
        pinyin::RecentHits(reloaded, order, 10);
        Check(order.size() == 3 && std::strcmp(kPinyinEntries[order[0].index].name, a) == 0,
              "recency survives a save/load round trip");

        // Old two-column histories still count (their order is unknown).
        pinyin::UsageTable old;
        old.Add(a, 4);
        Check(old.Count(a) == 4 && old.LastUsedAt(0) == 0, "a legacy row keeps its count");
        old.Bump(b);
        pinyin::RecentHits(old, order, 10);
        Check(!order.empty() && std::strcmp(kPinyinEntries[order[0].index].name, b) == 0,
              "a new use still outranks a legacy row");
    }

    std::printf("\nfailures: %d\n", g_fail);
    return g_fail;
}
