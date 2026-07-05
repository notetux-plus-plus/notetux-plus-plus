/* Scintilla source code edit control
 * CaseConvert.c — C translation of scintilla/src/CaseConvert.cxx
 *
 * ── Translation summary ───────────────────────────────────────────────
 *
 *   constexpr int   symmetricCaseConversionRanges[]  →  static const int   (unchanged)
 *   constexpr int   symmetricCaseConversions[]        →  static const int   (unchanged)
 *   constexpr std::string_view complexCaseConversions →  static const char  (unchanged)
 *   constexpr size_t maxConversionLength              →  #define MAX_CONVERSION_LENGTH
 *
 *   class CaseConverter : public ICaseConverter
 *     Build phase:
 *       std::vector<CharacterConversion> characterToConversion
 *         →  CharacterConversion *build; size_t buildLen; size_t buildCap
 *     Search phase (after FinishedAdding):
 *       std::vector<int>              characters   →  int *characters; size_t count
 *       std::vector<ConversionString> conversions  →  ConversionString *conversions
 *
 *   std::sort(begin, end)   →  qsort with CC_Compare comparator
 *   std::lower_bound(…)     →  CC_LowerBound (manual binary search)
 *   std::size(array)        →  sizeof(array) / sizeof(array[0])
 *   std::string_view NextField(std::string_view &)
 *     →  static const char *NextField(const char **ptr, size_t *len, size_t *fieldLen)
 *        advances *ptr past the '|' separator
 *
 *   CaseConverter::Find(int character)  →  static function CC_Find
 *   enum class CaseConversion           →  CaseConversion  (typedef enum in header)
 *
 *   CaseConverter caseConvList[3]       →  static CaseConverter s_conv[3]
 *     Each entry is lazily initialised on first call to ConverterForConversion.
 *
 *   std::string CaseConvertString(…)    →  dropped (heap-allocated variant)
 *        Use the buffer-based overload instead.
 */

#include "CaseConvert.h"
#include "UniConversion.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ── Tables (verbatim from CaseConvert.cxx) ─────────────────────────── */

static const int symmetricCaseConversionRanges[] = {
/* lower, upper, range length, range pitch */
97,65,26,1,
224,192,23,1,
248,216,7,1,
257,256,24,2,
314,313,8,2,
331,330,23,2,
462,461,8,2,
479,478,9,2,
505,504,20,2,
547,546,9,2,
583,582,5,2,
945,913,17,1,
963,931,9,1,
985,984,12,2,
1072,1040,32,1,
1104,1024,16,1,
1121,1120,17,2,
1163,1162,27,2,
1218,1217,7,2,
1233,1232,48,2,
1377,1329,38,1,
4304,7312,43,1,
7681,7680,75,2,
7841,7840,48,2,
7936,7944,8,1,
7952,7960,6,1,
7968,7976,8,1,
7984,7992,8,1,
8000,8008,6,1,
8032,8040,8,1,
8560,8544,16,1,
9424,9398,26,1,
11312,11264,48,1,
11393,11392,50,2,
11520,4256,38,1,
42561,42560,23,2,
42625,42624,14,2,
42787,42786,7,2,
42803,42802,31,2,
42879,42878,5,2,
42903,42902,10,2,
42933,42932,8,2,
65345,65313,26,1,
66600,66560,40,1,
66776,66736,36,1,
66967,66928,11,1,
66979,66940,15,1,
66995,66956,7,1,
68800,68736,51,1,
68976,68944,22,1,
71872,71840,32,1,
93792,93760,32,1,
125218,125184,34,1,
};

static const int symmetricCaseConversions[] = {
/* lower, upper */
255,376,
307,306,
309,308,
311,310,
378,377,
380,379,
382,381,
384,579,
387,386,
389,388,
392,391,
396,395,
402,401,
405,502,
409,408,
410,573,
411,42972,
414,544,
417,416,
419,418,
421,420,
424,423,
429,428,
432,431,
436,435,
438,437,
441,440,
445,444,
447,503,
454,452,
457,455,
460,458,
477,398,
499,497,
501,500,
572,571,
575,11390,
576,11391,
578,577,
592,11375,
593,11373,
594,11376,
595,385,
596,390,
598,393,
599,394,
601,399,
603,400,
604,42923,
608,403,
609,42924,
611,404,
612,42955,
613,42893,
614,42922,
616,407,
617,406,
618,42926,
619,11362,
620,42925,
623,412,
625,11374,
626,413,
629,415,
637,11364,
640,422,
642,42949,
643,425,
647,42929,
648,430,
649,580,
650,433,
651,434,
652,581,
658,439,
669,42930,
670,42928,
881,880,
883,882,
887,886,
891,1021,
892,1022,
893,1023,
940,902,
941,904,
942,905,
943,906,
972,908,
973,910,
974,911,
983,975,
1010,1017,
1011,895,
1016,1015,
1019,1018,
1231,1216,
4349,7357,
4350,7358,
4351,7359,
7306,7305,
7545,42877,
7549,11363,
7566,42950,
8017,8025,
8019,8027,
8021,8029,
8023,8031,
8048,8122,
8049,8123,
8050,8136,
8051,8137,
8052,8138,
8053,8139,
8054,8154,
8055,8155,
8056,8184,
8057,8185,
8058,8170,
8059,8171,
8060,8186,
8061,8187,
8112,8120,
8113,8121,
8144,8152,
8145,8153,
8160,8168,
8161,8169,
8165,8172,
8526,8498,
8580,8579,
11361,11360,
11365,570,
11366,574,
11368,11367,
11370,11369,
11372,11371,
11379,11378,
11382,11381,
11500,11499,
11502,11501,
11507,11506,
11559,4295,
11565,4301,
42874,42873,
42876,42875,
42892,42891,
42897,42896,
42899,42898,
42900,42948,
42952,42951,
42954,42953,
42957,42956,
42961,42960,
42967,42966,
42969,42968,
42971,42970,
42998,42997,
43859,42931,
67003,66964,
67004,66965,
};

