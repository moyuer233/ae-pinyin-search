/*******************************************************************/
/*                                                                 */
/* AE Pinyin Search - the host's own effect names                   */
/*                                                                 */
/* Asks After Effects for every installed effect (display name +    */
/* match name) and hands the list to EffectNameMatch, which answers */
/* what addProperty() should be given for an index row.              */
/*                                                                 */
/*******************************************************************/

#ifndef AEPINYINSEARCH_EFFECTNAMES_H
#define AEPINYINSEARCH_EFFECTNAMES_H

#include "AE_GeneralPlug.h"
#include "DiagLog.h"
#include "EffectNameMatch.h"

// Thin After Effects wrapper: enumerate once, then forward.
class EffectNames : public EffectNameMatch
{
public:
    EffectNames() : i_built(false) {}

    // Built on first use - the popup's first Show() - rather than while the
    // plug-in is still loading, because other plug-ins may register their effects
    // after this one and a table collected too early would be missing them.
    void EnsureBuilt(SPBasicSuite* spbP)
    {
        if (!i_built)
        {
            Build(spbP);
        }
    }

    void Build(SPBasicSuite* spbP)
    {
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
                AddEntry(display, match);
            }
        }
        spbP->ReleaseSuite(kAEGPEffectSuite, kAEGPEffectSuiteVersion5);

        Index();
        i_built = true;
        AEPinyinLog("effect names: %d installed effect(s) from the host", static_cast<int>(Size()));
    }

private:
    bool i_built;
};

#endif // AEPINYINSEARCH_EFFECTNAMES_H
