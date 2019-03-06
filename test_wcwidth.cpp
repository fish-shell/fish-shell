#include <cstring>
#include <wchar.h>
#include <iostream>

int main(int argc, char** argv) {
    // This initializes the locale according to the environment variables.
    // That means if the environment isn't set up for unicode, this whole excercise is pointless.
    setlocale(LC_ALL, "");

    // The locale is C, so try to get UTF-8 somehow.
    // Note that we don't handle locales with other encodings. I've never seen one used on purpose,
    // the biggest problem is the POSIX default.
    if (strcmp(setlocale(LC_ALL, NULL),"C") == 0) {
        // The list of all UTF-8 locales in glibc 2.28.
        // We try C.UTF-8 first, but even that's not guaranteed to be there.
        const char* locales[] = {"C.UTF-8", "aa_DJ.UTF-8", "af_ZA.UTF-8", "an_ES.UTF-8", "ar_BH.UTF-8", "ar_DZ.UTF-8",
                                 "ar_EG.UTF-8", "ar_IQ.UTF-8", "ar_JO.UTF-8", "ar_KW.UTF-8", "ar_LB.UTF-8", "ar_LY.UTF-8",
                                 "ar_MA.UTF-8", "ar_OM.UTF-8", "ar_QA.UTF-8", "ar_SA.UTF-8", "ar_SD.UTF-8", "ar_SY.UTF-8",
                                 "ar_TN.UTF-8", "ar_YE.UTF-8", "ast_ES.UTF-8", "be_BY.UTF-8", "bg_BG.UTF-8", "bhb_IN.UTF-8",
                                 "br_FR.UTF-8", "bs_BA.UTF-8", "ca_AD.UTF-8", "ca_ES.UTF-8", "ca_FR.UTF-8", "ca_IT.UTF-8",
                                 "cs_CZ.UTF-8", "cy_GB.UTF-8", "da_DK.UTF-8", "de_AT.UTF-8", "de_BE.UTF-8", "de_CH.UTF-8",
                                 "de_DE.UTF-8", "de_IT.UTF-8", "de_LI.UTF-8", "de_LU.UTF-8", "el_GR.UTF-8", "el_CY.UTF-8",
                                 "en_AU.UTF-8", "en_BW.UTF-8", "en_CA.UTF-8", "en_DK.UTF-8", "en_GB.UTF-8", "en_HK.UTF-8",
                                 "en_IE.UTF-8", "en_NZ.UTF-8", "en_PH.UTF-8", "en_SC.UTF-8", "en_SG.UTF-8", "en_US.UTF-8",
                                 "en_ZA.UTF-8", "en_ZW.UTF-8", "es_AR.UTF-8", "es_BO.UTF-8", "es_CL.UTF-8", "es_CO.UTF-8",
                                 "es_CR.UTF-8", "es_DO.UTF-8", "es_EC.UTF-8", "es_ES.UTF-8", "es_GT.UTF-8", "es_HN.UTF-8",
                                 "es_MX.UTF-8", "es_NI.UTF-8", "es_PA.UTF-8", "es_PE.UTF-8", "es_PR.UTF-8", "es_PY.UTF-8",
                                 "es_SV.UTF-8", "es_US.UTF-8", "es_UY.UTF-8", "es_VE.UTF-8", "et_EE.UTF-8", "eu_ES.UTF-8",
                                 "fi_FI.UTF-8", "fo_FO.UTF-8", "fr_BE.UTF-8", "fr_CA.UTF-8", "fr_CH.UTF-8", "fr_FR.UTF-8",
                                 "fr_LU.UTF-8", "ga_IE.UTF-8", "gd_GB.UTF-8", "gl_ES.UTF-8", "gv_GB.UTF-8", "he_IL.UTF-8",
                                 "hr_HR.UTF-8", "hsb_DE.UTF-8", "hu_HU.UTF-8", "id_ID.UTF-8", "is_IS.UTF-8", "it_CH.UTF-8",
                                 "it_IT.UTF-8", "ja_JP.UTF-8", "ka_GE.UTF-8", "kk_KZ.UTF-8", "kl_GL.UTF-8", "ko_KR.UTF-8",
                                 "ku_TR.UTF-8", "kw_GB.UTF-8", "lg_UG.UTF-8", "lt_LT.UTF-8", "lv_LV.UTF-8", "mg_MG.UTF-8",
                                 "mi_NZ.UTF-8", "mk_MK.UTF-8", "ms_MY.UTF-8", "mt_MT.UTF-8", "nb_NO.UTF-8", "nl_BE.UTF-8",
                                 "nl_NL.UTF-8", "nn_NO.UTF-8", "oc_FR.UTF-8", "om_KE.UTF-8", "pl_PL.UTF-8", "pt_BR.UTF-8",
                                 "pt_PT.UTF-8", "ro_RO.UTF-8", "ru_RU.UTF-8", "ru_UA.UTF-8", "sk_SK.UTF-8", "sl_SI.UTF-8",
                                 "so_DJ.UTF-8", "so_KE.UTF-8", "so_SO.UTF-8", "sq_AL.UTF-8", "st_ZA.UTF-8", "sv_FI.UTF-8",
                                 "sv_SE.UTF-8", "tcy_IN.UTF-8", "tg_TJ.UTF-8", "th_TH.UTF-8", "tl_PH.UTF-8", "tr_CY.UTF-8",
                                 "tr_TR.UTF-8", "uk_UA.UTF-8", "uz_UZ.UTF-8", "wa_BE.UTF-8", "xh_ZA.UTF-8", "yi_US.UTF-8",
                                 "zh_CN.UTF-8", "zh_HK.UTF-8", "zh_SG.UTF-8", "zh_TW.UTF-8", "zu_ZA.UTF-8"
        };
        for (const char *loc : locales) {
            const char *ret = setlocale(LC_ALL, loc);
            if (ret) break;
        }
    }
    std::cout << wcwidth(L'😃');
    return 0;
}