/* Original | Folded | Upper | Lower | (pipe-separated UTF-8) */
static const char complexCaseConversions[] =
"\xc2\xb5|\xce\xbc|\xce\x9c||"
"\xc3\x9f|ss|SS||"
"\xc4\xb0|i\xcc\x87||i\xcc\x87|"
"\xc4\xb1||I||"
"\xc5\x89|\xca\xbcn|\xca\xbcN||"
"\xc5\xbf|s|S||"
"\xc7\x85|\xc7\x86|\xc7\x84|\xc7\x86|"
"\xc7\x88|\xc7\x89|\xc7\x87|\xc7\x89|"
"\xc7\x8b|\xc7\x8c|\xc7\x8a|\xc7\x8c|"
"\xc7\xb0|j\xcc\x8c|J\xcc\x8c||"
"\xc7\xb2|\xc7\xb3|\xc7\xb1|\xc7\xb3|"
"\xcd\x85|\xce\xb9|\xce\x99||"
"\xce\x90|\xce\xb9\xcc\x88\xcc\x81|\xce\x99\xcc\x88\xcc\x81||"
"\xce\xb0|\xcf\x85\xcc\x88\xcc\x81|\xce\xa5\xcc\x88\xcc\x81||"
"\xcf\x82|\xcf\x83|\xce\xa3||"
"\xcf\x90|\xce\xb2|\xce\x92||"
"\xcf\x91|\xce\xb8|\xce\x98||"
"\xcf\x95|\xcf\x86|\xce\xa6||"
"\xcf\x96|\xcf\x80|\xce\xa0||"
"\xcf\xb0|\xce\xba|\xce\x9a||"
"\xcf\xb1|\xcf\x81|\xce\xa1||"
"\xcf\xb4|\xce\xb8||\xce\xb8|"
"\xcf\xb5|\xce\xb5|\xce\x95||"
"\xd6\x87|\xd5\xa5\xd6\x82|\xd4\xb5\xd5\x92||"
"\xe1\x8e\xa0|||\xea\xad\xb0|"
"\xe1\x8e\xa1|||\xea\xad\xb1|"
"\xe1\x8e\xa2|||\xea\xad\xb2|"
"\xe1\x8e\xa3|||\xea\xad\xb3|"
"\xe1\x8e\xa4|||\xea\xad\xb4|"
"\xe1\x8e\xa5|||\xea\xad\xb5|"
"\xe1\x8e\xa6|||\xea\xad\xb6|"
"\xe1\x8e\xa7|||\xea\xad\xb7|"
"\xe1\x8e\xa8|||\xea\xad\xb8|"
"\xe1\x8e\xa9|||\xea\xad\xb9|"
"\xe1\x8e\xaa|||\xea\xad\xba|"
"\xe1\x8e\xab|||\xea\xad\xbb|"
"\xe1\x8e\xac|||\xea\xad\xbc|"
"\xe1\x8e\xad|||\xea\xad\xbd|"
"\xe1\x8e\xae|||\xea\xad\xbe|"
"\xe1\x8e\xaf|||\xea\xad\xbf|"
"\xe1\x8e\xb0|||\xea\xae\x80|"
"\xe1\x8e\xb1|||\xea\xae\x81|"
"\xe1\x8e\xb2|||\xea\xae\x82|"
"\xe1\x8e\xb3|||\xea\xae\x83|"
"\xe1\x8e\xb4|||\xea\xae\x84|"
"\xe1\x8e\xb5|||\xea\xae\x85|"
"\xe1\x8e\xb6|||\xea\xae\x86|"
"\xe1\x8e\xb7|||\xea\xae\x87|"
"\xe1\x8e\xb8|||\xea\xae\x88|"
"\xe1\x8e\xb9|||\xea\xae\x89|"
"\xe1\x8e\xba|||\xea\xae\x8a|"
"\xe1\x8e\xbb|||\xea\xae\x8b|"
"\xe1\x8e\xbc|||\xea\xae\x8c|"
"\xe1\x8e\xbd|||\xea\xae\x8d|"
"\xe1\x8e\xbe|||\xea\xae\x8e|"
"\xe1\x8e\xbf|||\xea\xae\x8f|"
"\xe1\x8f\x80|||\xea\xae\x90|"
"\xe1\x8f\x81|||\xea\xae\x91|"
"\xe1\x8f\x82|||\xea\xae\x92|"
"\xe1\x8f\x83|||\xea\xae\x93|"
"\xe1\x8f\x84|||\xea\xae\x94|"
"\xe1\x8f\x85|||\xea\xae\x95|"
"\xe1\x8f\x86|||\xea\xae\x96|"
"\xe1\x8f\x87|||\xea\xae\x97|"
"\xe1\x8f\x88|||\xea\xae\x98|"
"\xe1\x8f\x89|||\xea\xae\x99|"
"\xe1\x8f\x8a|||\xea\xae\x9a|"
"\xe1\x8f\x8b|||\xea\xae\x9b|"
"\xe1\x8f\x8c|||\xea\xae\x9c|"
"\xe1\x8f\x8d|||\xea\xae\x9d|"
"\xe1\x8f\x8e|||\xea\xae\x9e|"
"\xe1\x8f\x8f|||\xea\xae\x9f|"
"\xe1\x8f\x90|||\xea\xae\xa0|"
"\xe1\x8f\x91|||\xea\xae\xa1|"
"\xe1\x8f\x92|||\xea\xae\xa2|"
"\xe1\x8f\x93|||\xea\xae\xa3|"
"\xe1\x8f\x94|||\xea\xae\xa4|"
"\xe1\x8f\x95|||\xea\xae\xa5|"
"\xe1\x8f\x96|||\xea\xae\xa6|"
"\xe1\x8f\x97|||\xea\xae\xa7|"
"\xe1\x8f\x98|||\xea\xae\xa8|"
"\xe1\x8f\x99|||\xea\xae\xa9|"
"\xe1\x8f\x9a|||\xea\xae\xaa|"
"\xe1\x8f\x9b|||\xea\xae\xab|"
"\xe1\x8f\x9c|||\xea\xae\xac|"
"\xe1\x8f\x9d|||\xea\xae\xad|"
"\xe1\x8f\x9e|||\xea\xae\xae|"
"\xe1\x8f\x9f|||\xea\xae\xaf|"
"\xe1\x8f\xa0|||\xea\xae\xb0|"
"\xe1\x8f\xa1|||\xea\xae\xb1|"
"\xe1\x8f\xa2|||\xea\xae\xb2|"
"\xe1\x8f\xa3|||\xea\xae\xb3|"
"\xe1\x8f\xa4|||\xea\xae\xb4|"
"\xe1\x8f\xa5|||\xea\xae\xb5|"
"\xe1\x8f\xa6|||\xea\xae\xb6|"
"\xe1\x8f\xa7|||\xea\xae\xb7|"
"\xe1\x8f\xa8|||\xea\xae\xb8|"
"\xe1\x8f\xa9|||\xea\xae\xb9|"
"\xe1\x8f\xaa|||\xea\xae\xba|"
"\xe1\x8f\xab|||\xea\xae\xbb|"
"\xe1\x8f\xac|||\xea\xae\xbc|"
"\xe1\x8f\xad|||\xea\xae\xbd|"
"\xe1\x8f\xae|||\xea\xae\xbe|"
"\xe1\x8f\xaf|||\xea\xae\xbf|"
"\xe1\x8f\xb0|||\xe1\x8f\xb8|"
"\xe1\x8f\xb1|||\xe1\x8f\xb9|"
"\xe1\x8f\xb2|||\xe1\x8f\xba|"
"\xe1\x8f\xb3|||\xe1\x8f\xbb|"
"\xe1\x8f\xb4|||\xe1\x8f\xbc|"
"\xe1\x8f\xb5|||\xe1\x8f\xbd|"
"\xe1\x8f\xb8|\xe1\x8f\xb0|\xe1\x8f\xb0||"
"\xe1\x8f\xb9|\xe1\x8f\xb1|\xe1\x8f\xb1||"
"\xe1\x8f\xba|\xe1\x8f\xb2|\xe1\x8f\xb2||"
"\xe1\x8f\xbb|\xe1\x8f\xb3|\xe1\x8f\xb3||"
"\xe1\x8f\xbc|\xe1\x8f\xb4|\xe1\x8f\xb4||"
"\xe1\x8f\xbd|\xe1\x8f\xb5|\xe1\x8f\xb5||"
"\xe1\xb2\x80|\xd0\xb2|\xd0\x92||"
"\xe1\xb2\x81|\xd0\xb4|\xd0\x94||"
"\xe1\xb2\x82|\xd0\xbe|\xd0\x9e||"
"\xe1\xb2\x83|\xd1\x81|\xd0\xa1||"
"\xe1\xb2\x84|\xd1\x82|\xd0\xa2||"
"\xe1\xb2\x85|\xd1\x82|\xd0\xa2||"
"\xe1\xb2\x86|\xd1\x8a|\xd0\xaa||"
"\xe1\xb2\x87|\xd1\xa3|\xd1\xa2||"
"\xe1\xb2\x88|\xea\x99\x8b|\xea\x99\x8a||"
"\xe1\xba\x96|h\xcc\xb1|H\xcc\xb1||"
"\xe1\xba\x97|t\xcc\x88|T\xcc\x88||"
"\xe1\xba\x98|w\xcc\x8a|W\xcc\x8a||"
"\xe1\xba\x99|y\xcc\x8a|Y\xcc\x8a||"
"\xe1\xba\x9a|a\xca\xbe|A\xca\xbe||"
"\xe1\xba\x9b|\xe1\xb9\xa1|\xe1\xb9\xa0||"
"\xe1\xba\x9e|ss||\xc3\x9f|"
"\xe1\xbd\x90|\xcf\x85\xcc\x93|\xce\xa5\xcc\x93||"
"\xe1\xbd\x92|\xcf\x85\xcc\x93\xcc\x80|\xce\xa5\xcc\x93\xcc\x80||"
"\xe1\xbd\x94|\xcf\x85\xcc\x93\xcc\x81|\xce\xa5\xcc\x93\xcc\x81||"
"\xe1\xbd\x96|\xcf\x85\xcc\x93\xcd\x82|\xce\xa5\xcc\x93\xcd\x82||"
"\xe1\xbe\x80|\xe1\xbc\x80\xce\xb9|\xe1\xbc\x88\xce\x99||"
"\xe1\xbe\x81|\xe1\xbc\x81\xce\xb9|\xe1\xbc\x89\xce\x99||"
"\xe1\xbe\x82|\xe1\xbc\x82\xce\xb9|\xe1\xbc\x8a\xce\x99||"
"\xe1\xbe\x83|\xe1\xbc\x83\xce\xb9|\xe1\xbc\x8b\xce\x99||"
"\xe1\xbe\x84|\xe1\xbc\x84\xce\xb9|\xe1\xbc\x8c\xce\x99||"
"\xe1\xbe\x85|\xe1\xbc\x85\xce\xb9|\xe1\xbc\x8d\xce\x99||"
"\xe1\xbe\x86|\xe1\xbc\x86\xce\xb9|\xe1\xbc\x8e\xce\x99||"
"\xe1\xbe\x87|\xe1\xbc\x87\xce\xb9|\xe1\xbc\x8f\xce\x99||"
"\xe1\xbe\x88|\xe1\xbc\x80\xce\xb9|\xe1\xbc\x88\xce\x99|\xe1\xbe\x80|"
"\xe1\xbe\x89|\xe1\xbc\x81\xce\xb9|\xe1\xbc\x89\xce\x99|\xe1\xbe\x81|"
"\xe1\xbe\x8a|\xe1\xbc\x82\xce\xb9|\xe1\xbc\x8a\xce\x99|\xe1\xbe\x82|"
"\xe1\xbe\x8b|\xe1\xbc\x83\xce\xb9|\xe1\xbc\x8b\xce\x99|\xe1\xbe\x83|"
"\xe1\xbe\x8c|\xe1\xbc\x84\xce\xb9|\xe1\xbc\x8c\xce\x99|\xe1\xbe\x84|"
"\xe1\xbe\x8d|\xe1\xbc\x85\xce\xb9|\xe1\xbc\x8d\xce\x99|\xe1\xbe\x85|"
"\xe1\xbe\x8e|\xe1\xbc\x86\xce\xb9|\xe1\xbc\x8e\xce\x99|\xe1\xbe\x86|"
"\xe1\xbe\x8f|\xe1\xbc\x87\xce\xb9|\xe1\xbc\x8f\xce\x99|\xe1\xbe\x87|"
"\xe1\xbe\x90|\xe1\xbc\xa0\xce\xb9|\xe1\xbc\xa8\xce\x99||"
"\xe1\xbe\x91|\xe1\xbc\xa1\xce\xb9|\xe1\xbc\xa9\xce\x99||"
"\xe1\xbe\x92|\xe1\xbc\xa2\xce\xb9|\xe1\xbc\xaa\xce\x99||"
"\xe1\xbe\x93|\xe1\xbc\xa3\xce\xb9|\xe1\xbc\xab\xce\x99||"
"\xe1\xbe\x94|\xe1\xbc\xa4\xce\xb9|\xe1\xbc\xac\xce\x99||"
"\xe1\xbe\x95|\xe1\xbc\xa5\xce\xb9|\xe1\xbc\xad\xce\x99||"
"\xe1\xbe\x96|\xe1\xbc\xa6\xce\xb9|\xe1\xbc\xae\xce\x99||"
"\xe1\xbe\x97|\xe1\xbc\xa7\xce\xb9|\xe1\xbc\xaf\xce\x99||"
"\xe1\xbe\x98|\xe1\xbc\xa0\xce\xb9|\xe1\xbc\xa8\xce\x99|\xe1\xbe\x90|"
"\xe1\xbe\x99|\xe1\xbc\xa1\xce\xb9|\xe1\xbc\xa9\xce\x99|\xe1\xbe\x91|"
"\xe1\xbe\x9a|\xe1\xbc\xa2\xce\xb9|\xe1\xbc\xaa\xce\x99|\xe1\xbe\x92|"
"\xe1\xbe\x9b|\xe1\xbc\xa3\xce\xb9|\xe1\xbc\xab\xce\x99|\xe1\xbe\x93|"
"\xe1\xbe\x9c|\xe1\xbc\xa4\xce\xb9|\xe1\xbc\xac\xce\x99|\xe1\xbe\x94|"
"\xe1\xbe\x9d|\xe1\xbc\xa5\xce\xb9|\xe1\xbc\xad\xce\x99|\xe1\xbe\x95|"
"\xe1\xbe\x9e|\xe1\xbc\xa6\xce\xb9|\xe1\xbc\xae\xce\x99|\xe1\xbe\x96|"
"\xe1\xbe\x9f|\xe1\xbc\xa7\xce\xb9|\xe1\xbc\xaf\xce\x99|\xe1\xbe\x97|"
"\xe1\xbe\xa0|\xe1\xbd\xa0\xce\xb9|\xe1\xbd\xa8\xce\x99||"
"\xe1\xbe\xa1|\xe1\xbd\xa1\xce\xb9|\xe1\xbd\xa9\xce\x99||"
"\xe1\xbe\xa2|\xe1\xbd\xa2\xce\xb9|\xe1\xbd\xaa\xce\x99||"
"\xe1\xbe\xa3|\xe1\xbd\xa3\xce\xb9|\xe1\xbd\xab\xce\x99||"
"\xe1\xbe\xa4|\xe1\xbd\xa4\xce\xb9|\xe1\xbd\xac\xce\x99||"
"\xe1\xbe\xa5|\xe1\xbd\xa5\xce\xb9|\xe1\xbd\xad\xce\x99||"
"\xe1\xbe\xa6|\xe1\xbd\xa6\xce\xb9|\xe1\xbd\xae\xce\x99||"
"\xe1\xbe\xa7|\xe1\xbd\xa7\xce\xb9|\xe1\xbd\xaf\xce\x99||"
"\xe1\xbe\xa8|\xe1\xbd\xa0\xce\xb9|\xe1\xbd\xa8\xce\x99|\xe1\xbe\xa0|"
"\xe1\xbe\xa9|\xe1\xbd\xa1\xce\xb9|\xe1\xbd\xa9\xce\x99|\xe1\xbe\xa1|"
"\xe1\xbe\xaa|\xe1\xbd\xa2\xce\xb9|\xe1\xbd\xaa\xce\x99|\xe1\xbe\xa2|"
"\xe1\xbe\xab|\xe1\xbd\xa3\xce\xb9|\xe1\xbd\xab\xce\x99|\xe1\xbe\xa3|"
"\xe1\xbe\xac|\xe1\xbd\xa4\xce\xb9|\xe1\xbd\xac\xce\x99|\xe1\xbe\xa4|"
"\xe1\xbe\xad|\xe1\xbd\xa5\xce\xb9|\xe1\xbd\xad\xce\x99|\xe1\xbe\xa5|"
"\xe1\xbe\xae|\xe1\xbd\xa6\xce\xb9|\xe1\xbd\xae\xce\x99|\xe1\xbe\xa6|"
"\xe1\xbe\xaf|\xe1\xbd\xa7\xce\xb9|\xe1\xbd\xaf\xce\x99|\xe1\xbe\xa7|"
"\xe1\xbe\xb2|\xe1\xbd\xb0\xce\xb9|\xe1\xbe\xba\xce\x99||"
"\xe1\xbe\xb3|\xce\xb1\xce\xb9|\xce\x91\xce\x99||"
"\xe1\xbe\xb4|\xce\xac\xce\xb9|\xce\x86\xce\x99||"
"\xe1\xbe\xb6|\xce\xb1\xcd\x82|\xce\x91\xcd\x82||"
"\xe1\xbe\xb7|\xce\xb1\xcd\x82\xce\xb9|\xce\x91\xcd\x82\xce\x99||"
"\xe1\xbe\xbc|\xce\xb1\xce\xb9|\xce\x91\xce\x99|\xe1\xbe\xb3|"
"\xe1\xbe\xbe|\xce\xb9|\xce\x99||"
"\xe1\xbf\x82|\xe1\xbd\xb4\xce\xb9|\xe1\xbf\x8a\xce\x99||"
"\xe1\xbf\x83|\xce\xb7\xce\xb9|\xce\x97\xce\x99||"
"\xe1\xbf\x84|\xce\xae\xce\xb9|\xce\x89\xce\x99||"
"\xe1\xbf\x86|\xce\xb7\xcd\x82|\xce\x97\xcd\x82||"
"\xe1\xbf\x87|\xce\xb7\xcd\x82\xce\xb9|\xce\x97\xcd\x82\xce\x99||"
"\xe1\xbf\x8c|\xce\xb7\xce\xb9|\xce\x97\xce\x99|\xe1\xbf\x83|"
"\xe1\xbf\x92|\xce\xb9\xcc\x88\xcc\x80|\xce\x99\xcc\x88\xcc\x80||"
"\xe1\xbf\x93|\xce\xb9\xcc\x88\xcc\x81|\xce\x99\xcc\x88\xcc\x81||"
"\xe1\xbf\x96|\xce\xb9\xcd\x82|\xce\x99\xcd\x82||"
"\xe1\xbf\x97|\xce\xb9\xcc\x88\xcd\x82|\xce\x99\xcc\x88\xcd\x82||"
"\xe1\xbf\xa2|\xcf\x85\xcc\x88\xcc\x80|\xce\xa5\xcc\x88\xcc\x80||"
"\xe1\xbf\xa3|\xcf\x85\xcc\x88\xcc\x81|\xce\xa5\xcc\x88\xcc\x81||"
"\xe1\xbf\xa4|\xcf\x81\xcc\x93|\xce\xa1\xcc\x93||"
"\xe1\xbf\xa6|\xcf\x85\xcd\x82|\xce\xa5\xcd\x82||"
"\xe1\xbf\xa7|\xcf\x85\xcc\x88\xcd\x82|\xce\xa5\xcc\x88\xcd\x82||"
"\xe1\xbf\xb2|\xe1\xbd\xbc\xce\xb9|\xe1\xbf\xba\xce\x99||"
"\xe1\xbf\xb3|\xcf\x89\xce\xb9|\xce\xa9\xce\x99||"
"\xe1\xbf\xb4|\xcf\x8e\xce\xb9|\xce\x8f\xce\x99||"
"\xe1\xbf\xb6|\xcf\x89\xcd\x82|\xce\xa9\xcd\x82||"
"\xe1\xbf\xb7|\xcf\x89\xcd\x82\xce\xb9|\xce\xa9\xcd\x82\xce\x99||"
"\xe1\xbf\xbc|\xcf\x89\xce\xb9|\xce\xa9\xce\x99|\xe1\xbf\xb3|"
"\xe2\x84\xa6|\xcf\x89||\xcf\x89|"
"\xe2\x84\xaa|k||k|"
"\xe2\x84\xab|\xc3\xa5||\xc3\xa5|"
"\xea\xad\xb0|\xe1\x8e\xa0|\xe1\x8e\xa0||"
"\xea\xad\xb1|\xe1\x8e\xa1|\xe1\x8e\xa1||"
"\xea\xad\xb2|\xe1\x8e\xa2|\xe1\x8e\xa2||"
"\xea\xad\xb3|\xe1\x8e\xa3|\xe1\x8e\xa3||"
"\xea\xad\xb4|\xe1\x8e\xa4|\xe1\x8e\xa4||"
"\xea\xad\xb5|\xe1\x8e\xa5|\xe1\x8e\xa5||"
"\xea\xad\xb6|\xe1\x8e\xa6|\xe1\x8e\xa6||"
"\xea\xad\xb7|\xe1\x8e\xa7|\xe1\x8e\xa7||"
"\xea\xad\xb8|\xe1\x8e\xa8|\xe1\x8e\xa8||"
"\xea\xad\xb9|\xe1\x8e\xa9|\xe1\x8e\xa9||"
"\xea\xad\xba|\xe1\x8e\xaa|\xe1\x8e\xaa||"
"\xea\xad\xbb|\xe1\x8e\xab|\xe1\x8e\xab||"
"\xea\xad\xbc|\xe1\x8e\xac|\xe1\x8e\xac||"
"\xea\xad\xbd|\xe1\x8e\xad|\xe1\x8e\xad||"
"\xea\xad\xbe|\xe1\x8e\xae|\xe1\x8e\xae||"
"\xea\xad\xbf|\xe1\x8e\xaf|\xe1\x8e\xaf||"
"\xea\xae\x80|\xe1\x8e\xb0|\xe1\x8e\xb0||"
"\xea\xae\x81|\xe1\x8e\xb1|\xe1\x8e\xb1||"
"\xea\xae\x82|\xe1\x8e\xb2|\xe1\x8e\xb2||"
"\xea\xae\x83|\xe1\x8e\xb3|\xe1\x8e\xb3||"
"\xea\xae\x84|\xe1\x8e\xb4|\xe1\x8e\xb4||"
"\xea\xae\x85|\xe1\x8e\xb5|\xe1\x8e\xb5||"
"\xea\xae\x86|\xe1\x8e\xb6|\xe1\x8e\xb6||"
"\xea\xae\x87|\xe1\x8e\xb7|\xe1\x8e\xb7||"
"\xea\xae\x88|\xe1\x8e\xb8|\xe1\x8e\xb8||"
"\xea\xae\x89|\xe1\x8e\xb9|\xe1\x8e\xb9||"
"\xea\xae\x8a|\xe1\x8e\xba|\xe1\x8e\xba||"
"\xea\xae\x8b|\xe1\x8e\xbb|\xe1\x8e\xbb||"
"\xea\xae\x8c|\xe1\x8e\xbc|\xe1\x8e\xbc||"
"\xea\xae\x8d|\xe1\x8e\xbd|\xe1\x8e\xbd||"
"\xea\xae\x8e|\xe1\x8e\xbe|\xe1\x8e\xbe||"
"\xea\xae\x8f|\xe1\x8e\xbf|\xe1\x8e\xbf||"
"\xea\xae\x90|\xe1\x8f\x80|\xe1\x8f\x80||"
"\xea\xae\x91|\xe1\x8f\x81|\xe1\x8f\x81||"
"\xea\xae\x92|\xe1\x8f\x82|\xe1\x8f\x82||"
"\xea\xae\x93|\xe1\x8f\x83|\xe1\x8f\x83||"
"\xea\xae\x94|\xe1\x8f\x84|\xe1\x8f\x84||"
"\xea\xae\x95|\xe1\x8f\x85|\xe1\x8f\x85||"
"\xea\xae\x96|\xe1\x8f\x86|\xe1\x8f\x86||"
"\xea\xae\x97|\xe1\x8f\x87|\xe1\x8f\x87||"
"\xea\xae\x98|\xe1\x8f\x88|\xe1\x8f\x88||"
"\xea\xae\x99|\xe1\x8f\x89|\xe1\x8f\x89||"
"\xea\xae\x9a|\xe1\x8f\x8a|\xe1\x8f\x8a||"
"\xea\xae\x9b|\xe1\x8f\x8b|\xe1\x8f\x8b||"
"\xea\xae\x9c|\xe1\x8f\x8c|\xe1\x8f\x8c||"
"\xea\xae\x9d|\xe1\x8f\x8d|\xe1\x8f\x8d||"
"\xea\xae\x9e|\xe1\x8f\x8e|\xe1\x8f\x8e||"
"\xea\xae\x9f|\xe1\x8f\x8f|\xe1\x8f\x8f||"
"\xea\xae\xa0|\xe1\x8f\x90|\xe1\x8f\x90||"
"\xea\xae\xa1|\xe1\x8f\x91|\xe1\x8f\x91||"
"\xea\xae\xa2|\xe1\x8f\x92|\xe1\x8f\x92||"
"\xea\xae\xa3|\xe1\x8f\x93|\xe1\x8f\x93||"
"\xea\xae\xa4|\xe1\x8f\x94|\xe1\x8f\x94||"
"\xea\xae\xa5|\xe1\x8f\x95|\xe1\x8f\x95||"
"\xea\xae\xa6|\xe1\x8f\x96|\xe1\x8f\x96||"
"\xea\xae\xa7|\xe1\x8f\x97|\xe1\x8f\x97||"
"\xea\xae\xa8|\xe1\x8f\x98|\xe1\x8f\x98||"
"\xea\xae\xa9|\xe1\x8f\x99|\xe1\x8f\x99||"
"\xea\xae\xaa|\xe1\x8f\x9a|\xe1\x8f\x9a||"
"\xea\xae\xab|\xe1\x8f\x9b|\xe1\x8f\x9b||"
"\xea\xae\xac|\xe1\x8f\x9c|\xe1\x8f\x9c||"
"\xea\xae\xad|\xe1\x8f\x9d|\xe1\x8f\x9d||"
"\xea\xae\xae|\xe1\x8f\x9e|\xe1\x8f\x9e||"
"\xea\xae\xaf|\xe1\x8f\x9f|\xe1\x8f\x9f||"
"\xea\xae\xb0|\xe1\x8f\xa0|\xe1\x8f\xa0||"
"\xea\xae\xb1|\xe1\x8f\xa1|\xe1\x8f\xa1||"
"\xea\xae\xb2|\xe1\x8f\xa2|\xe1\x8f\xa2||"
"\xea\xae\xb3|\xe1\x8f\xa3|\xe1\x8f\xa3||"
"\xea\xae\xb4|\xe1\x8f\xa4|\xe1\x8f\xa4||"
"\xea\xae\xb5|\xe1\x8f\xa5|\xe1\x8f\xa5||"
"\xea\xae\xb6|\xe1\x8f\xa6|\xe1\x8f\xa6||"
"\xea\xae\xb7|\xe1\x8f\xa7|\xe1\x8f\xa7||"
"\xea\xae\xb8|\xe1\x8f\xa8|\xe1\x8f\xa8||"
"\xea\xae\xb9|\xe1\x8f\xa9|\xe1\x8f\xa9||"
"\xea\xae\xba|\xe1\x8f\xaa|\xe1\x8f\xaa||"
"\xea\xae\xbb|\xe1\x8f\xab|\xe1\x8f\xab||"
"\xea\xae\xbc|\xe1\x8f\xac|\xe1\x8f\xac||"
"\xea\xae\xbd|\xe1\x8f\xad|\xe1\x8f\xad||"
"\xea\xae\xbe|\xe1\x8f\xae|\xe1\x8f\xae||"
"\xea\xae\xbf|\xe1\x8f\xaf|\xe1\x8f\xaf||"
"\xef\xac\x80|ff|FF||"
"\xef\xac\x81|fi|FI||"
"\xef\xac\x82|fl|FL||"
"\xef\xac\x83|ffi|FFI||"
"\xef\xac\x84|ffl|FFL||"
"\xef\xac\x85|st|ST||"
"\xef\xac\x86|st|ST||"
"\xef\xac\x93|\xd5\xb4\xd5\xb6|\xd5\x84\xd5\x86||"
"\xef\xac\x94|\xd5\xb4\xd5\xa5|\xd5\x84\xd4\xb5||"
"\xef\xac\x95|\xd5\xb4\xd5\xab|\xd5\x84\xd4\xbb||"
"\xef\xac\x96|\xd5\xbe\xd5\xb6|\xd5\x8e\xd5\x86||"
"\xef\xac\x97|\xd5\xb4\xd5\xad|\xd5\x84\xd4\xbd||"
;

/* ── CaseConverter ───────────────────────────────────────────────────── */

#define MAX_CONVERSION_LENGTH 6

typedef struct {
    char conversion[MAX_CONVERSION_LENGTH + 1];
} ConversionString;

typedef struct {
    int              character;
    ConversionString conversion;
} CharacterConversion;

/* A CaseConverter builds a sorted parallel-array lookup for one CaseConversion
 * mode.  Initialised lazily on first use. */
typedef struct {
    /* Build phase — accumulated before FinishedAdding() */
    CharacterConversion *build;
    size_t               buildLen;
    size_t               buildCap;
    /* Search phase — parallel sorted arrays (set after FinishedAdding) */
    int             *characters;
    ConversionString *conversions;
    size_t            count;
    /* Also implements ICaseConverter vtable */
    ICaseConverter   iface;   /* must come last so cast CaseConverter*→ICaseConverter* works */
} CaseConverter;

/* vtable forward declaration */
static size_t CC_VtableCaseConvertString(ICaseConverter *self,
                                          char *converted, size_t sizeConverted,
                                          const char *mixed, size_t lenMixed);

static void CC_Init(CaseConverter *self) {
    self->build       = NULL;
    self->buildLen    = 0;
    self->buildCap    = 0;
    self->characters  = NULL;
    self->conversions = NULL;
    self->count       = 0;
    self->iface.CaseConvertString = CC_VtableCaseConvertString;
}

static int CC_Initialised(const CaseConverter *self) {
    return self->characters != NULL;
}

static void CC_Add(CaseConverter *self, int character,
                   const char *conversion, size_t convLen) {
    assert(convLen <= MAX_CONVERSION_LENGTH);
    if (self->buildLen == self->buildCap) {
        size_t cap = self->buildCap ? self->buildCap * 2 : 256;
        CharacterConversion *p = realloc(self->build,
                                          cap * sizeof(CharacterConversion));
        assert(p);
        self->build    = p;
        self->buildCap = cap;
    }
    CharacterConversion *cc = &self->build[self->buildLen++];
    cc->character = character;
    memset(cc->conversion.conversion, 0, sizeof(cc->conversion.conversion));
    memcpy(cc->conversion.conversion, conversion, convLen);
}

/* Binary search — replaces std::lower_bound */
static const char *CC_Find(const CaseConverter *self, int character) {
    if (!self->count) return NULL;
    size_t lo = 0, hi = self->count;
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (self->characters[mid] < character)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo < self->count && self->characters[lo] == character)
        return self->conversions[lo].conversion;
    return NULL;
}

/* qsort comparator for CharacterConversion */
static int CC_Compare(const void *a, const void *b) {
    const int ca = ((const CharacterConversion *)a)->character;
    const int cb = ((const CharacterConversion *)b)->character;
    return (ca > cb) - (ca < cb);
}

static void CC_FinishedAdding(CaseConverter *self) {
    qsort(self->build, self->buildLen, sizeof(CharacterConversion), CC_Compare);
    self->count       = self->buildLen;
    self->characters  = malloc(self->count * sizeof(int));
    self->conversions = malloc(self->count * sizeof(ConversionString));
    assert(self->characters && self->conversions);
    for (size_t i = 0; i < self->count; i++) {
        self->characters[i]  = self->build[i].character;
        self->conversions[i] = self->build[i].conversion;
    }
    free(self->build);
    self->build    = NULL;
    self->buildLen = self->buildCap = 0;
}

static void CC_AddSymmetric(CaseConverter *self, CaseConversion conv,
                             int lower, int upper) {
    const int character = (conv == CASE_UPPER) ? lower : upper;
    const int source    = (conv == CASE_UPPER) ? upper : lower;
    char converted[MAX_CONVERSION_LENGTH + 1] = {0};
    UTF8FromUTF32Character(source, converted);
    CC_Add(self, character, converted, strlen(converted));
}

/* Advance *ptr past the next '|' and return a pointer to the field start;
 * *fieldLen receives the field's byte length. */
static const char *NextField(const char **ptr, size_t *remaining,
                              size_t *fieldLen) {
    const char *start = *ptr;
    const char *sep   = memchr(start, '|', *remaining);
    if (!sep) {
        *fieldLen  = *remaining;
        *remaining = 0;
        *ptr       = start + *fieldLen;
    } else {
        *fieldLen  = (size_t)(sep - start);
        *remaining -= *fieldLen + 1;
        *ptr        = sep + 1;
    }
    return start;
}

static void CC_SetupConversions(CaseConverter *self, CaseConversion conv) {
    /* Symmetric ranges */
    const size_t nRanges = sizeof(symmetricCaseConversionRanges) /
                           sizeof(symmetricCaseConversionRanges[0]);
    for (size_t i = 0; i < nRanges;) {
        const int lower  = symmetricCaseConversionRanges[i++];
        const int upper  = symmetricCaseConversionRanges[i++];
        const int length = symmetricCaseConversionRanges[i++];
        const int pitch  = symmetricCaseConversionRanges[i++];
        for (int j = 0; j < length * pitch; j += pitch)
            CC_AddSymmetric(self, conv, lower + j, upper + j);
    }
    /* Symmetric singletons */
    const size_t nSingles = sizeof(symmetricCaseConversions) /
                            sizeof(symmetricCaseConversions[0]);
    for (size_t i = 0; i < nSingles;) {
        const int lower = symmetricCaseConversions[i++];
        const int upper = symmetricCaseConversions[i++];
        CC_AddSymmetric(self, conv, lower, upper);
    }
    /* Complex cases: pipe-delimited "original|folded|upper|lower|" records */
    const char *ptr       = complexCaseConversions;
    size_t      remaining = strlen(complexCaseConversions);
    while (remaining > 0) {
        size_t originLen, foldedLen, upperLen, lowerLen;
        const char *originUTF8 = NextField(&ptr, &remaining, &originLen);
        const char *foldedUTF8 = NextField(&ptr, &remaining, &foldedLen);
        const char *upperUTF8  = NextField(&ptr, &remaining, &upperLen);
        const char *lowerUTF8  = NextField(&ptr, &remaining, &lowerLen);

        const char *converted;
        size_t       convLen;
        switch (conv) {
        case CASE_FOLD:  converted = foldedUTF8; convLen = foldedLen; break;
        case CASE_UPPER: converted = upperUTF8;  convLen = upperLen;  break;
        default:         converted = lowerUTF8;  convLen = lowerLen;  break;
        }
        if (convLen > 0) {
            unsigned char buf[UTF8MaxBytes + 1];
            memcpy(buf, originUTF8,
                   originLen < UTF8MaxBytes ? originLen : UTF8MaxBytes);
            buf[originLen < UTF8MaxBytes ? originLen : UTF8MaxBytes] = 0;
            const int character = UnicodeFromUTF8(buf);
            CC_Add(self, character, converted, convLen);
        }
    }
    CC_FinishedAdding(self);
}

/* ── Global converter instances (lazily initialised) ─────────────────── */

static CaseConverter s_conv[3];
static int           s_conv_inited = 0;

static void EnsureInited(void) {
    if (!s_conv_inited) {
        CC_Init(&s_conv[0]);
        CC_Init(&s_conv[1]);
        CC_Init(&s_conv[2]);
        s_conv_inited = 1;
    }
}

static CaseConverter *ConverterForConversion(CaseConversion conv) {
    EnsureInited();
    const unsigned index = (unsigned)conv;
    assert(index < 3);
    CaseConverter *pcc = &s_conv[index];
    if (!CC_Initialised(pcc))
        CC_SetupConversions(pcc, conv);
    return pcc;
}

/* ── ICaseConverter vtable implementation ─────────────────────────────── */

static size_t CC_VtableCaseConvertString(ICaseConverter *self,
                                          char *converted, size_t sizeConverted,
                                          const char *mixed, size_t lenMixed) {
    /* self points to the iface field; recover the enclosing CaseConverter.
     * iface is the last field, so: CaseConverter = (char*)self - offsetof(...) */
    CaseConverter *cc = (CaseConverter *)((char *)self -
        offsetof(CaseConverter, iface));
    size_t lenConverted = 0;
    size_t mixedPos     = 0;
    unsigned char bytes[UTF8MaxBytes + 1];
    while (mixedPos < lenMixed) {
        const unsigned char leadByte = (unsigned char)mixed[mixedPos];
        const char *caseConverted    = NULL;
        size_t      lenMixedChar     = 1;
        if (UTF8IsAscii(leadByte)) {
            caseConverted = CC_Find(cc, leadByte);
        } else {
            bytes[0] = leadByte;
            const int widthCharBytes = UTF8BytesOfLead[leadByte];
            for (int b = 1; b < widthCharBytes; b++)
                bytes[b] = (mixedPos + (size_t)b < lenMixed)
                            ? (unsigned char)mixed[mixedPos + (size_t)b] : 0;
            const int classified = UTF8Classify(bytes, widthCharBytes);
            if (!(classified & UTF8MaskInvalid)) {
                lenMixedChar  = (size_t)(classified & UTF8MaskWidth);
                const int character = UnicodeFromUTF8(bytes);
                caseConverted = CC_Find(cc, character);
            }
        }
        if (caseConverted) {
            while (*caseConverted) {
                converted[lenConverted++] = *caseConverted++;
                if (lenConverted >= sizeConverted) return 0;
            }
        } else {
            for (size_t i = 0; i < lenMixedChar; i++) {
                converted[lenConverted++] = mixed[mixedPos + i];
                if (lenConverted >= sizeConverted) return 0;
            }
        }
        mixedPos += lenMixedChar;
    }
    return lenConverted;
}

/* ── Public API ──────────────────────────────────────────────────────── */

ICaseConverter *ConverterFor(CaseConversion conversion) {
    return &ConverterForConversion(conversion)->iface;
}

const char *CaseConvert(int character, CaseConversion conversion) {
    return CC_Find(ConverterForConversion(conversion), character);
}

size_t CaseConvertString(char *converted, size_t sizeConverted,
                          const char *mixed, size_t lenMixed,
                          CaseConversion conversion) {
    return CC_VtableCaseConvertString(ConverterFor(conversion),
                                      converted, sizeConverted, mixed, lenMixed);
}
